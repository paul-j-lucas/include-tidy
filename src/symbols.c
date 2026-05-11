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
#include <stddef.h>                     /* for unreachable(3) */
#include <string.h>

/// @endcond

/**
 * @addtogroup tidy-symbols-group
 * @{
 */

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct symbols_init_visitor_data  symbols_init_visitor_data;

////////// structures /////////////////////////////////////////////////////////

/**
 * Additional data passed to symbols_init_visitor.
 */
struct symbols_init_visitor_data {
  CXFile  source_file;                  ///< The file being tidied.
  bool    verbose_printed;              ///< Printed any verbose output?
};

////////// local functions ////////////////////////////////////////////////////

NODISCARD
static unsigned get_next_token_index( CXToken const[], unsigned, unsigned );

static void     tidy_symbol_cleanup( tidy_symbol* );
static void     visit_MacroDefinition( CXCursor, symbols_init_visitor_data* );


////////// local variables ////////////////////////////////////////////////////

static rb_tree_t symbol_set;            ///< Set of symbols.

////////// local functions ////////////////////////////////////////////////////

/**
 * Gets the index of the next token that is not a comment.
 *
 * @param tokens The array of tokens.
 * @param token_count The length of \a tokens.
 * @param token_idx The current token index.
 * @return Returns the index of the next non-comment token or an integer &ge;
 * \a token_count for none.
 */
NODISCARD
static unsigned get_next_token_index( CXToken const tokens[],
                                      unsigned token_count,
                                      unsigned token_idx ) {
  unsigned i;
  for ( i = token_idx + 1; i < token_count; ++i ) {
    if ( clang_getTokenKind( tokens[i] ) != CXToken_Comment )
      break;
  } // for
  return i;
}

/**
 * Gets the cursor for the identifier given by \a token within \a scope_cursor,
 * but only if \a token actually is an identifier, neither `__VA_ARGS__` nor
 * `__VA_OPT__`, nor one of the current macro's parameters.
 *
 * @param token The token to get the cursor for.
 * @param scope_cursor The cursor of the scope to search within.
 * @param param_set The set of macro parameter names.
 * @return Returns said cursor or the null cursor if \a token is:
 *  + Not an identifier; or:
 *  + Either `__VA_ARGS__` nor `__VA_OPT__`; or:
 *  + In \a param_set.
 */
NODISCARD
static CXCursor macro_get_cursor_by_name( CXToken token, CXCursor scope_cursor,
                                          rb_tree_t const *param_set ) {
  assert( param_set != NULL );

  CXCursor rv_cursor = clang_getNullCursor();

  if ( clang_getTokenKind( token ) != CXToken_Identifier )
    return rv_cursor;

  CXTranslationUnit const tu = clang_Cursor_getTranslationUnit( scope_cursor );
  CXString const          token_cxs = clang_getTokenSpelling( tu, token );
  char const *const       token_cstr = clang_getCString( token_cxs );

  if ( strcmp( token_cstr, "__VA_ARGS__" ) != 0 &&
       strcmp( token_cstr, "__VA_OPT__" ) != 0 &&
       rb_tree_find( param_set, token_cstr ) == NULL ) {
    rv_cursor = tidy_getCursorByName( token_cstr, scope_cursor );
  }

  clang_disposeString( token_cxs );
  return rv_cursor;
}

/**
 * Gets the names of all of a macro's parameters.
 *
 * @param tu The translation unit to use.
 * @param tokens The array of macro tokens.
 * @param token_count The length of \a tokens.
 * @param param_set The set to add the parameter names to.
 * @return Returns the index of the token one past the `)`.
 */
static unsigned macro_get_params( CXTranslationUnit tu, CXToken const tokens[],
                                  unsigned token_count, rb_tree_t *param_set ) {
  assert( param_set != NULL );

  unsigned rv_idx = 1;

  // Start at index 2 since tokens[0] is the macro name, tokens[1] is the '('.
  for ( unsigned i = 2; i < token_count; ++i ) {
    CXTokenKind const kind = clang_getTokenKind( tokens[i] );
    switch ( kind ) {
      case CXToken_Identifier:
      case CXToken_Punctuation:
        break;
      default:
        continue;
    } // switch

    CXString const    token_cxs = clang_getTokenSpelling( tu, tokens[i] );
    char const *const token_cstr = clang_getCString( token_cxs );

    switch ( kind ) {
      case CXToken_Identifier:
        PJL_DISCARD_RV(
          rb_tree_insert(
            param_set, CONST_CAST( char*, token_cstr ),
            strlen( token_cstr ) + 1/*\0*/
          )
        );
        break;
      case CXToken_Punctuation:
        if ( strcmp( token_cstr, ")" ) == 0 ) {
          rv_idx = i + 1;
          i = token_count;              // will cause loop to exit
        }
        break;
      default:
        unreachable();
    } // switch

    clang_disposeString( token_cxs );
  } // for

  return rv_idx;
}

