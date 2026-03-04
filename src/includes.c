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
#include "include-tidy.h"
#include "red_black.h"
#include "util.h"

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////

/**
 * TODO
 */
struct include_file {
  CXFile    file;                       ///< File that was included.
  unsigned  count;                      ///< Number of times included.
  unsigned  depth;
};
typedef struct include_file include_file;

static rb_tree_t includes;

////////// local functions ////////////////////////////////////////////////////

static void include_file_cleanup( include_file *file ) {
  (void)file;
}

/**
 * Compares to \ref include_file objects.
 */
NODISCARD
static int include_file_cmp( include_file const *i_file,
                             include_file const *j_file ) {
  assert( i_file != NULL );
  assert( j_file != NULL );

  CXString i_string = clang_getFileName( i_file->file );
  CXString j_string = clang_getFileName( j_file->file );

  int const cmp =
    strcmp( clang_getCString( i_string ), clang_getCString( j_string ) );

  clang_disposeString( i_string );
  clang_disposeString( j_string );

  return cmp;
}

/**
 * TODO
 */
static void include_visitor( CXFile included_file,
                             CXSourceLocation *source_loc,
                             unsigned include_depth,
                             CXClientData client_data ) {
  (void)source_loc;
  (void)client_data;

  include_file file = {
    .file = included_file,
    .count = 1,
    .depth = include_depth
  };
  rb_insert_rv_t rv_rbi = rb_tree_insert( &includes, &file, sizeof file );
  if ( !rv_rbi.inserted ) {
    include_file *const old_file = RB_DINT( rv_rbi.node );
    ++old_file->count;
    if ( include_depth < old_file->depth )
      old_file->depth = include_depth;
  }
}

/**
 * TODO
 */
static void includes_cleanup( void ) {
  rb_tree_cleanup(
    &includes, POINTER_CAST( rb_free_fn_t, &include_file_cleanup )
  );
}

////////// extern functions ///////////////////////////////////////////////////

void includes_init( CXTranslationUnit tu ) {
  ASSERT_RUN_ONCE();
  rb_tree_init(
    &includes, RB_DINT, POINTER_CAST( rb_cmp_fn_t, &include_file_cmp )
  );
  ATEXIT( &includes_cleanup );
  clang_getInclusions( tu, &include_visitor, /*client_data=*/NULL );
}

/* vim:set et sw=2 ts=2: */
