/*
**      include-tidy -- #include tidier
**      src/symbols.c
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
#include "symbols.h"
#include "clang_util.h"
#include "include-tidy.h"
#include "includes.h"
#include "options.h"
#include "red_black.h"
#include "tidy_util.h"
#include "util.h"

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////

/**
 * Additional data passed to symbol_visitor.
 */
struct symbol_visitor_data {
  CXFile source_file;                   ///< The file being tidied.
};
typedef struct symbol_visitor_data symbol_visitor_data;

// local functions
static void ts_cleanup( tidy_symbol* );

static rb_tree_t symbol_set;            ///< Set of symbols.

////////// local functions ////////////////////////////////////////////////////

/**
 * Helper function for symbol_visitor that gets whether the symbol at \a cursor
 * is referenced from \a file.
 *
 * @param cursor The cursor for the symbol.
 * @param file The file of interest.
 * @return Returns `true` only if the symbol is referenced from file.
 */
static bool is_symbol_in_file( CXCursor cursor, CXFile file ) {
  CXSourceLocation  sym_loc = clang_getCursorLocation( cursor );
  CXFile            sym_file;

  clang_getSpellingLocation( sym_loc, &sym_file,
                             /*line=*/NULL, /*column=*/NULL, /*offset=*/NULL );
  return sym_file != NULL && clang_File_isEqual( sym_file, file );
}

/**
 * Visits each symbol in a translation unit.
 *
 * @param cursor The cursor for the symbol in the AST being visited.
 * @param parent Not used.
 * @param client_data A pointer to a symbol_visitor_data.
 * @return Always returns `CXChildVisit_Recurse`.
 */
static enum CXChildVisitResult symbol_visitor( CXCursor cursor, CXCursor parent,
                                               CXClientData client_data ) {
  (void)parent;
  assert( client_data != NULL );
  symbol_visitor_data const *const svd =
    POINTER_CAST( symbol_visitor_data const*, client_data );

  if ( !is_symbol_in_file( cursor, svd->source_file ) )
    goto skip;

  switch ( clang_getCursorKind( cursor ) ) {
    case CXCursor_CallExpr:
    case CXCursor_DeclRefExpr:
    case CXCursor_MacroExpansion:;
      break;
    default:
      goto skip;
  } // switch

  // Gets the cursor for _a_ declaration of the symbol.
  CXCursor decl_cursor = clang_getCursorReferenced( cursor );
  if ( clang_isInvalid( decl_cursor.kind ) )
    goto skip;

  // Gets the cursor for the first time the symbol was seen.
  CXCursor          first_cursor = clang_getCanonicalCursor( decl_cursor );
  CXSourceLocation  first_loc = clang_getCursorLocation( first_cursor );
  CXFile            first_file;
  unsigned          first_line;

  clang_getSpellingLocation(
    first_loc, &first_file, &first_line, /*column=*/NULL, /*offset=*/NULL
  );
  if ( first_file == NULL )
    goto skip;

  // If the symbol was first seen in the file being tidied, we don't care.
  if ( clang_File_isEqual( first_file, svd->source_file ) )
    goto skip;

  // If the symbol was declared in a file directly included, that file is
  // needed.
  tidy_include *const include = include_find( first_file );
  if ( include != NULL && include->depth == 1 )
    include->is_needed = true;

  tidy_symbol sym = {
    .name = clang_getCursorSpelling( first_cursor ),
    .decl_file = first_file,
    .decl_line = first_line
  };

  rb_insert_rv_t const rv_rbi = rb_tree_insert( &symbol_set, &sym, sizeof sym );
  if ( !rv_rbi.inserted )
    ts_cleanup( &sym );

skip:
  return CXChildVisit_Recurse;
}

/**
 * Cleans-up all symbols.
 */
static void symbols_cleanup( void ) {
  rb_tree_cleanup( &symbol_set, POINTER_CAST( rb_free_fn_t, &ts_cleanup ) );
}

/**
 * Cleans-up a tidy_symbol.
 *
 * @param sym The tidy_symbol to clean up.  If NULL, does nothing.
 */
static void ts_cleanup( tidy_symbol *sym ) {
  if ( sym == NULL )
    return;
  clang_disposeString( sym->name );
}

/**
 * Compares two \ref tidy_include objects.
 *
 * @param i_sym The first symbol.
 * @param j_sym The second symbol.
 * @return Returns a number less than 0, 0, or greater than 0 if the name of \a
 * i_sym is less than, equal to, or greater than the name of \a j_sym,
 * respectively.
 */
NODISCARD
static int ts_cmp( tidy_symbol const *i_sym, tidy_symbol const *j_sym ) {
  assert( i_sym != NULL );
  assert( j_sym != NULL );

  char const *const i_sym_cstr = clang_getCString( i_sym->name );
  char const *const j_sym_cstr = clang_getCString( j_sym->name );

  return strcmp( i_sym_cstr, j_sym_cstr );
}

/**
 * Visits each symbol that needs a `#include` file in a translation unit.
 *
 * @param node_data The tidy_symbol.
 * @param visit_data Not used.
 * @return Always returns `false` (keep visiting).
 */
NODISCARD
static bool ts_visitor( void *node_data, void *visit_data ) {
  assert( node_data != NULL );

  tidy_symbol const *const sym = node_data;
  (void)visit_data;

  CXString file_str = tidy_File_getRealPathName( sym->decl_file );
  include_print( clang_getCString( file_str ), clang_getCString( sym->name ) );
  clang_disposeString( file_str );

  return false;
}

////////// extern functions ///////////////////////////////////////////////////

void symbols_init( CXTranslationUnit tu ) {
  ASSERT_RUN_ONCE();
  rb_tree_init( &symbol_set, RB_DINT, POINTER_CAST( rb_cmp_fn_t, &ts_cmp ) );
  ATEXIT( &symbols_cleanup );
  CXCursor cursor = clang_getTranslationUnitCursor( tu );
  symbol_visitor_data svd = {
    .source_file = clang_getFile( tu, tidy_source_path )
  };
  clang_visitChildren( cursor, &symbol_visitor, &svd );
}

void symbols_visit( void ) {
  rb_tree_visit( &symbol_set, &ts_visitor, /*visit_data=*/NULL );
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
