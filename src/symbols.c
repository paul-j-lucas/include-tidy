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
#include "trans_unit.h"
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

////////// structs ////////////////////////////////////////////////////////////

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
static void     visit_FieldDecl( CXCursor, symbols_init_visitor_data* );
static void     visit_MacroDefinition( CXCursor, symbols_init_visitor_data* );
static void     visit_most_kinds( CXCursor, CXCursor,
                                  symbols_init_visitor_data* );

////////// local variables ////////////////////////////////////////////////////

static rb_tree_t symbol_set;            ///< Set of symbols.

////////// local functions ////////////////////////////////////////////////////

/**
 * Gets whether \a cursor is referenced from \a file.
 *
 * @param cursor The cursor to use.
 * @param file The file of interest.
 * @return Returns `true` only if the \a cursor is referenced from \a file.
 */
static bool cursor_in_file( CXCursor cursor, CXFile file ) {
  assert( file != NULL );

  CXFile const cursor_file = tidy_getCursorLocation_File( cursor );
  return cursor_file != NULL && clang_File_isEqual( cursor_file, file );
}

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

#ifdef NEED_II_MATRIX                   /* See comment above ii_matrix def. */
/**
 * Gets whether it's possible to go from a cursor that refernces a symbol to
 * the cursor that defines said symbol via the set of files that were included.
 *
 * @param ref_cursor A cursor referencing a symbol.
 * @param def_cursor A cursor defining a symbol.
 * @return Returns `true` only if it's possible.
 */
static bool is_include_path( CXCursor ref_cursor, CXCursor def_cursor ) {
  if ( tidy_Cursor_isInvalid( def_cursor ) )
    return false;
  CXFile const def_file = tidy_getCursorLocation_File( def_cursor );
  if ( def_file == NULL )
    return false;
  tidy_include const *const def_include = include_find_by_File( def_file );
  if ( def_include == NULL )
    return false;
  if ( includes_include( NULL, def_include ) > 0 )
    return true;

  if ( tidy_Cursor_isInvalid( ref_cursor ) )
    return false;
  CXFile const ref_file = tidy_getCursorLocation_File( ref_cursor );
  if ( ref_file == NULL )
    return false;
  tidy_include const *const ref_include = include_find_by_File( ref_file );
  if ( ref_include == NULL )
    return false;

  return includes_include( ref_include, def_include ) > 0;
}
#endif /* NEED_II_MATRIX */

/**
 * Gets the cursor for the identifier given by \a token within \a scope_cursor,
 * but only if \a token actually is an identifier, neither `__VA_ARGS__` nor
 * `__VA_OPT__`, nor one of the current macro's parameters.
 *
 * @param token The token to get the cursor for.
 * @param scope_cursor The cursor of the scope to search within.
 * @param param_set The set of macro parameter names.
 * @return Returns said cursor; or an invalid cursor if \a token is an
 * identifier, but not found; or the null cursor if \a token is:
 *  + Not an identifier; or:
 *  + Either `__VA_ARGS__` nor `__VA_OPT__`; or:
 *  + In \a param_set.
 */
NODISCARD
static CXCursor macro_get_cursor_by_name( CXToken token, CXCursor scope_cursor,
                                          rb_tree_t const *param_set ) {
  assert( param_set != NULL );

  if ( clang_getTokenKind( token ) != CXToken_Identifier )
    return clang_getNullCursor();

  CXString const    token_cxs = clang_getTokenSpelling( tidy_tu, token );
  char const *const token_cs = clang_getCString( token_cxs );

  CXCursor const rv_cursor =
    strcmp( token_cs, "__VA_ARGS__" ) != 0 &&
    strcmp( token_cs, "__VA_OPT__" ) != 0 &&
    rb_tree_find( param_set, token_cs ) == NULL ?
      tidy_getCursorByName( token_cs, scope_cursor )
    :
      clang_getNullCursor();

  clang_disposeString( token_cxs );
  return rv_cursor;
}

/**
 * Gets the names of all of a macro's parameters.
 *
 * @param tokens The array of macro tokens.
 * @param token_count The length of \a tokens.
 * @param param_set The set to add the parameter names to.
 * @return Returns the index of the token one past the `)`.
 */
