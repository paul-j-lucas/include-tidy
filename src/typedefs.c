/*
**      include-tidy -- #include tidier
**      src/typedefs.c
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
 * Defines structures and functions for keeping track of symbols referenced.
 */

// local
#include "pjl_config.h"
#include "typedefs.h"
#include "clang_util.h"
#include "red_black.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <stdbool.h>
#include <string.h>

/// @endcond

/**
 * @addtogroup tidy-typedefs-group
 * @{
 */

////////// local variables ////////////////////////////////////////////////////

static rb_tree_t typedef_map;           ///< Map of typedefs.

////////// local functions ////////////////////////////////////////////////////

/**
 * Cleans-up a tidy_typedef.
 *
 * @param tdef The tidy_typedef to clean up.  If NULL, does nothing.
 */
static void tidy_typedef_cleanup( tidy_typedef *tdef ) {
  if ( tdef == NULL )
    return;
  FREE( tdef->alias_name );
}

/**
 * Cleans-up all symbols.
 */
static void typedefs_cleanup( void ) {
  rb_tree_cleanup(
    &typedef_map, POINTER_CAST( rb_free_fn_t, &tidy_typedef_cleanup )
  );
}

/**
 * Compares two tidy_typedef objects.
 *
 * @param i_tdef The first tidy_typedef.
 * @param j_tdef The second tidy_typedef.
 * @return Returns a number less than 0, 0, or greater than 0 if \a i_tdef is
 * less than, equal to, or greater than \a j_tdef, respectively.
 */
NODISCARD
static int tidy_typedef_cmp( tidy_typedef const *i_tdef,
                             tidy_typedef const *j_tdef ) {
  assert( i_tdef != NULL );
  assert( j_tdef != NULL );
  return tidy_Cursor_compare( i_tdef->type_csr, j_tdef->type_csr );
}

////////// extern functions ///////////////////////////////////////////////////

void typedef_add( CXCursor cursor ) {
  CXCursor const type_csr = tidy_Cursor_getCanonicalTypeDeclaration( cursor );
  if ( tidy_Cursor_isInvalid( type_csr ) )
    return;

  CXString const    alias_name_cxs = clang_getCursorSpelling( cursor );
  char const *const alias_name = clang_getCString( alias_name_cxs );
  CXString const    type_name_cxs = clang_getCursorSpelling( type_csr );
  char const *const type_name = clang_getCString( type_name_cxs );

  //
  // There can be declarations like:
  //
  //      using reverse_iterator = std::reverse_iterator<iterator>;
  //
  // i.e., the alias name is the same as the type name.  There's no point in
  // mapping these.
  //
  bool const is_same =
    (alias_name == NULL && type_name == NULL) ||
    (alias_name != NULL && type_name != NULL &&
     strcmp( alias_name, type_name ) == 0);

  clang_disposeString( alias_name_cxs );
  clang_disposeString( type_name_cxs );

  if ( is_same )
    return;

  tidy_typedef new_tdef = { .type_csr = cursor };
  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &typedef_map, &new_tdef, sizeof new_tdef );
  if ( rv_rbi.inserted ) {
    tidy_typedef *const tdef = RB_DINT( rv_rbi.node );
    tdef->alias_name = tidy_Cursor_getScopedSimpleName( cursor );
  }
}

tidy_typedef const* typedef_find( CXCursor cursor ) {
  tidy_typedef const find_tdef = { .type_csr = cursor };
  rb_node_t const *const found_rb = rb_tree_find( &typedef_map, &find_tdef );
  return found_rb != NULL ? RB_DINT( found_rb ) : NULL;
}

void typedefs_init( void ) {
  ASSERT_RUN_ONCE();
  rb_tree_init(
    &typedef_map, RB_DINT, POINTER_CAST( rb_cmp_fn_t, &tidy_typedef_cmp )
  );
  ATEXIT( &typedefs_cleanup );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
