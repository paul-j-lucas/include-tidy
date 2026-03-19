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
 * Additional data for include_find().
 */
struct include_find_data {
  char const *header_name;
  size_t      header_len;
};
typedef struct include_find_data include_find_data;

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
  unsigned        count;                ///< Number of times included.
  unsigned        depth;                ///< "Depth" of include.
  unsigned        line;                 ///< Line included from.
  bool            is_local;             ///< Local include file?
  bool            is_needed;            ///< Is this include needed?
  rb_tree_t       symbol_set;           ///< Symbols referenced from this file.
};
typedef struct tidy_include tidy_include;

/**
 * TODO.
 */
struct symbols_declared {
  size_t  fixed_len;                    ///< TODO.
  char   *symbols;                      ///< TODO.
  size_t  symbols_len;                  ///< Length of \ref symbols.
};
typedef struct symbols_declared symbols_declared;

// local functions
NODISCARD
static bool     symbols_declared_visitor( void*, void* ),
#ifdef TIDY_MOVE_SYMBOLS
                tidy_File_isLocalInclude( CXFile ),
                tidy_symbol_move_visitor( void*, void* );
#else
                tidy_File_isLocalInclude( CXFile );
#endif

static void     tidy_include_cleanup( tidy_include* );

static rb_tree_t include_set;           ///< Set of included files.

////////// local functions ////////////////////////////////////////////////////

/**
 * Visits each include file TODO.
 *
 * @param node_data The tidy_include to visit.
 * @param visit_data The include_find_data to use.
 * @return Returns `true` if the header is found.
 */
static bool include_find_visitor( void *node_data, void *visit_data ) {
  assert( node_data != NULL );
  assert( visit_data != NULL );

  tidy_include const *const       include = node_data;
  include_find_data const *const  ifd = visit_data;

  CXString          path_str  = clang_getFileName( include->file );
  char const *const path_cstr = clang_getCString( path_str );
  size_t const      path_len  = strlen( path_cstr );
  bool              rv        = false;

  if ( ifd->header_len <= path_len ) {
    char const *const suffix = path_cstr + (path_len - ifd->header_len);
    rv = strcmp( ifd->header_name, suffix ) == 0 &&
         (suffix == path_cstr || suffix[-1] == '/');
  }

  clang_disposeString( path_str );
  return rv;
}

/**
 * Prints a `#include` preprocessor directive.
 *
 * @param include The tidy_include to print.
 * @param comment The text of the comment (not including the delimiters).
 */