static unsigned macro_get_params( CXToken const tokens[], unsigned token_count,
                                  rb_tree_t *param_set ) {
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

    CXString const    token_cxs = clang_getTokenSpelling( tidy_tu, tokens[i] );
    char const *const token_cs = clang_getCString( token_cxs );

    switch ( kind ) {
      case CXToken_Identifier:
        PJL_DISCARD_RV(
          rb_tree_insert(
            param_set, CONST_CAST( char*, token_cs ),
            strlen( token_cs ) + 1/*\0*/
          )
        );
        break;
      case CXToken_Punctuation:
        if ( strcmp( token_cs, ")" ) == 0 ) {
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
 * @param tokens The array of macro tokens.
 * @param token_count The length of \a tokens.
 * @param ptoken_idx A pointer to the current index within \a tokens.
 * @param param_set The set of macro parameter names.
 * @return Returns said cursor or the null cursor for none.
 */
static CXCursor macro_get_symbol_cursor( CXToken const tokens[],
                                         unsigned token_count,
                                         unsigned *ptoken_idx,
                                         rb_tree_t const *param_set ) {
  assert( ptoken_idx != NULL );
  assert( param_set != NULL );

  CXCursor const tu_cursor = clang_getTranslationUnitCursor( tidy_tu );

  CXCursor rv_cursor =
    macro_get_cursor_by_name( tokens[ *ptoken_idx ], tu_cursor, param_set );

  CXCursor loop_cursor = rv_cursor;
  unsigned i = *ptoken_idx;

  while ( !tidy_Cursor_isInvalid( loop_cursor ) ) {
    rv_cursor = loop_cursor;
    *ptoken_idx = i;

    i = get_next_token_index( tokens, token_count, *ptoken_idx );
    if ( i >= token_count )
      break;
    if ( clang_getTokenKind( tokens[i] ) != CXToken_Punctuation )
      break;                            // can't be "::"
    if ( !tidy_Token_isEqual( tidy_tu, tokens[i], "::" ) )
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

  tidy_symbol new_sym = {
    .name = tidy_getCursorScopedName( sym_cursor )
  };
  if ( config_ignore_symbol( new_sym.name ) )
    goto skip;

  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &symbol_set, &new_sym, sizeof new_sym );
  tidy_symbol *const sym = RB_DINT( rv_rbi.node );
  ++sym->ref_count;
  if ( !rv_rbi.inserted )
    goto skip;

  CXFile include_file = config_get_symbol_include( sym->name );
  if ( include_file == NULL )
    include_file = sym_file;
  tidy_include const *const include_added_to =
    include_add_symbol( include_file, sym );

  if ( (opt_verbose & TIDY_VERBOSE_SYMBOLS) != 0 ) {
    if ( false_set( &sivd->verbose_printed ) )
      verbose_printf( "symbols:\n" );

    if ( include_added_to != NULL ) {
      char delims[2];
      include_get_delims( include_added_to, delims );
      verbose_printf(
        "  \"%s\" -> %c%s%c\n",
        sym->name, delims[0], include_added_to->abs_path, delims[1]
      );
    }
    else {
      CXString const abs_path_cxs = tidy_File_getRealPathName( include_file );
      char const *const abs_path = clang_getCString( abs_path_cxs );
      verbose_printf(
        "  \"%s\" -> \"%s\" (NOT added)\n", sym->name, abs_path
      );
      clang_disposeString( abs_path_cxs );
    }
  }

  return;

skip:
  tidy_symbol_cleanup( &new_sym );
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

  if ( cursor_in_file( cursor, sivd->source_file ) ) {
    enum CXCursorKind const kind = clang_getCursorKind( cursor );
    switch ( kind ) {
      case CXCursor_CallExpr:
      case CXCursor_Constructor:
      case CXCursor_DeclRefExpr:
      case CXCursor_Destructor:
      case CXCursor_FunctionDecl:
      case CXCursor_MacroExpansion:
      case CXCursor_MemberRefExpr:
      case CXCursor_TemplateRef:
      case CXCursor_TypedefDecl:
      case CXCursor_TypeRef:
        visit_most_kinds( cursor, parent, sivd );
        break;

      case CXCursor_FieldDecl:
        visit_FieldDecl( cursor, sivd );
        break;

      case CXCursor_MacroDefinition:
        visit_MacroDefinition( cursor, sivd );
        break;

      default:
        /* suppress warning */;
    } // switch
  }

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
 * Visits a `CXCursor_FieldDecl` kind of cursor
 *
 * @param field_cursor The attribute definition's cursor to visit.
 * @param sivd The symbols_init_visitor_data to use.
 */
static void visit_FieldDecl( CXCursor field_cursor,
                             symbols_init_visitor_data *sivd ) {
  CXSourceRange const range = tidy_getCursorExtent( field_cursor );

  CXToken *tokens;
  unsigned token_count;
  clang_tokenize( tidy_tu, range, &tokens, &token_count );

  for ( unsigned i = 0; i < token_count; ++i ) {
    CXCursor const rv_cursor = tidy_getCursorByToken( tokens[i], field_cursor );
    if ( !tidy_Cursor_isInvalid( rv_cursor ) )
      maybe_add_symbol( rv_cursor, sivd );
  } // for

  clang_disposeTokens( tidy_tu, tokens, token_count );
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
 * @param macro_cursor The macro definition's cursor to visit.
 * @param sivd The symbols_init_visitor_data to use.
 */
static void visit_MacroDefinition( CXCursor macro_cursor,
                                   symbols_init_visitor_data *sivd ) {
  CXSourceRange const range = clang_getCursorExtent( macro_cursor );

  CXToken *tokens;
  unsigned token_count;
  clang_tokenize( tidy_tu, range, &tokens, &token_count );

  //
  // While iterating over all tokens of the macro, we have to skip identifers
  // of macro parameters for function-like macros because those are obviously
  // defined by the macro itself.  To skip them, we first have to collect the
  // set of them.
  //
  rb_tree_t param_set;
  rb_tree_init( &param_set, RB_DINT, POINTER_CAST( rb_cmp_fn_t, &strcmp ) );

  unsigned i = clang_Cursor_isMacroFunctionLike( macro_cursor ) ?
    macro_get_params( tokens, token_count, &param_set ) :
    1;                                  // tokens[0] = macro name; start at 1

  for ( ; i < token_count; ++i ) {
    CXCursor const sym_cursor =
      macro_get_symbol_cursor( tokens, token_count, &i, &param_set );
    if ( !tidy_Cursor_isInvalid( sym_cursor ) )
      maybe_add_symbol( sym_cursor, sivd );
  } // for

  rb_tree_cleanup( &param_set, /*free_fn=*/NULL );
  clang_disposeTokens( tidy_tu, tokens, token_count );
}

/**
 * Visit most kinds of cursor.
 *
 * @param cursor The cursor to visit.
 * @param parent The parent cursor of \a cursor.
 * @param sivd The symbols_init_visitor_data to use.
 */
static void visit_most_kinds( CXCursor cursor, CXCursor parent,
                              symbols_init_visitor_data *sivd ) {
  assert( sivd != NULL );

  // Gets the cursor for _a_ declaration of the symbol.
  CXCursor dec_cursor = clang_getCursorReferenced( cursor );
  if ( tidy_Cursor_isInvalid( dec_cursor ) )
    return;

  // Gets the cursor for the _the_ declaration of the symbol.
  dec_cursor = clang_getCanonicalCursor( dec_cursor );
  if ( tidy_Cursor_isInvalid( dec_cursor ) )
    return;

  maybe_add_symbol( dec_cursor, sivd );

  // Now we have to determine whether the definition of a symbol is also
  // necessary in addition to its declaration.

  CXCursor def_cursor;

  enum CXCursorKind const kind = clang_getCursorKind( cursor );
  if ( kind == CXCursor_TypeRef ) {
    CXType type = clang_getCursorType( parent );
    if ( type.kind == CXType_Invalid )
      return;

    switch ( type.kind ) {
      case CXType_Pointer:
      case CXType_LValueReference:
      case CXType_RValueReference:
        //
        // This handles a case like:
        //
        //      typedef struct c_type c_type_t;
        //
        //      void c_type_dump( c_type_t const *type );
        //
        // Even if c_type is incomplete at the time c_type_t was defined, it
        // doesn't matter since the parameter is using a pointer, so the
        // definition of c_type (and whatever include file it's declared in)
        // isn't needed.
        //
        return;
      default:
        /* suppress warning */;
    } // switch

    type = clang_getCanonicalType( type );
    if ( type.kind != CXType_Record )   // class, struct, or union
      return;

    CXCursor const type_cursor = clang_getTypeDeclaration( type );
    def_cursor = clang_getCursorDefinition( type_cursor );

    if ( tidy_Cursor_isBeforeInTranslationUnit( def_cursor, dec_cursor ) )
      return;
  }
  else {
    def_cursor = tidy_getCursorUnderlying( cursor );
  }

  if ( tidy_Cursor_isInvalid( def_cursor ) )
    return;
  if ( clang_equalCursors( def_cursor, dec_cursor ) )
    return;

  maybe_add_symbol( def_cursor, sivd );
}

////////// extern functions ///////////////////////////////////////////////////

void symbols_init( void ) {
  ASSERT_RUN_ONCE();
  rb_tree_init(
    &symbol_set, RB_DINT, POINTER_CAST( rb_cmp_fn_t, &tidy_symbol_cmp )
  );
  ATEXIT( &symbols_cleanup );

  CXCursor const cursor = clang_getTranslationUnitCursor( tidy_tu );
  symbols_init_visitor_data sivd = {
    .source_file = clang_getFile( tidy_tu, tidy_source_path )
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
