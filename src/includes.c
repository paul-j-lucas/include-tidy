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
 * A file that was included.
 */
struct tidy_include {
  CXFile          file;                 ///< File that was included.
  CXFileUniqueID  file_id;              ///< Unique file ID.
  unsigned        count;                ///< Number of times included.
  unsigned        depth;                ///< "Depth" of include.
  unsigned        line;                 ///< Line included from.
  bool            is_needed;            ///< Is this include needed?
  rb_tree_t       symbol_set;           ///< Symbols referenced from this file.
};
typedef struct tidy_include tidy_include;

/**
 * TODO.
 */
struct symbols_declared {
  char   *symbols;                      ///< TODO.
  size_t  len;                          ///< TODO.
};
typedef struct symbols_declared symbols_declared;

// local functions
NODISCARD
static bool     ti_symbol_visitor( void*, void* );

NODISCARD
static CXString tidy_File_getRealPathName( CXFile );
static void     tidy_include_cleanup( tidy_include* );

static rb_tree_t include_set;           ///< Set of included files.

////////// local functions ////////////////////////////////////////////////////

/**
 * Gets whether \a full_path is a local include file (as opposed to a system
 * include file).
 *
 * @param full_path The full path of a file that's included.
 * @return Returns `true` only if \a full_path is a local include file.
 */
static bool is_include_local( char const *full_path ) {
  static char   cwd_buf[ PATH_MAX + 1 ];
  static size_t cwd_len;

  assert( full_path != NULL );

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

  return strncmp( full_path, cwd_buf, cwd_len ) == 0;
}

/**
 * Prints a `#include` preprocessor directive.
 *
 * @param include TODO.
 * @param comment The text of the comment (not including the delimiters).
 */
static void include_print( tidy_include const *include, char const *comment ) {
  assert( include != NULL );
  assert( comment != NULL );

  CXString          file_str  = tidy_File_getRealPathName( include->file );
  char const *const file_cstr = clang_getCString( file_str );

  char inc_delim[2];
  if ( is_include_local( file_cstr ) ) {
    inc_delim[0] = '"';
    inc_delim[1] = '"';
  }
  else {
    inc_delim[0] = '<';
    inc_delim[1] = '>';
  }

  char const *const resolved_path = include_resolve( file_cstr );

  if ( opt_comment_style[0] == NULL ) {
    PRINTF( "#include %c%s%c\n", inc_delim[0], resolved_path, inc_delim[1] );
    goto done;
  }

  char *include_directive = NULL;
  unsigned len = check_asprintf( &include_directive,
    "#include %c%s%c", inc_delim[0], resolved_path, inc_delim[1]
  );
  PUTS( include_directive );
  free( include_directive );
  if ( ++len < opt_comment_align )
    FPUTNSP( opt_comment_align - len, stdout );
  PRINTF( "%s%s%s\n", opt_comment_style[0], comment, opt_comment_style[1] );

done:
  clang_disposeString( file_str );
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
 * TODO
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
  (void)rv;

  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &include_set, &include, sizeof include );

  if ( rv_rbi.inserted ) {
    tidy_include *const new_include = RB_DINT( rv_rbi.node );
    if ( include_len == 1 ) {           // file was directly included
      clang_getSpellingLocation(
        inclusion_stack[0], /*file=*/NULL, &new_include->line, /*column=*/NULL,
        /*offset=*/NULL
      );
    }
    rb_tree_init(
      &new_include->symbol_set, RB_DPTR,
      POINTER_CAST( rb_cmp_fn_t, &tidy_symbol_cmp )
    );
  }
  else {
    tidy_include *const old_include = RB_DINT( rv_rbi.node );
    ++old_include->count;
    if ( include_len < old_include->depth )
      old_include->depth = include_len;
  }
}

/**
 * Visits each include file that was included.
 *
 * @param node_data The tidy_include to visit.
 * @param visit_data Not used.
 * @return Always returns `false` (keep visiting).
 */