static void include_print( tidy_include const *include, char const *comment ) {
  assert( include != NULL );
  assert( comment != NULL );

  char inc_delim[2];
  if ( include->is_local ) {
    inc_delim[0] = '"';
    inc_delim[1] = '"';
  }
  else {
    inc_delim[0] = '<';
    inc_delim[1] = '>';
  }

  CXString          file_str  = tidy_File_getRealPathName( include->file );
  char const *const file_cstr = clang_getCString( file_str );
  char const *const file_path = include_resolve( file_cstr );

  int const raw_len =
    printf( "#include %c%s%c", inc_delim[0], file_path, inc_delim[1] );
  assert( raw_len >= 0 );
  clang_disposeString( file_str );

  if ( opt_comment_style[0] != NULL ) {
    unsigned const len = STATIC_CAST( unsigned, raw_len ) + 1;
    if ( len < opt_comment_align )
      FPUTNSP( opt_comment_align - len, stdout );
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
 * Visits each file included.
 *
 * @param included_file The file being included.
 * @param inclusion_stack The stack of all files being included.
 * @param include_len The length of \a inclusion_stack.
 * @param data Not used.
 */
static void includes_init_visitor( CXFile included_file,
                                   CXSourceLocation inclusion_stack[],
                                   unsigned include_len, CXClientData data ) {
  (void)data;

  tidy_include include = {
    .file = included_file,
    .count = 1,
    .depth = include_len
  };
  int const rv = clang_getFileUniqueID( included_file, &include.file_id );
  assert( rv == 0 );

  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &include_set, &include, sizeof include );

  if ( rv_rbi.inserted ) {
    tidy_include *const new_include = RB_DINT( rv_rbi.node );
    if ( include_len == 1 ) {
      // We care about line numbers only for files that were directly included.
      clang_getSpellingLocation(
        inclusion_stack[0], /*file=*/NULL, &new_include->line, /*column=*/NULL,
        /*offset=*/NULL
      );
    }
    new_include->is_local = tidy_File_isLocalInclude( new_include->file );
    rb_tree_init(
      &new_include->symbol_set, RB_DPTR,
      POINTER_CAST( rb_cmp_fn_t, &tidy_symbol_cmp )
    );
  }
  else {
    tidy_include *const old_include = RB_DINT( rv_rbi.node );
    ++old_include->count;
  }
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

  if ( include->is_needed ) {
    if ( !(include->depth > 1 || opt_all_includes) )
      goto skip;
    size_t const comment_delimis_len =
      strlen( opt_comment_style[0] ) + strlen( opt_comment_style[1] );
    symbols_declared declared = {
      .fixed_len = opt_comment_align + comment_delimis_len
    };
    rb_tree_visit(
      &include->symbol_set, &symbols_declared_visitor, &declared
    );
    comment = declared.symbols;
  }
  else if ( include->depth == 1 ) {
    check_asprintf( &comment, "DELETE line %u", include->line );
  }

  if ( true_clear( &ipvd->print_blank_line ) )
    putchar( '\n' );
  include_print( include, comment );
  free( comment );
  ipvd->printed_any_includes = true;

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

  if ( ( include->is_needed && (include->depth > 1 || opt_all_includes)) ||
       (!include->is_needed && include->depth == 1) ) {
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
 * Visits each symbol in an included file that is referenced from the file
 * being tidied.
 *
 * @param node_data The tidy_symbol to visit.
 * @param visit_data The symbols_declared to use.
 * @return Returns `false` to keep visiting or `true` if the comment would be
 * too long.
 */
NODISCARD
static bool symbols_declared_visitor( void *node_data, void *visit_data ) {
  assert( node_data != NULL );
  assert( visit_data != NULL );
  tidy_symbol const *const sym = node_data;
  symbols_declared *const declared = visit_data;

  char const   *name_cstr = clang_getCString( sym->name );
  size_t const  name_len  = strlen( name_cstr );
  bool          stop      = false;

  if ( declared->symbols_len == 0 ) {
    declared->symbols = check_strdup( name_cstr );
    declared->symbols_len = name_len;
  }
  else {
    size_t new_len = STRLITLEN( ", " ) + name_len;
    size_t total_len = declared->fixed_len + declared->symbols_len + new_len;
    if ( total_len > opt_line_length ) {
      stop = true;
      new_len = STRLITLEN( ", ..." );
      total_len = declared->fixed_len + declared->symbols_len + new_len;
      if ( total_len > opt_line_length )
        goto done;
      name_cstr = "...";
    }
    REALLOC( declared->symbols, char, declared->symbols_len + new_len + 1 );
    sprintf( declared->symbols + declared->symbols_len, ", %s", name_cstr );
    declared->symbols_len += new_len;
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
 * Gets whether \a include_file is a local include file (as opposed to a system
 * include file).
 *
 * @param include_file The included file.
 * @return Returns `true` only if \a full_path is a local include file.
 */
static bool tidy_File_isLocalInclude( CXFile include_file ) {
  static char   cwd_buf[ PATH_MAX + 1 ];
  static size_t cwd_len;

  if ( cwd_len == 0 ) {
    if ( getcwd( cwd_buf, PATH_MAX ) == NULL ) {
      fatal_error( EX_UNAVAILABLE,
        "could not get current working directory: %s\n", STRERROR()
      );
    }
    cwd_len = strlen( cwd_buf );
    if ( cwd_len > 0 && cwd_buf[ cwd_len - 1 ] != '/' )
      strcpy( cwd_buf + cwd_len++, "/" );
  }

  CXString          file_str  = tidy_File_getRealPathName( include_file );
  char const *const file_cstr = clang_getCString( file_str );
  bool const        is_local  = strncmp( file_cstr, cwd_buf, cwd_len ) == 0;

  clang_disposeString( file_str );
  return is_local;
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

  CXString          i_file_str  = tidy_File_getRealPathName( i_include->file );
  char const *const i_file_cstr = clang_getCString( i_file_str );
  char const *const i_resolved_path = include_resolve( i_file_cstr );

  CXString          j_file_str  = tidy_File_getRealPathName( j_include->file );
  char const *const j_file_cstr = clang_getCString( j_file_str );
  char const *const j_resolved_path = include_resolve( j_file_cstr );

  int const cmp = strcmp( i_resolved_path, j_resolved_path );
  clang_disposeString( i_file_str );
  clang_disposeString( j_file_str );
  return cmp;
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

////////// extern functions ///////////////////////////////////////////////////

bool include_add_symbol( CXFile include_file, tidy_symbol *sym ) {
  assert( sym != NULL );
  tidy_include *const include = tidy_include_find( include_file );
  if ( include == NULL )
    return false;
  include->is_needed = true;
  PJL_DISCARD_RV( rb_tree_insert( &include->symbol_set, sym, 0 ) );
  return true;
}

CXFile include_find( char const *header_name ) {
  assert( header_name != NULL );

  include_find_data ifd = {
    .header_name = header_name,
    .header_len = strlen( header_name )
  };
  rb_node_t const *const found_rb = rb_tree_visit(
    &include_set, &include_find_visitor, &ifd
  );
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
  clang_getInclusions( tu, &includes_init_visitor, /*client_data=*/NULL );
}

void includes_print( void ) {
  bool reset_opt_comment_style = false;
  if ( opt_comment_style[0][0] == '\0' ) {
    opt_comment_style[0] = "// ";
    reset_opt_comment_style = true;
  }

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

  if ( reset_opt_comment_style )
    opt_comment_style[0] = "";
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