/**
 * Gets the cursor for the fully qualified symbol from \a tokens.
 *
 * @param tu The translation to use.
 * @param tokens The array of macro tokens.
 * @param token_count The length of \a tokens.
 * @param ptoken_idx A pointer to the current index within \a tokens.
 * @param param_set The set of macro parameter names.
 * @return Returns said cursor or the null cursor for none.
 */
static CXCursor macro_get_symbol_cursor( CXTranslationUnit tu,
                                         CXToken const tokens[],
                                         unsigned token_count,
                                         unsigned *ptoken_idx,
                                         rb_tree_t const *param_set ) {
  assert( ptoken_idx != NULL );
  assert( param_set != NULL );

  CXCursor const tu_cursor = clang_getTranslationUnitCursor( tu );

  CXCursor rv_cursor =
    macro_get_cursor_by_name( tokens[ *ptoken_idx ], tu_cursor, param_set );

  CXCursor loop_cursor = rv_cursor;
  unsigned i = *ptoken_idx;

  while ( !clang_isInvalid( loop_cursor.kind ) ) {
    rv_cursor = loop_cursor;
    *ptoken_idx = i;

    i = get_next_token_index( tokens, token_count, *ptoken_idx );
    if ( i >= token_count )
      break;
    if ( clang_getTokenKind( tokens[i] ) != CXToken_Punctuation )
      break;                            // can't be "::"
    if ( !tidy_Token_isEqual( tu, tokens[i], "::" ) )
      break;
    i = get_next_token_index( tokens, token_count, i );
    if ( i >= token_count )
      break;
    loop_cursor = macro_get_cursor_by_name( tokens[i], rv_cursor, param_set );
  } // while

  return rv_cursor;
}

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

  char const *const sym_name = tidy_getCursorScopedName( sym_cursor );
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

  if ( !tidy_Cursor_isInFile( cursor, sivd->source_file ) )
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
      CXCursor const underlying_cursor = tidy_getCursorUnderlying( cursor );
      if ( clang_Cursor_isNull( underlying_cursor ) )
        break;
      if ( tidy_Cursor_isInFile( underlying_cursor, sivd->source_file ) )
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
 * @remarks
 * @parblock
 * We have to iterate over all tokens of the macro's definition looking for
 * identifiers to see whether the file being tidied includes the headers
 * defining those identifiers.  For example, if a header contains:
 *
 *      #define POINTER_CAST(T,EXPR)    ((T)(uintptr_t)(EXPR))
 *
 * it should also `#include <stdint.h>` because `uintptr_t` is used.  The user
 * of the macro shouldn't have to know or care about the definition, nor be
 * forced to `#include <stdint.h>` explicitly.
 * @endparblock
 *
 * @param macro_cursor The macro definition's cursor.
 * @param sivd The symbols_init_visitor_data to use.
 */
static void visit_MacroDefinition( CXCursor macro_cursor,
                                   symbols_init_visitor_data *sivd ) {
  CXSourceRange const     macro_range = clang_getCursorExtent( macro_cursor );
  CXTranslationUnit const tu = clang_Cursor_getTranslationUnit( macro_cursor );

  CXToken *tokens;
  unsigned token_count;
  clang_tokenize( tu, macro_range, &tokens, &token_count );

  //
  // While iterating over all tokens of the macro, we have to skip identifers
  // of macro parameters for function-like macros because those are obviously
  // defined by the macro itself.  To skip them, we first have to collect the
  // set of them.
  //
  rb_tree_t param_set;
  rb_tree_init( &param_set, RB_DINT, POINTER_CAST( rb_cmp_fn_t, &strcmp ) );

  unsigned i = clang_Cursor_isMacroFunctionLike( macro_cursor ) ?
    macro_get_params( tu, tokens, token_count, &param_set ) :
    1;                                  // tokens[0] = macro name; start at 1

  for ( ; i < token_count; ++i ) {
    CXCursor const sym_cursor =
      macro_get_symbol_cursor( tu, tokens, token_count, &i, &param_set );
    if ( !clang_isInvalid( sym_cursor.kind ) )
      maybe_add_symbol( sym_cursor, sivd );
  } // for

  rb_tree_cleanup( &param_set, /*free_fn=*/NULL );
  clang_disposeTokens( tu, tokens, token_count );
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
