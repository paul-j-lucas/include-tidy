/*
**      include-tidy -- #include tidier
**      src/includes.c
**
**      Copyright (C) 2026  Paul J. Lucas
**
**      This program is free software: you can redistribute it and/or modify
**      it under the terms of the GNU General Public License as published by
**      the Free Software Foundation, either version 3 of the License, or
**      (at your option) any later version.
**
**      This program is distributed in the hope that it will be useful,
**      but WITHOUT ANY WARRANTY; without even the implied warranty of
**      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**      GNU General Public License for more details.
**
**      You should have received a copy of the GNU General Public License
**      along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file
 * Defines functions for keeping track of files included.
 */

// local
#include "pjl_config.h"
#include "includes.h"
#include "clang_util.h"
#include "color.h"
#include "config_file.h"
#include "options.h"
#include "print.h"
#include "red_black.h"
#include "symbols.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <errno.h>
#include <limits.h>                     /* for PATH_MAX */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>                     /* for atexit(3) */
#include <string.h>
#include <sysexits.h>

// libclang
#include <clang-c/Index.h>

/// @endcond

/**
 * @addtogroup tidy-includes-group
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

#define VERBOSE_INCLUDE_INDENT    2     /**< Spaces per include depth. */

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct includes_print_visitor_data  includes_print_visitor_data;
typedef struct visitChildren_visitor_data   visitChildren_visitor_data;

////////// structures /////////////////////////////////////////////////////////

/**
 * Additional data for includes_print_visitor().
 */
struct includes_print_visitor_data {
  bool  print_blank_line;               ///< Print a blank line?
  bool  print_local;                    ///< Print local includes?
  bool  print_standard;                 ///< Print standard includes?
  bool  printed_any_includes;           ///< Did we print any includes?
  bool  printed_source_file;            ///< Printed source file name?
};

/**
 * Additional data passed to visitChildren_visitor().
 */
struct visitChildren_visitor_data {
  char const *source_file_no_ext;       ///< File being tidied without ext.
  bool        verbose_printed;          ///< Printed any verbose output?
};

////////// local functions ////////////////////////////////////////////////////

NODISCARD
static char*  make_symbols_used_comment( tidy_include const* );

static void   tidy_include_cleanup( tidy_include* );

////////// extern variables ///////////////////////////////////////////////////

unsigned tidy_includes_missing;
unsigned tidy_includes_unnecessary;

////////// local variables ////////////////////////////////////////////////////

static rb_tree_t include_set;           ///< Set of included files.

////////// local functions ////////////////////////////////////////////////////

/**
 * Gets the include delimiters, either local or system, to use.
 *
 * @param is_local `true` only if the include file is local.
 * @param delim The 2-element array to receive the delimiters.
 */
static void get_include_delims( bool is_local, char delim[static 2] ) {
  if ( is_local ) {
    delim[0] = '"';
    delim[1] = '"';
  }
  else {
    delim[0] = '<';
    delim[1] = '>';
  }
}

/**
 * Prints a `#include` preprocessor directive.
 *
 * @param include The tidy_include to print.
 */
static void include_print( tidy_include const *include ) {
  assert( include != NULL );

  char       *comment = NULL;
  char        inc_delim[2];
  bool const  is_direct = include->depth == 0;
  bool        reset_opt_comment_style = false;
  char const *sgr_color = NULL;

  if ( include->is_needed ) {
    if ( opt_comment_style[0][0] != '\0' )
      comment = make_symbols_used_comment( include );
    if ( !is_direct )
      sgr_color = sgr_include_add;
  }
  else if ( is_direct ) {
    if ( opt_comment_style[0][0] == '\0' ) {
      opt_comment_style[0] = "// ";
      reset_opt_comment_style = true;
    }
    check_asprintf( &comment, "DELETE LINE %u", include->line );
    sgr_color = sgr_include_del;
  }

  get_include_delims( include->is_local, inc_delim );
  color_start( stdout, sgr_color );
  int const raw_len = printf(
    "#include %c%s%c", inc_delim[0], include->rel_path, inc_delim[1]
  );
  if ( unlikely( raw_len < 0 ) ) {
    color_end( stdout, sgr_color );
    perror_exit( EX_IOERR );
  }

  if ( comment != NULL ) {
    unsigned const column = STATIC_CAST( unsigned, raw_len ) + 1;
    if ( column < opt_align_column )
      FPUTNSP( opt_align_column - column, stdout );
    printf( "%s%s%s", opt_comment_style[0], comment, opt_comment_style[1] );
  }

  color_end( stdout, sgr_color );
  putchar( '\n' );

  free( comment );
  if ( reset_opt_comment_style )
    opt_comment_style[0] = "";
}

