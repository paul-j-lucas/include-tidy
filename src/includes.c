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

// local
#include "pjl_config.h"
#include "includes.h"
#include "clang_util.h"
#include "include-tidy.h"
#include "options.h"
#include "red_black.h"
#include "symbols.h"
#include "util.h"

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <limits.h>                     /* for PATH_MAX */
#include <stdlib.h>                     /* for atexit(3) */
#include <string.h>
#include <unistd.h>                     /* for getcwd(3) */

///////////////////////////////////////////////////////////////////////////////

/**
 * Additional data for include_getFile().
 */
struct include_getFile_data {
  char const *rel_path;
  size_t      rel_path_len;
};
typedef struct include_getFile_data include_getFile_data;

/**
 * Additional data passed to visitChildren_visitor().
 */
struct visitChildren_visitor_data {
  bool  verbose_printed;                ///< Printed any verbose output?
};
typedef struct visitChildren_visitor_data visitChildren_visitor_data;

/**
 * Additional data for includes_print_visitor().
 */
struct includes_print_visitor_data {
  bool  print_blank_line;               ///< Print a blank line?
  bool  print_local;                    ///< Print local includes?
  bool  printed_any_includes;           ///< Did we print any includes?
};
typedef struct includes_print_visitor_data includes_print_visitor_data;

/**
 * A file that was included.
 */
struct tidy_include {
  CXFile          file;                 ///< File that was included.
  CXFileUniqueID  file_id;              ///< Unique file ID.
  CXString        real_path_cxs;        ///< Real path of \a file.
  char const     *resolved_path;
  unsigned        count;                ///< Number of times included.
  unsigned        line;                 ///< Line included from.
  bool            is_direct;            ///< Directly included?
  bool            is_local;             ///< Local include file?
  bool            is_needed;            ///< Include needed?
  rb_tree_t       symbol_set;           ///< Symbols referenced from this file.
};
typedef struct tidy_include tidy_include;

/**
 * The symbols used from an include file.
 */
struct symbols_used {
  /// Sum of \ref opt_comment_align and `strlen(`\ref opt_comment_style
  /// "opt_comment_style"`[*])`.
  size_t  fixed_len;

  char   *symbols;                      ///< TODO.
  size_t  symbols_len;                  ///< Length of \ref symbols.
};
typedef struct symbols_used symbols_used;

// local functions
NODISCARD
#ifdef TIDY_MOVE_SYMBOLS
static bool     symbols_used_visitor( void*, void* ),
                tidy_symbol_move_visitor( void*, void* );
#else
static bool     symbols_used_visitor( void*, void* );
#endif

static void     tidy_include_cleanup( tidy_include* );

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
 * Visits each file that's included.
 *
 * @param node_data The tidy_include to visit.
 * @param visit_data The include_getFile_data to use.
 * @return Returns `true` if the header is found.
 */
static bool include_getFile_visitor( void *node_data, void *visit_data ) {
  assert( node_data != NULL );
  assert( visit_data != NULL );

  tidy_include const *const         include = node_data;
  include_getFile_data const *const igfd = visit_data;

  CXString          path_cxs  = clang_getFileName( include->file );
  char const *const path_cs   = clang_getCString( path_cxs );
  size_t const      path_len  = strlen( path_cs );
  bool              rv        = false;

  if ( igfd->rel_path_len <= path_len ) {
    char const *const suffix = path_cs + (path_len - igfd->rel_path_len);
    rv = strcmp( igfd->rel_path, suffix ) == 0 &&
         (suffix == path_cs || suffix[-1] == '/');
  }

  clang_disposeString( path_cxs );
  return rv;
}

/**
 * Prints a `#include` preprocessor directive.
 *
 * @param include The tidy_include to print.
 * @param comment The text of the comment (not including the delimiters).  May
 * be NULL.
 */
