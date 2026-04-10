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
#include "red_black.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>                     /* for atexit() */
#include <string.h>

// libclang
#include <clang-c/Index.h>

/// @endcond

/**
 * @addtogroup tidy-symbols-group
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * Additional data passed to visitChildren_visitor.
 */
struct visitChildren_visitor_data {
  CXFile  source_file;                  ///< The file being tidied.
  bool    verbose_printed;              ///< Printed any verbose output?
};
typedef struct visitChildren_visitor_data visitChildren_visitor_data;

// local functions
static void tidy_symbol_cleanup( tidy_symbol* );
static void visit_MacroDefinition( CXCursor, visitChildren_visitor_data* );

static rb_tree_t symbol_set;            ///< Set of symbols.

////////// local functions ////////////////////////////////////////////////////

/**
 * Gets the "underlying" cursor for \a cursor, if any.
 *
 * @remarks
 * @parblock
 * For a case like:
 *
 *      typedef struct foo foo_t;
 *      // ...
 *      foo_t x;                        // cursor is at foo_t here
 *
 * where \a cursor is at a use of `foo_t`, we want to get the "underlying"
 * cursor, in this case for `foo`.
 * @endparblock
 *
 * @param cursor The original cursor to get the underlying cursor for.
 * @return Returns the underlying cursor for \a cursor or the null cursor if
 * none.
 */
NODISCARD
static CXCursor get_underlying_cursor( CXCursor cursor ) {
  CXCursor underlying_cursor = clang_getNullCursor();

  if ( clang_getCursorKind( cursor ) == CXCursor_TypeRef ) {
    CXType type = clang_getCanonicalType( clang_getCursorType( cursor ) );
    bool is_via_ptr_ref = true;
    do {
      switch ( type.kind ) {
        case CXType_Pointer:
        case CXType_LValueReference:
        case CXType_RValueReference:
          type = clang_getPointeeType( type );
          break;
        default:
          is_via_ptr_ref = false;
      } // switch
    } while ( is_via_ptr_ref );

    underlying_cursor = clang_getTypeDeclaration( type );
  }

  return underlying_cursor;
}

/**
 * Helper function for visitChildren_visitor that gets whether the symbol at \a
 * cursor is referenced from \a file.
 *
 * @param cursor The cursor for the symbol.
 * @param file The file of interest.
 * @return Returns `true` only if the symbol is referenced from \a file.
 */
NODISCARD
static bool is_symbol_in_file( CXCursor cursor, CXFile file ) {
  CXSourceLocation const  sym_loc = clang_getCursorLocation( cursor );
  CXFile                  sym_file;

  clang_getSpellingLocation( sym_loc, &sym_file,
                             /*line=*/NULL, /*column=*/NULL, /*offset=*/NULL );
  return sym_file != NULL && clang_File_isEqual( sym_file, file );
}

/**
 * Helper function for visitChildren_visitor that maybe adds a symbol to the
 * global set.
 *
 * @param sym_cursor The cursor at the symbol.
 * @param vcvd The visitChildren_visitor_data to use.
 */
static void maybe_add_symbol( CXCursor sym_cursor,
                              visitChildren_visitor_data *vcvd ) {
  assert( vcvd != NULL );

  CXSourceLocation const  sym_loc = clang_getCursorLocation( sym_cursor );
  CXFile                  sym_decl_file;

  clang_getSpellingLocation(
    sym_loc, &sym_decl_file, /*line=*/NULL, /*column=*/NULL, /*offset=*/NULL
  );
  if ( sym_decl_file == NULL )
    return;

  // If the symbol was first declared in the file being tidied, we don't care.
  if ( clang_File_isEqual( sym_decl_file, vcvd->source_file ) )
    return;

  tidy_symbol new_symbol = {
    .name_cxs = clang_getCursorSpelling( sym_cursor )
  };
  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &symbol_set, &new_symbol, sizeof new_symbol );
  if ( !rv_rbi.inserted )
    goto clean_up;

  tidy_symbol *const symbol      = RB_DINT( rv_rbi.node );
  char const  *const symbol_name = clang_getCString( symbol->name_cxs );

  CXFile include_file = config_get_symbol_include( symbol_name );
  if ( include_file == NULL )
    include_file = sym_decl_file;
  bool const added_symbol = include_add_symbol( include_file, symbol );

  if ( (opt_verbose & TIDY_VERBOSE_SYMBOLS) != 0 ) {
    if ( !vcvd->verbose_printed ) {
      verbose_printf( "symbols:\n" );
      vcvd->verbose_printed = true;
    }
    CXString const abs_path_cxs = tidy_File_getRealPathName( include_file );
    char const *const abs_path = clang_getCString( abs_path_cxs );
    verbose_printf(
      "  %s -> %s (%sadded)\n",
      symbol_name, abs_path, added_symbol ? "" : "NOT "
    );
    clang_disposeString( abs_path_cxs );
  }

  if ( added_symbol )
    return;

