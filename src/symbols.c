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
#include "include-tidy.h"
#include "includes.h"
#include "red_black.h"
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

  CXSourceLocation  ref_loc = clang_getCursorLocation( cursor );
  CXFile            ref_file;

  clang_getSpellingLocation(
    ref_loc, &ref_file, /*line=*/NULL, /*column=*/NULL, /*offset=*/NULL
  );
  if ( ref_file == NULL )
    goto skip;
  if ( !clang_File_isEqual( ref_file, svd->source_file ) )
    goto skip;

  enum CXCursorKind const sym_kind = clang_getCursorKind( cursor );
  switch ( sym_kind ) {
    case CXCursor_CallExpr:
    case CXCursor_DeclRefExpr:
    case CXCursor_MacroExpansion:;

      // Follow the reference to the actual declaration
      CXCursor decl = clang_getCursorReferenced( cursor );
      if ( clang_isInvalid( decl.kind ) )
        break;

      CXSourceLocation  decl_loc = clang_getCursorLocation( decl );
      CXFile            decl_file;
      unsigned          decl_line;

      clang_getSpellingLocation(
        decl_loc, &decl_file, &decl_line, /*column=*/NULL, /*offset=*/NULL
      );
      if ( decl_file == NULL )
        break;

      // If the symbol is declared in the file being tidied, we don't care.
      if ( clang_File_isEqual( decl_file, svd->source_file ) )
        break;

      // If the symbol is declared in a file included, we don't care.
      tidy_include *const inc = include_find( decl_file );
      if ( inc != NULL && inc->depth == 1 ) {
        inc->is_needed = true;
        break;
      }

      tidy_symbol sym = {
        .name = clang_getCursorSpelling( decl ),
        .decl_file = decl_file,
        .decl_line = decl_line
      };

      rb_insert_rv_t const rv_rbi =
        rb_tree_insert( &symbol_set, &sym, sizeof sym );
      if ( !rv_rbi.inserted )
        ts_cleanup( &sym );
      break;

    default:
      break;
  } // switch

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
  (void)visit_data;

  char                      delims[] = { '<', '>' };
  tidy_symbol const *const  sym = node_data;
  CXString                  file_str = clang_getFileName( sym->decl_file );
  char const               *file_cstr = clang_getCString( file_str );

  if ( STRNCMPLIT( file_cstr, "./" ) == 0 ) {
    file_cstr += STRLITLEN( "./" );
    delims[0] = delims[1] = '"';
  }

  printf( "#include %c%s%c // %s\n",
    delims[0], file_cstr, delims[1],
    clang_getCString( sym->name )
  );

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