/**
 * Cleans-up set of included files.
 */
static void includes_cleanup( void ) {
  rb_tree_cleanup(
    &include_set, POINTER_CAST( rb_free_fn_t, &tidy_include_cleanup )
  );
}

/**
 * Visits each include file that was included.
 *
 * @param node_data The tidy_include to visit.
 * @param visit_data Contains a `bool` specifying whether to print only local
 * includes.
 * @return Always returns `false` (keep visiting).
 */
NODISCARD
static bool includes_print_visitor( void *node_data, void *visit_data ) {
  assert( node_data != NULL );
  tidy_include const *const include = node_data;
  includes_print_visitor_data *const ipvd = visit_data;

  if ( ipvd->print_local != include->is_local )
    goto skip;
  if ( ipvd->print_standard != config_is_standard_include( include->rel_path ) )
    goto skip;

  if ( (opt_verbose & TIDY_VERBOSE_SOURCE_FILE) != 0 &&
       false_set( &ipvd->printed_source_file ) ) {
    verbose_printf( "%s\n", arg_source_path );
  }

  if ( true_clear( &ipvd->print_blank_line ) )
    putchar( '\n' );
  include_print( include );
  ipvd->printed_any_includes = true;

skip:
  return false;
}

/**
 * Gets whether \a includer should be an implicit proxy for \a include.
 *
 * @param includer The include file that includes \a include.
 * @param include An include file.
 * @return Returns `true` only if \a includer should be a proxy for \a include.
 */
NODISCARD
static bool is_implicit_proxy( tidy_include const *includer,
                               tidy_include const *include ) {
  assert( includer != NULL );
  assert( include != NULL );

  if ( includer->is_local || include->is_local )
    return false;

  if ( !config_is_standard_include( include->rel_path ) )
    return true;

  char const *const include_name = base_name( include->rel_path );
  char const *const includer_name = base_name( includer->rel_path );
  return strcmp( include_name, includer_name ) == 0;
}

/**
 * Gets whether \a abs_path refers to a local include file (as opposed to a
 * system include file).
 *
 * @param abs_path The absolute path of an include file.
 * @return Returns `true` only if \a abs_path refers to a local include file.
 */
static bool is_local_include( char const *abs_path ) {
  assert( abs_path != NULL );
  assert( abs_path[0] == '/' );

  size_t cwd_path_len;
  char const *const cwd_path = get_cwd( &cwd_path_len );
  return strncmp( abs_path, cwd_path, cwd_path_len ) == 0;
}

/**
 * For \a include, makes a comment containing a comma-separated list of the
 * symbols used.
 *
 * @param include The tidy_include to make the comment for.
 * @return Returns said comment.  The caller is responsible for freeing it.
 */
static char* make_symbols_used_comment( tidy_include const *include ) {
  assert( include != NULL );

  size_t const fixed_len = opt_align_column +
    strlen( opt_comment_style[0] ) + strlen( opt_comment_style[1] );

  bool            done = false;
  rb_iterator_t   iter;
  char           *symbols = NULL;
  size_t          symbols_len = 0;

  rb_iterator_init( &include->symbol_set, &iter );
  for ( tidy_symbol const *sym;
        !done && (sym = rb_iterator_next( &iter )) != NULL; ) {
    char const   *sym_name = clang_getCString( sym->name_cxs );
    size_t const  sym_name_len = strlen( sym_name );

    if ( symbols_len == 0 ) {
      symbols = check_strdup( sym_name );
      symbols_len = sym_name_len;
      continue;
    }

    size_t add_len = STRLITLEN( ", " ) + sym_name_len;
    size_t line_len = fixed_len + symbols_len + add_len;
    if ( line_len > opt_line_length ) {
      // The next symbol doesn't fit: try adding ", ..." instead.
      add_len = STRLITLEN( ", ..." );
      line_len = fixed_len + symbols_len + add_len;
      if ( line_len > opt_line_length ) // ", ..." didn't fit either
        break;
      sym_name = "...";
      done = true;
    }
    REALLOC( symbols, char, symbols_len + add_len + 1 );
    sprintf( symbols + symbols_len, ", %s", sym_name );
    symbols_len += add_len;
  } // for

  return symbols;
}

