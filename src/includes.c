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
static void ti_cleanup( tidy_include* );

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

  tidy_include inc = {
    .file = included_file,
    .count = 1,
    .depth = include_len
  };

  if ( include_len == 1 ) {             // file is directly included
    clang_getSpellingLocation(
      inclusion_stack[0], /*file=*/NULL, &inc.line, /*column=*/NULL,
      /*offset=*/NULL
    );
  }

  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &include_set, &inc, sizeof inc );
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
    &include_set, POINTER_CAST( rb_free_fn_t, &ti_cleanup )
  );
}

/**
 * TODO.
 */
static void ti_cleanup( tidy_include *inc ) {
  (void)inc;
}

/**
 * Compares two \ref tidy_include objects.
 *
 * @param i_inc The first tidy_include.
 * @param j_inc The second tidy_include.
 * @return Returns a number less than 0, 0, or greater than 0 if the filename
 * of \a i_inc is less than, equal to, or greater than the filename of \a
 * j_inc, respectively.

 */
NODISCARD
static int ti_cmp( tidy_include const *i_inc, tidy_include const *j_inc ) {
  assert( i_inc != NULL );
  assert( j_inc != NULL );

  CXString i_str = clang_getFileName( i_inc->file );
  CXString j_str = clang_getFileName( j_inc->file );

  char const *const i_cstr = clang_getCString( i_str );
  char const *const j_cstr = clang_getCString( j_str );

  int const cmp = strcmp( i_cstr, j_cstr );

  clang_disposeString( i_str );
  clang_disposeString( j_str );

  return cmp;
}

/**
 * Visits each include file that was included.
 *
 * @param node_data The tidy_include.
 * @param visit_data Not used.
 * @return Always returns `false` (keep visiting).
 */
NODISCARD
static bool ti_unneeded_visitor( void *node_data, void *visit_data ) {
  assert( node_data != NULL );
  (void)visit_data;

  tidy_include const *const inc = node_data;
  if ( !inc->is_needed && inc->depth == 1 ) {
    char        delims[] = { '<', '>' };
    CXString    file_str = clang_getFileName( inc->file );
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
  rb_tree_init( &include_set, RB_DINT, POINTER_CAST( rb_cmp_fn_t, &ti_cmp ) );
  ATEXIT( &includes_cleanup );
  clang_getInclusions( tu, &include_visitor, /*client_data=*/NULL );
}

void includes_print_unneeded( void ) {
  rb_tree_visit( &include_set, &ti_unneeded_visitor, /*visit_data=*/NULL );
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
