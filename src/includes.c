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
#include "red_black.h"
#include "util.h"

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <stdlib.h>                     /* for atexit(3) */
#include <string.h>

///////////////////////////////////////////////////////////////////////////////

// local functions
static void tidy_include_cleanup( tidy_include* );

static rb_tree_t include_set;           ///< Set of included files.

////////// local functions ////////////////////////////////////////////////////

/**
 * TODO
 *
 * @param included_file The file being includes.
 * @param inclusion_stack The stack of all files being included.
 * @param include_len The length of \a inclusion_stack.
 * @param client_data Not used.
 */
static void include_visitor( CXFile included_file,
                             CXSourceLocation *inclusion_stack,
                             unsigned include_len,
                             CXClientData client_data ) {
  (void)client_data;

  tidy_include include = {
    .file = included_file,
    .count = 1,
    .depth = include_len
  };

  if ( include_len == 1 ) {
    clang_getSpellingLocation(
      inclusion_stack[0], /*file=*/NULL, &include.line, /*column=*/NULL,
      /*offset=*/NULL
    );
  }

  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &include_set, &include, sizeof include );
  if ( !rv_rbi.inserted ) {
    tidy_include *const old_include = RB_DINT( rv_rbi.node );
    ++old_include->count;
    if ( include_len < old_include->depth )
      old_include->depth = include_len;
  }
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
 * TODO.
 */
static void tidy_include_cleanup( tidy_include *include ) {
  (void)include;
}

/**
 * Compares two \ref tidy_include objects.
 */
NODISCARD
static int tidy_include_cmp( tidy_include const *i_include,
                             tidy_include const *j_include ) {
  assert( i_include != NULL );
  assert( j_include != NULL );

  CXString i_string = clang_getFileName( i_include->file );
  CXString j_string = clang_getFileName( j_include->file );

  int const cmp =
    strcmp( clang_getCString( i_string ), clang_getCString( j_string ) );

  clang_disposeString( i_string );
  clang_disposeString( j_string );

  return cmp;
}

/**
 * Visits each include file that was includes.
 *
 * @param node_data The tidy_include.
 * @param visit_data Not used.
 * @return Always returns `false` (keep visiting).
 */
NODISCARD
static bool tidy_include_visitor( void *node_data, void *visit_data ) {
  assert( node_data != NULL );
  (void)visit_data;

  tidy_include const *const include = node_data;
  if ( !include->is_needed ) {
    char        delims[] = { '<', '>' };
    CXString    file_str = clang_getFileName( include->file );
    char const *file_cstr = clang_getCString( file_str );

    if ( STRNCMPLIT( file_cstr, "./" ) == 0 ) {
      file_cstr += STRLITLEN( "./" );
      delims[0] = delims[1] = '"';
    }

    printf( "#include %c%s%c // REMOVE\n",
      delims[0], file_cstr, delims[1]
    );

    clang_disposeString( file_str );
  }

  return false;
}

////////// extern functions ///////////////////////////////////////////////////

tidy_include* include_find( CXFile file ) {
  tidy_include const include = { .file = file };
  rb_node_t const *const found_rb = rb_tree_find( &include_set, &include );
  return found_rb != NULL ? RB_DINT( found_rb ) : NULL;
}

void includes_init( CXTranslationUnit tu ) {
  ASSERT_RUN_ONCE();
  rb_tree_init(
    &include_set, RB_DINT, POINTER_CAST( rb_cmp_fn_t, &tidy_include_cmp )
  );
  ATEXIT( &includes_cleanup );
  clang_getInclusions( tu, &include_visitor, /*client_data=*/NULL );
}

void includes_print_unneeded( void ) {
  rb_tree_visit( &include_set, &tidy_include_visitor, /*visit_data=*/NULL );
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