/**
 * Cleans-up all memory associated with \a include but does _not_ free \a
 * include itself.
 *
 * @param include The tidy_include to clean up.  If NULL, does nothing.
 */
static void tidy_include_cleanup( tidy_include *include ) {
  if ( include == NULL )
    return;

  clang_disposeString( include->abs_path_cxs );
  // rel_path points into abs_path_cxs, so it doesn't need to be freed.

  // Because the nodes point to existing tidy_symbol objects, use NULL.
  rb_tree_cleanup( &include->symbol_set, /*free_fn=*/NULL );
}

/**
 * Compares two \ref tidy_include objects by their unique file ID.
 *
 * @param i_include The first tidy_include.
 * @param j_include The second tidy_include.
 * @return Returns a number less than 0, 0, or greater than 0 if the file ID of
 * \a i_include is less than, equal to, or greater than the file ID of \a
 * j_include, respectively.
 */
NODISCARD
static int tidy_include_cmp_by_id( tidy_include const *i_include,
                                   tidy_include const *j_include ) {
  assert( i_include != NULL );
  assert( j_include != NULL );
  return tidy_CXFileUniqueID_cmp( &i_include->file_id, &j_include->file_id );
}

/**
 * Compares two \ref tidy_include objects by their relative paths.
 *
 * @param i_include The first tidy_include.
 * @param j_include The second tidy_include.
 * @return Returns a number less than 0, 0, or greater than 0 if the relative
 * path of \a i_include is less than, equal to, or greater than the relative
 * path of \a j_include, respectively.
 */
NODISCARD
static int tidy_include_cmp_by_rel_path( tidy_include const *i_include,
                                         tidy_include const *j_include ) {
  assert( i_include != NULL );
  assert( j_include != NULL );
  if ( i_include->sort_rank < j_include->sort_rank )
    return -1;
  if ( i_include->sort_rank > j_include->sort_rank )
    return 1;
  return strcmp( i_include->rel_path, j_include->rel_path );
}

/**
 * Visits each `#include` directive in a translation unit.
 *
 * @param cursor The cursor for the symbol in the AST being visited.
 * @param parent Not used.
 * @param data A pointer to a visitChildren_visitor_data.
 * @return Always returns `CXChildVisit_Continue`.
 */