static void include_print( tidy_include const *include, char const *comment ) {
  assert( include != NULL );

  char inc_delim[2];
  get_include_delims( include->is_local, inc_delim );

  int const raw_len = printf(
    "#include %c%s%c", inc_delim[0], include->resolved_path, inc_delim[1]
  );
  if ( unlikely( raw_len < 0 ) )
    perror_exit( EX_IOERR );

  if ( comment != NULL ) {
    unsigned const column = STATIC_CAST( unsigned, raw_len ) + 1;
    if ( column < opt_comment_align )
      FPUTNSP( opt_comment_align - column, stdout );
    printf( "%s%s%s", opt_comment_style[0], comment, opt_comment_style[1] );
  }

  putchar( '\n' );
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

  char *comment = NULL;
  bool  reset_opt_comment_style = false;

  if ( include->is_needed ) {
    if ( !(!include->is_direct || opt_all_includes) )
      goto skip;
    if ( opt_comment_style[0][0] != '\0' ) {
      size_t const comment_delimis_len =
        strlen( opt_comment_style[0] ) + strlen( opt_comment_style[1] );
      symbols_used used = {
        .fixed_len = opt_comment_align + comment_delimis_len
      };
      rb_tree_visit( &include->symbol_set, &symbols_used_visitor, &used );
      comment = used.symbols;
    }
  }
  else if ( include->is_direct ) {
    if ( opt_comment_style[0][0] == '\0' ) {
      opt_comment_style[0] = "// ";
      reset_opt_comment_style = true;
    }
    check_asprintf( &comment, "DELETE line %u", include->line );
  }

  if ( true_clear( &ipvd->print_blank_line ) )
    putchar( '\n' );
  include_print( include, comment );
  free( comment );
  ipvd->printed_any_includes = true;
  if ( reset_opt_comment_style )
    opt_comment_style[0] = "";

skip:
  return false;
}

/**
 * Visits each include file and, if needed, inserts into a second set sorted by
 * name.
 *
 * @param node_data The tidy_include to visit.
 * @param visit_data The rb_tree_t to insert into.
 * @return Always returns `false`.
 */
NODISCARD
static bool includes_sort_by_name_visitor( void *node_data, void *visit_data ) {
  assert( node_data != NULL );
  assert( visit_data != NULL );

  tidy_include *const include = node_data;

  if ( ( include->is_needed && (!include->is_direct || opt_all_includes)) ||
       (!include->is_needed && include->is_direct) ) {
    rb_tree_t *const include_set_by_name = visit_data;
#ifdef TIDY_MOVE_SYMBOLS
    rb_insert_rv_t const rv_rbi =
      rb_tree_insert( include_set_by_name, include, sizeof *include );
    if ( !rv_rbi.inserted ) {
      tidy_include *const old_include = RB_DPTR( rv_rbi.node );
      rb_tree_visit(
        &include->symbol_set, &tidy_symbol_move_visitor,
        &old_include->symbol_set
      );
      old_include->is_needed = true;
      include->is_needed = false;
    }
#else
    PJL_DISCARD_RV(
      rb_tree_insert( include_set_by_name, include, sizeof *include )
    );
#endif
  }

  return false;
}

/**
 * Gets whether \a include_file is a local include file (as opposed to a system
 * include file).
 *
 * @param include_file The included file.
 * @return Returns `true` only if \a full_path is a local include file.
 */
static bool is_local_include( char const *include_file ) {
  static char   cwd_buf[ PATH_MAX ];
  static size_t cwd_len;

  if ( cwd_len == 0 ) {
    if ( getcwd( cwd_buf, sizeof cwd_buf ) == NULL ) {
      fatal_error( EX_UNAVAILABLE,
        "could not get current working directory: %s\n", STRERROR()
      );
    }
    cwd_len = strlen( cwd_buf );
    if ( cwd_len > 0 && cwd_buf[ cwd_len - 1 ] != '/' )
      strcpy( cwd_buf + cwd_len++, "/" );
  }

  return strncmp( include_file, cwd_buf, cwd_len ) == 0;
}

/**
 * Visits each symbol in an included file that is referenced from the file
 * being tidied.
 *
 * @param node_data The tidy_symbol to visit.
 * @param visit_data The symbols_used to use.
 * @return Returns `false` to keep visiting or `true` if the comment would be
 * too long.
 */
