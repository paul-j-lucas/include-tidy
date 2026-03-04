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
 * TODO
 */
struct symbol_visitor_data {
  CXFile source_file;                   ///< The file being tidied.
};
typedef struct symbol_visitor_data symbol_visitor_data;

// local functions
static void tidy_symbol_cleanup( tidy_symbol* );

static rb_tree_t symbol_set;            ///< Set of symbols.

////////// local functions ////////////////////////////////////////////////////

/**
 * Visit each symbol in a translation unit.
 *
 * @param parent Not used.
 * @param client_data TODO.
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
      tidy_include_file const *const inc_file = include_find( decl_file );
      if ( inc_file != NULL )
        break;

      tidy_symbol sym = {
        .name = clang_getCursorSpelling( decl ),
        .decl_file = decl_file,
        .decl_line = decl_line
      };

      rb_insert_rv_t const rv_rbi =
        rb_tree_insert( &symbol_set, &sym, sizeof sym );
      if ( !rv_rbi.inserted )
        tidy_symbol_cleanup( &sym );
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
  rb_tree_cleanup(
    &symbol_set, POINTER_CAST( rb_free_fn_t, &tidy_symbol_cleanup )
  );
}

/**
 */
static void tidy_symbol_cleanup( tidy_symbol *sym ) {
  if ( sym == NULL )
    return;
  clang_disposeString( sym->name );
}

/**
 * Compares two \ref tidy_include_file objects.
 */
NODISCARD
static int tidy_symbol_cmp( tidy_symbol const *i_sym,
                            tidy_symbol const *j_sym ) {
  assert( i_sym != NULL );
  assert( j_sym != NULL );

  return strcmp( clang_getCString( i_sym->name ),
                 clang_getCString( j_sym->name ) );
}

static bool tidy_symbol_visitor( void *node_data, void *visit_data ) {
  assert( node_data != NULL );
  (void)visit_data;

  tidy_symbol const *const sym = node_data;
  CXString decl_str = clang_getFileName( sym->decl_file );

  printf( "#include <%s> // %s\n",
    clang_getCString( sym->name ),
    clang_getCString( decl_str )
  );

  clang_disposeString( decl_str );
  return false;
}

////////// extern functions ///////////////////////////////////////////////////

void symbols_init( CXTranslationUnit tu ) {
  ASSERT_RUN_ONCE();
  rb_tree_init(
    &symbol_set, RB_DINT, POINTER_CAST( rb_cmp_fn_t, &tidy_symbol_cmp )
  );
  ATEXIT( &symbols_cleanup );
  CXCursor cursor = clang_getTranslationUnitCursor( tu );
  symbol_visitor_data svd = {
    .source_file = clang_getFile( tu, tidy_source_path )
  };
  clang_visitChildren( cursor, &symbol_visitor, &svd );
}

void symbols_visit( void ) {
  rb_tree_visit( &symbol_set, &tidy_symbol_visitor, /*visit_data=*/NULL );
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