static enum CXChildVisitResult visitChildren_visitor( CXCursor cursor,
                                                      CXCursor parent,
                                                      CXClientData data ) {
  (void)parent;
  assert( data != NULL );

  if ( clang_getCursorKind( cursor ) != CXCursor_InclusionDirective )
    goto skip;

  visitChildren_visitor_data *const vcvd =
    POINTER_CAST( visitChildren_visitor_data*, data );

  unsigned          include_line, include_col;
  CXSourceLocation  include_loc = clang_getCursorLocation( cursor );
  CXFile const      included_file = clang_getIncludedFile( cursor );
  CXFile            including_file;
  bool const        is_direct = clang_Location_isFromMainFile( include_loc );

  clang_getSpellingLocation(
    include_loc, &including_file, &include_line, &include_col,
    /*offset=*/NULL
  );

  if ( included_file == NULL ) {
    CXString const    included_name_cxs = clang_getCursorSpelling( cursor );
    char const *const included_name = clang_getCString( included_name_cxs );
    CXString const    including_name_cxs = clang_getFileName( including_file );
    char const *const including_name = clang_getCString( including_name_cxs );

    print_error(
      path_no_dot_slash( including_name ), include_line, include_col,
      "\"%s\": %s (missing -I option?)\n",
      included_name, strerror( ENOENT )
    );
    exit( EX_DATAERR );
  }

  tidy_include new_include = {
    .file_id = tidy_getFileUniqueID( included_file )
  };
  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &include_set, &new_include, sizeof new_include );

  if ( !rv_rbi.inserted && !is_direct )
    goto skip;

  tidy_include *const include = RB_DINT( rv_rbi.node );

  if ( rv_rbi.inserted ) {
    CXString const    abs_path_cxs = tidy_File_getRealPathName( included_file );
    char const *const abs_path = clang_getCString( abs_path_cxs );

    include->abs_path_cxs = abs_path_cxs;
    include->file         = included_file;
    include->is_local     = is_local_include( abs_path );
    include->rel_path     = opt_include_paths_relativize( abs_path );

    if ( !is_direct ) {
      tidy_include *const includer = include_find( including_file );
      assert( includer != NULL );
      include->depth = includer->depth + 1;
      if ( is_implicit_proxy( includer, include ) ) {
        tidy_include *proxy = includer;
        while ( proxy->proxy != NULL )
          proxy = proxy->proxy;
        include->proxy = proxy;
      }
    }

    char const *const include_ext = path_ext( include->rel_path );
    if ( include_ext != NULL && include_ext[0] == 'h' ) {
      char path_buf[ PATH_MAX ];
      char const *const include_no_ext =
        path_no_ext( include->rel_path, path_buf );
      if ( strcmp( include_no_ext, vcvd->source_file_no_ext ) == 0 ) {
        //
        // This include file's name matches the source file's (without
        // extensions), hence it's the .h corresponding to the .c so sort the
        // this include file first, e.g.:
        //
        //      // foo.c
        //      #include "foo.h"        // corresponding header sorted first
        //      #include "a.h"
        //      #include "b.h"
        //
        include->sort_rank = -1;
      }
    }

    rb_tree_init(
      // Use RB_DPTR to make nodes point to existing tidy_symbol objects in
      // symbol_set in symbols.c
      &include->symbol_set, RB_DPTR,
      POINTER_CAST( rb_cmp_fn_t, &tidy_symbol_cmp )
    );
  }
  else {                                // is_direct must be true here
    include->depth = 0;
    include->proxy = NULL;
  }

  if ( is_direct )
    include->line = include_line;

  if ( (opt_verbose & TIDY_VERBOSE_INCLUDES) != 0 ) {
    if ( false_set( &vcvd->verbose_printed ) )
      verbose_printf( "includes:\n" );

    char inc_delim[2];
    get_include_delims( include->is_local, inc_delim );

    verbose_printf(
      "  %2u%*s %c%s%c\n",
      include->depth,
      STATIC_CAST( int, include->depth * VERBOSE_INCLUDE_INDENT ), "",
      inc_delim[0], clang_getCString( include->abs_path_cxs ), inc_delim[1]
    );
  }

skip:
  return CXChildVisit_Continue;
}

////////// extern functions ///////////////////////////////////////////////////

void include_add_proxy( CXFile from_include_file, CXFile to_include_file ) {
  assert( from_include_file != NULL );
  assert( to_include_file != NULL );

  tidy_include *const from_include = include_find( from_include_file );
  if ( from_include == NULL )
    return;
  tidy_include *const to_include = include_find( to_include_file );
  if ( to_include == NULL )
    return;

  from_include->proxy = to_include;
  from_include->is_proxy_explicit = true;
}

bool include_add_symbol( CXFile include_file, tidy_symbol *sym ) {
  assert( include_file != NULL );
  assert( sym != NULL );

  tidy_include *include = include_find( include_file );
  if ( include == NULL )
    return false;
  while ( include->proxy != NULL )
    include = include->proxy;
  include->is_needed = true;
  PJL_DISCARD_RV( rb_tree_insert( &include->symbol_set, sym, sizeof *sym ) );
  return true;
}