NODISCARD
static bool symbols_used_visitor( void *node_data, void *visit_data ) {
  assert( node_data != NULL );
  assert( visit_data != NULL );
  tidy_symbol const *const sym = node_data;
  symbols_used *const used = visit_data;

  char const   *name_cs   = clang_getCString( sym->name );
  size_t const  name_len  = strlen( name_cs );
  bool          stop      = false;

  if ( used->symbols_len == 0 ) {
    used->symbols = check_strdup( name_cs );
    used->symbols_len = name_len;
  }
  else {
    size_t new_len = STRLITLEN( ", " ) + name_len;
    size_t total_len = used->fixed_len + used->symbols_len + new_len;
    if ( total_len > opt_line_length ) {
      stop = true;
      new_len = STRLITLEN( ", ..." );
      total_len = used->fixed_len + used->symbols_len + new_len;
      if ( total_len > opt_line_length )
        goto done;
      name_cs = "...";
    }
    REALLOC( used->symbols, char, used->symbols_len + new_len + 1 );
    sprintf( used->symbols + used->symbols_len, ", %s", name_cs );
    used->symbols_len += new_len;
  }

done:
  return stop;
}

#ifdef TIDY_MOVE_SYMBOLS
/**
 * TODO.
 *
 * @param node_data TODO.
 * @param visit_data TODO.
 * @return Always returns `false`.
 */
NODISCARD
static bool tidy_symbol_move_visitor( void *node_data, void *visit_data ) {
  assert( node_data != NULL );
  assert( visit_data != NULL );

  tidy_symbol *const from_symbol    = node_data;
  rb_tree_t   *const to_symbol_set  = visit_data;

  PJL_DISCARD_RV(
    rb_tree_insert( to_symbol_set, from_symbol, sizeof *from_symbol )
  );

  return false;
}
#endif

/**
 * Cleans-up all memory associated with \a include but does _not_ free \a
 * include itself.
 *
 * @param include The tidy_include to clean up.  If NULL, does nothing.
 */
static void tidy_include_cleanup( tidy_include *include ) {
  if ( include == NULL )
    return;
  clang_disposeString( include->real_path_cxs );
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
  return memcmp(
    &i_include->file_id, &j_include->file_id, sizeof i_include->file_id
  );
}

/**
 * Compares two \ref tidy_include objects by their file names.
 *
 * @param i_include The first tidy_include.
 * @param j_include The second tidy_include.
 * @return Returns a number less than 0, 0, or greater than 0 if the file name
 * of \a i_include is less than, equal to, or greater than the file name of \a
 * j_include, respectively.
 */
NODISCARD
static int tidy_include_cmp_by_name( tidy_include const *i_include,
                                     tidy_include const *j_include ) {
  assert( i_include != NULL );
  assert( j_include != NULL );

  return strcmp( i_include->resolved_path, j_include->resolved_path );
}

/**
 * Attempts to find \a file among the set of files included.
 *
 * @param file The file to find.
 * @return Returns the corresponding tidy_include if found or NULL if not.
 */
NODISCARD
static tidy_include* tidy_include_find( CXFile file ) {
  tidy_include include = { .file = file };
  int const rv = clang_getFileUniqueID( file, &include.file_id );
  (void)rv;

  rb_node_t const *const found_rb = rb_tree_find( &include_set, &include );
  return found_rb != NULL ? RB_DINT( found_rb ) : NULL;
}