NODISCARD
static bool includes_print_visitor( void *node_data, void *visit_data ) {
  assert( node_data != NULL );
  tidy_include const *const include = node_data;
  (void)visit_data;

  if ( include->is_needed ) {
    symbols_declared declared = { 0 };
    rb_tree_visit( &include->symbol_set, &ti_symbol_visitor, &declared );
    include_print( include, declared.symbols );
    free( declared.symbols );
  }
  else if ( include->depth == 1 ) {     // directly included, but not needed
    char *delete_line = NULL;
    check_asprintf( &delete_line, "DELETE line %u", include->line );
    include_print( include, delete_line );
    free( delete_line );
  }

  return false;
}

/**
 * Gets the real path of \a file.
 *
 * @param file The file to get the real path of.
 * @return Returns the string containing the real path of \a file.  The caller
 * _must_ call `clang_disposeString()` on it.
 *
 */
NODISCARD
static CXString tidy_File_getRealPathName( CXFile file ) {
  CXString    file_str  = clang_File_tryGetRealPathName( file );
  char const *file_cstr = clang_getCString( file_str );

  if ( file_cstr == NULL || file_cstr[0] == '\0' ) {
    clang_disposeString( file_str );
    file_str = clang_getFileName( file );
  }

  return file_str;
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
 * Compares two \ref tidy_include objects.
 *
 * @param i_include The first tidy_include.
 * @param j_include The second tidy_include.
 * @return Returns a number less than 0, 0, or greater than 0 if the file ID of
 * \a i_include is less than, equal to, or greater than the file ID of \a
 * j_include, respectively.
 */
NODISCARD
static int tidy_include_cmp( tidy_include const *i_include,
                             tidy_include const *j_include ) {
  assert( i_include != NULL );
  assert( j_include != NULL );
  return memcmp(
    &i_include->file_id, &j_include->file_id, sizeof i_include->file_id
  );
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
 * TODO.
 *
 * @param node_data The tidy_symbol to visit.
 * @param visit_data The symbols_declared to use.
 * @return Returns TODO.
 */
NODISCARD
static bool ti_symbol_visitor( void *node_data, void *visit_data ) {
  assert( node_data != NULL );
  assert( visit_data != NULL );
  tidy_symbol const *const sym = node_data;
  symbols_declared *const declared = visit_data;

  char const *const name_cstr = clang_getCString( sym->name );
  size_t const      name_len  = strlen( name_cstr );

  if ( declared->len == 0 ) {
    declared->symbols = check_strdup( name_cstr );
    declared->len = name_len;
  }
  else {
    size_t const add_len = 3 +3 +  STRLITLEN( ", " ) + name_len;
    if ( declared->len + add_len >= opt_line_length )
      return true;
    REALLOC( declared->symbols, char, declared->len + add_len + 1 );
    sprintf( declared->symbols + declared->len, ", %s", name_cstr );
    declared->len += add_len;
  }

  return false;
}

////////// extern functions ///////////////////////////////////////////////////

bool include_add_symbol( CXFile include_file, tidy_symbol *sym ) {
  assert( sym != NULL );
  tidy_include *const include = tidy_include_find( include_file );
  if ( include == NULL )
    return false;
  include->is_needed = true;
  rb_insert_rv_t const rv_rbi = rb_tree_insert( &include->symbol_set, sym, 0 );
  (void)rv_rbi;
  return true;
}

void includes_init( CXTranslationUnit tu ) {
  ASSERT_RUN_ONCE();
  rb_tree_init(
    &include_set, RB_DINT, POINTER_CAST( rb_cmp_fn_t, &tidy_include_cmp )
  );
  ATEXIT( &includes_cleanup );
  clang_getInclusions( tu, &includes_init_visitor, /*client_data=*/NULL );
}

void includes_print( void ) {
  bool reset_opt_comment_style = false;
  if ( opt_comment_style[0] == NULL ) {
    opt_comment_style[0] = "// ";
    opt_comment_style[1] = "";
    reset_opt_comment_style = true;
  }

  rb_tree_visit( &include_set, &includes_print_visitor, /*visit_data=*/NULL );

  if ( reset_opt_comment_style ) {
    opt_comment_style[0] = NULL;
    opt_comment_style[1] = NULL;
  }
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