clean_up:
  tidy_symbol_cleanup( &new_symbol );
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
 * Cleans-up a tidy_symbol.
 *
 * @param sym The tidy_symbol to clean up.  If NULL, does nothing.
 */
static void tidy_symbol_cleanup( tidy_symbol *sym ) {
  if ( sym == NULL )
    return;
  clang_disposeString( sym->name_cxs );
}

/**
 * Visits each symbol in a translation unit.
 *
 * @param cursor The cursor for the symbol in the AST being visited.
 * @param parent Not used.
 * @param data A pointer to a visitChildren_visitor_data.
 * @return Always returns `CXChildVisit_Recurse`.
 */
static enum CXChildVisitResult visitChildren_visitor( CXCursor cursor,
                                                      CXCursor parent,
                                                      CXClientData data ) {
  (void)parent;
  assert( data != NULL );
  visitChildren_visitor_data *const vcvd =
    POINTER_CAST( visitChildren_visitor_data*, data );

  if ( !is_symbol_in_file( cursor, vcvd->source_file ) )
    goto skip;

  switch ( clang_getCursorKind( cursor ) ) {
    case CXCursor_CallExpr:
    case CXCursor_DeclRefExpr:
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

      maybe_add_symbol( first_cursor, vcvd );
      CXCursor const underlying_cursor = get_underlying_cursor( cursor );
      if ( !clang_Cursor_isNull( underlying_cursor ) )
        maybe_add_symbol( underlying_cursor, vcvd );
      break;

    case CXCursor_MacroDefinition:
      visit_MacroDefinition( cursor, vcvd );
      break;

    default:
      /* suppress warning */;
  } // switch

skip:
  return CXChildVisit_Recurse;
}

/**
 * Visits a `CXCursor_MacroDefinition` kind of cursor.
 *
 * @param macro_cursor The macro definition's cursor.
 * @param vcvd The visitChildren_visitor_data to use.
 */
static void visit_MacroDefinition( CXCursor macro_cursor,
                                   visitChildren_visitor_data *vcvd ) {
  CXTranslationUnit const tu = clang_Cursor_getTranslationUnit( macro_cursor );
  CXSourceRange const macro_range = clang_getCursorExtent( macro_cursor );

  CXToken *macro_tokens;
  unsigned token_count;
  clang_tokenize( tu, macro_range, &macro_tokens, &token_count );

  for ( unsigned i = 0; i < token_count; ++i ) {
    if ( clang_getTokenKind( macro_tokens[i] ) != CXToken_Identifier )
      continue;
    CXSourceLocation loc = clang_getTokenLocation( tu, macro_tokens[i] );
    CXCursor const ident_cursor = clang_getCursor( tu, loc );
    CXCursor const referenced = clang_getCursorReferenced( ident_cursor );
    if ( !clang_isInvalid( referenced.kind ) )
      maybe_add_symbol( referenced, vcvd );
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
  visitChildren_visitor_data vcvd = {
    .source_file = clang_getFile( tu, arg_source_path )
  };
  clang_visitChildren( cursor, &visitChildren_visitor, &vcvd );
  if ( vcvd.verbose_printed )
    verbose_printf( "\n" );
}

int tidy_symbol_cmp( tidy_symbol const *i_sym, tidy_symbol const *j_sym ) {
  assert( i_sym != NULL );
  assert( j_sym != NULL );

  char const *const i_sym_name = clang_getCString( i_sym->name_cxs );
  char const *const j_sym_name = clang_getCString( j_sym->name_cxs );
  return strcmp( i_sym_name, j_sym_name );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