/**
 * Visits each `#include` directive in a translation unit.
 *
 * @param cursor The cursor for the symbol in the AST being visited.
 * @param parent Not used.
 * @param data Not used.
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

  CXSourceLocation  include_loc = clang_getCursorLocation( cursor );
  CXFile            included_file = clang_getIncludedFile( cursor );
  bool const        is_direct = clang_Location_isFromMainFile( include_loc );

  tidy_include new_include = { 0 };
  int const rv = clang_getFileUniqueID( included_file, &new_include.file_id );
  assert( rv == 0 );


  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &include_set, &new_include, sizeof new_include );
  tidy_include *const include = RB_DINT( rv_rbi.node );
  if ( !rv_rbi.inserted ) {
    if ( is_direct )
      include->is_direct = true;
    goto skip;
  }

  CXString real_path_cxs = tidy_File_getRealPathName( included_file );
  char const *const real_path_cs = clang_getCString( real_path_cxs );

  include->file           = included_file;
  include->is_direct      = is_direct;
  include->is_local       = is_local_include( real_path_cs );
  include->real_path_cxs  = real_path_cxs;
  include->resolved_path  = include_resolve( real_path_cs );

  clang_getSpellingLocation(
    include_loc, /*file=*/NULL, &include->line, /*column=*/NULL, /*offset=*/NULL
  );

  rb_tree_init(
    // Use RB_DPTR to make nodes point to existing tidy_symbol objects in
    // symbol_set in symbols.c
    &include->symbol_set, RB_DPTR,
    POINTER_CAST( rb_cmp_fn_t, &tidy_symbol_cmp )
  );

  if ( (opt_verbose & TIDY_VERBOSE_INCLUDES) != 0 ) {
    if ( !vcvd->verbose_printed ) {
      verbose_printf( "includes:\n" );
      vcvd->verbose_printed = true;
    }

    char inc_delim[2];
    get_include_delims( include->is_local, inc_delim );

    verbose_printf(
      "  %c %c%s%c\n",
      include->is_direct ? '*' : ' ', inc_delim[0], real_path_cs, inc_delim[1]
    );
  }

skip:
  return CXChildVisit_Continue;
}

////////// extern functions ///////////////////////////////////////////////////

bool include_add_symbol( CXFile include_file, tidy_symbol *sym ) {
  assert( include_file != NULL );
  assert( sym != NULL );

  tidy_include *const include = tidy_include_find( include_file );
  if ( include == NULL )
    return false;
  include->is_needed = true;
  PJL_DISCARD_RV( rb_tree_insert( &include->symbol_set, sym, sizeof *sym ) );
  return true;
}

CXFile include_getFile( char const *rel_path ) {
  assert( rel_path != NULL );

  include_getFile_data igfd = {
    .rel_path = rel_path,
    .rel_path_len = strlen( rel_path )
  };
  rb_node_t const *const found_rb =
    rb_tree_visit( &include_set, &include_getFile_visitor, &igfd );
  if ( found_rb == NULL )
    return NULL;
  tidy_include const *const include = RB_DINT( found_rb );
  return include->file;
}

void includes_init( CXTranslationUnit tu ) {
  ASSERT_RUN_ONCE();
  rb_tree_init(
    &include_set, RB_DINT, POINTER_CAST( rb_cmp_fn_t, &tidy_include_cmp_by_id )
  );
  ATEXIT( &includes_cleanup );

  visitChildren_visitor_data vcvd = { 0 };
  CXCursor cursor = clang_getTranslationUnitCursor( tu );
  clang_visitChildren( cursor, &visitChildren_visitor, &vcvd );
  if ( vcvd.verbose_printed )
    verbose_printf( "\n" );
}

void includes_print( void ) {
  rb_tree_t include_set_by_name;
  rb_tree_init(
    // Use RB_DPTR to make nodes point to existing tidy_include objects in
    // include_set.
    &include_set_by_name, RB_DPTR,
    POINTER_CAST( rb_cmp_fn_t, &tidy_include_cmp_by_name )
  );
  rb_tree_visit(
    &include_set, &includes_sort_by_name_visitor, &include_set_by_name
  );

  includes_print_visitor_data ipvd = { .print_local = true };
  rb_tree_visit( &include_set_by_name, &includes_print_visitor, &ipvd );

  ipvd.print_local = !ipvd.print_local;
  ipvd.print_blank_line = ipvd.printed_any_includes;
  rb_tree_visit( &include_set_by_name, &includes_print_visitor, &ipvd );

  // Because the nodes point to existing tidy_include objects, use NULL.
  rb_tree_cleanup( &include_set_by_name, /*free_fn=*/NULL );
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
