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

/**
 * @file
 * Defines structures and functions for keeping track of symbols referenced.
 */

// local
#include "pjl_config.h"
#include "symbols.h"
#include "clang_util.h"
#include "config_file.h"
#include "includes.h"
#include "options.h"
#include "print.h"
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
 * @addtogroup tidy-symbols-group
 * @{
 */

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct symbols_init_visitor_data symbols_init_visitor_data;

////////// structures /////////////////////////////////////////////////////////

/**
 * Additional data passed to symbols_init_visitor.
 */
struct symbols_init_visitor_data {
  CXFile  source_file;                  ///< The file being tidied.
  bool    verbose_printed;              ///< Printed any verbose output?
};

////////// local functions ////////////////////////////////////////////////////

static void tidy_symbol_cleanup( tidy_symbol* );
static void visit_MacroDefinition( CXCursor, symbols_init_visitor_data* );

////////// local variables ////////////////////////////////////////////////////

static rb_tree_t symbol_set;            ///< Set of symbols.

////////// local functions ////////////////////////////////////////////////////

/**
 * Helper function for symbols_init_visitor that maybe adds a symbol to the
 * global set.
 *
 * @param sym_cursor The cursor for the symbol.
 * @param sivd The symbols_init_visitor_data to use.
 */
static void maybe_add_symbol( CXCursor sym_cursor,
                              symbols_init_visitor_data *sivd ) {
  assert( sivd != NULL );

  CXFile const sym_file = tidy_getCursorLocation_File( sym_cursor );
  if ( sym_file == NULL )
    return;

  // If the symbol was first declared in the file being tidied, we don't care.
  if ( clang_File_isEqual( sym_file, sivd->source_file ) )
    return;

  enum CXCursorKind const   sym_kind = clang_getCursorKind( sym_cursor );
  char const               *sym_name;

  switch ( sym_kind ) {
    case CXCursor_ClassDecl:
    case CXCursor_CXXMethod:            // C++ member functions
    case CXCursor_EnumConstantDecl:
    case CXCursor_EnumDecl:
    case CXCursor_FieldDecl:
    case CXCursor_Namespace:
    case CXCursor_StructDecl:
    case CXCursor_UnionDecl:
    case CXCursor_VarDecl:              // static members of class or namespace
      sym_name = tidy_get_Cursor_scoped_name( sym_cursor );
      break;
    default:
      sym_name = tidy_getCursorSpelling( sym_cursor );
      break;
  } // switch

  tidy_symbol new_symbol = { .name = sym_name };
  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &symbol_set, &new_symbol, sizeof new_symbol );
  if ( !rv_rbi.inserted ) {
    tidy_symbol_cleanup( &new_symbol );
    return;
  }

  tidy_symbol *const  symbol = RB_DINT( rv_rbi.node );
  CXFile              include_file = config_get_symbol_include( symbol->name );

  if ( include_file == NULL )
    include_file = sym_file;
  tidy_include const *const include_added_to =
    include_add_symbol( include_file, symbol );

  if ( (opt_verbose & TIDY_VERBOSE_SYMBOLS) != 0 ) {
    if ( false_set( &sivd->verbose_printed ) )
      verbose_printf( "symbols:\n" );

    if ( include_added_to != NULL ) {
      verbose_printf(
        "  %s -> %s\n", symbol->name, include_added_to->abs_path
      );
    }
    else {
      CXString const abs_path_cxs = tidy_File_getRealPathName( include_file );
      char const *const abs_path = clang_getCString( abs_path_cxs );
      verbose_printf(
        "  %s -> %s (NOT added)\n", symbol->name, abs_path
      );
      clang_disposeString( abs_path_cxs );
    }
  }
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
 * Visits each symbol in a translation unit.
 *
 * @param cursor The cursor for the symbol in the AST being visited.
 * @param parent Not used.
 * @param data A pointer to a symbols_init_visitor_data.
 * @return Always returns `CXChildVisit_Recurse`.
 */
static enum CXChildVisitResult symbols_init_visitor( CXCursor cursor,
                                                     CXCursor parent,
                                                     CXClientData data ) {
  (void)parent;
  assert( data != NULL );
  symbols_init_visitor_data *const sivd =
    POINTER_CAST( symbols_init_visitor_data*, data );

  if ( !tidy_is_Cursor_in_File( cursor, sivd->source_file ) )
    goto skip;

  enum CXCursorKind const kind = clang_getCursorKind( cursor );
  switch ( kind ) {
    case CXCursor_CallExpr:
    case CXCursor_Constructor:
    case CXCursor_DeclRefExpr:
    case CXCursor_Destructor:
    case CXCursor_FunctionDecl:
    case CXCursor_MacroExpansion:
    case CXCursor_MemberRefExpr:
    case CXCursor_NamespaceRef:
    case CXCursor_TemplateRef:
    case CXCursor_TypeRef:;

      // Gets the cursor for _a_ declaration of the symbol.
      CXCursor const decl_cursor = clang_getCursorReferenced( cursor );
      if ( clang_isInvalid( decl_cursor.kind ) )
        break;

      // Gets the cursor for the _first_ declaration of the symbol.
      CXCursor const first_cursor = clang_getCanonicalCursor( decl_cursor );
      if ( clang_isInvalid( first_cursor.kind ) )
        break;

      maybe_add_symbol( first_cursor, sivd );
      CXCursor const underlying_cursor = tidy_get_Cursor_underlying( cursor );
      if ( clang_Cursor_isNull( underlying_cursor ) )
        break;
      if ( tidy_is_Cursor_in_File( underlying_cursor, sivd->source_file ) )
        maybe_add_symbol( underlying_cursor, sivd );
      break;

    case CXCursor_MacroDefinition:
      visit_MacroDefinition( cursor, sivd );
      break;

    default:
      /* suppress warning */;
  } // switch

skip:
  return CXChildVisit_Recurse;
}

/**
 * Cleans-up a tidy_symbol.
 *
 * @param sym The tidy_symbol to clean up.  If NULL, does nothing.
 */
static void tidy_symbol_cleanup( tidy_symbol *sym ) {
  if ( sym == NULL )
    return;
  FREE( sym->name );
}

/**
 * Visits a `CXCursor_MacroDefinition` kind of cursor.
 *
 * @param macro_cursor The macro definition's cursor.
 * @param sivd The symbols_init_visitor_data to use.
 */
static void visit_MacroDefinition( CXCursor macro_cursor,
                                   symbols_init_visitor_data *sivd ) {
  CXTranslationUnit const tu = clang_Cursor_getTranslationUnit( macro_cursor );
  CXSourceRange const macro_range = clang_getCursorExtent( macro_cursor );

  CXToken *macro_tokens;
  unsigned token_count;
  clang_tokenize( tu, macro_range, &macro_tokens, &token_count );

  for ( unsigned i = 0; i < token_count; ++i ) {
    if ( clang_getTokenKind( macro_tokens[i] ) != CXToken_Identifier )
      continue;
    CXSourceLocation const loc = clang_getTokenLocation( tu, macro_tokens[i] );
    CXCursor const ident_cursor = clang_getCursor( tu, loc );
    CXCursor const ref_cursor = clang_getCursorReferenced( ident_cursor );
    if ( !clang_isInvalid( ref_cursor.kind ) )
      maybe_add_symbol( ref_cursor, sivd );
  } // for

  clang_disposeTokens( tu, macro_tokens, token_count );
}

////////// extern functions ///////////////////////////////////////////////////

void symbols_init( CXTranslationUnit tu ) {
  ASSERT_RUN_ONCE();
  rb_tree_init(
    &symbol_set, RB_DINT, POINTER_CAST( rb_cmp_fn_t, &tidy_symbol_cmp )
  );
  ATEXIT( &symbols_cleanup );

  CXCursor const cursor = clang_getTranslationUnitCursor( tu );
  symbols_init_visitor_data sivd = {
    .source_file = clang_getFile( tu, arg_source_path )
  };
  clang_visitChildren( cursor, &symbols_init_visitor, &sivd );
  if ( sivd.verbose_printed )
    verbose_printf( "\n" );
}

int tidy_symbol_cmp( tidy_symbol const *i_sym, tidy_symbol const *j_sym ) {
  assert( i_sym != NULL );
  assert( j_sym != NULL );
  return strcmp( i_sym->name, j_sym->name );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