tidy_include* include_find( CXFile file ) {
  assert( file != NULL );

  tidy_include find_include = {
    .file_id = tidy_getFileUniqueID( file )
  };
  rb_node_t const *const found_rb = rb_tree_find( &include_set, &find_include );
  return found_rb != NULL ? RB_DINT( found_rb ) : NULL;
}

CXFile include_get_File( char const *rel_path ) {
  assert( rel_path != NULL );

  bool          found = false;
  rb_iterator_t iter;
  size_t const  rel_path_len = strlen( rel_path );

  rb_iterator_init( &include_set, &iter );
  for ( tidy_include const *include;
        (include = rb_iterator_next( &iter )) != NULL; ) {
    char const *const abs_path      = clang_getCString( include->abs_path_cxs );
    size_t const      abs_path_len  = strlen( abs_path );

    if ( rel_path_len <= abs_path_len ) {
      char const *const suffix = abs_path + (abs_path_len - rel_path_len);
      found = strcmp( rel_path, suffix ) == 0 &&
              (suffix == abs_path || suffix[-1] == '/');
    }

    if ( found )
      return include->file;
  } // for

  return NULL;
}

void include_proxies_dump( void ) {
  if ( rb_tree_empty( &include_set ) )
    return;
  verbose_printf( "configuration proxies:\n" );
  rb_iterator_t iter;
  rb_iterator_init( &include_set, &iter );
  for ( tidy_include const *include;
        (include = rb_iterator_next( &iter )) != NULL; ) {
    if ( include->is_proxy_explicit ) {
      verbose_printf(
        "  \"%s\" -> \"%s\"\n",
        include->rel_path, include->proxy->rel_path
      );
    }
  } // for
  verbose_printf( "\n" );
}

void includes_init( CXTranslationUnit tu ) {
  ASSERT_RUN_ONCE();
  rb_tree_init(
    &include_set, RB_DINT, POINTER_CAST( rb_cmp_fn_t, &tidy_include_cmp_by_id )
  );
  ATEXIT( &includes_cleanup );

  char path_buf[ PATH_MAX ];
  visitChildren_visitor_data vcvd = {
    .source_file_no_ext = path_no_ext( base_name( arg_source_path ), path_buf )
  };
  CXCursor cursor = clang_getTranslationUnitCursor( tu );
  clang_visitChildren( cursor, &visitChildren_visitor, &vcvd );
  if ( vcvd.verbose_printed )
    verbose_printf( "\n" );
}

void includes_print( void ) {
  rb_tree_t include_set_by_rel_path;
  rb_tree_init(
    // Use RB_DPTR to make nodes point to existing tidy_include objects in
    // include_set.
    &include_set_by_rel_path, RB_DPTR,
    POINTER_CAST( rb_cmp_fn_t, &tidy_include_cmp_by_rel_path )
  );
  rb_iterator_t iter;
  rb_iterator_init( &include_set, &iter );
  for ( tidy_include *include;
        (include = rb_iterator_next( &iter )) != NULL; ) {
    bool const is_direct = include->depth == 0;
    if ( include->is_needed ? (!is_direct || opt_all_includes) : is_direct ) {
      if ( !include->is_needed )
        ++tidy_includes_unnecessary;
      else if ( !is_direct )
        ++tidy_includes_missing;
      PJL_DISCARD_RV( rb_tree_insert( &include_set_by_rel_path, include, 0 ) );
    }
  } // for

  includes_print_visitor_data ipvd = { .print_local = true };
  rb_tree_visit( &include_set_by_rel_path, &includes_print_visitor, &ipvd );

  ipvd.print_blank_line =
    opt_all_includes && true_clear( &ipvd.printed_any_includes );
  ipvd.print_local = !ipvd.print_local;
  rb_tree_visit( &include_set_by_rel_path, &includes_print_visitor, &ipvd );

  ipvd.print_blank_line =
    opt_all_includes && true_clear( &ipvd.printed_any_includes );
  ipvd.print_standard = true;
  rb_tree_visit( &include_set_by_rel_path, &includes_print_visitor, &ipvd );

  // Because the nodes point to existing tidy_include objects, use NULL.
  rb_tree_cleanup( &include_set_by_rel_path, /*free_fn=*/NULL );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
