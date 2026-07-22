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
typedef struct tidy_typedef               tidy_typedef;

////////// structs ////////////////////////////////////////////////////////////

/**
 * Additional data passed to symbols_init_visitor.
 */
struct symbols_init_visitor_data {
  CXFile  source_file;                  ///< The file being tidied.
  bool    verbose_printed;              ///< Printed any verbose output?
};

/**
 * Maps a cursor for either a `TypedefDecl` or a `TypeAliasDecl` to its scoped
 * alias name.
 *
 * @remarks
 * @parblock
 * It's necessary to keep a map of cursors for type aliases to their "pretty"
 * scoped alias names.
 * @endparblock
 *
 * @par Example
 * @parblock
 * Given:
 *
 *      namespace std {
 *        // ...
 *        using ostream = basic_ostream<char>;
 *        // ...
 *      }
 *
 * the \ref type_cursor is the entire `using` declaration and \ref alias_name
 * is `"std::ostream"`.  This mapping is needed to include the "pretty" names
 * in include comments.
 *
 * If the file being tidied uses `std::ostream` like:
 *
 *      void f( std::ostream& );
 *
 * then the symbol in the comment
 * will be `std::ostream` and not `std::basic_ostream`:
 *
 *      #include <ostream>              // std::ostream
 *
 * @endparblock
 */
struct tidy_typedef {
  CXCursor    type_cursor;              ///< `TypedefDecl` or `TypeAliasDecl`.
  char const *alias_name;               ///< Scoped alias name.
};

////////// local functions ////////////////////////////////////////////////////

NODISCARD
static unsigned get_next_token_index( CXToken const[], unsigned, unsigned );

static void     tidy_symbol_cleanup( tidy_symbol* );
static void     tidy_typedef_cleanup( tidy_typedef* );
static void     typedef_add( CXCursor );

NODISCARD
static tidy_typedef const* typedef_find( CXCursor );

static void     visit_CallExpr( CXCursor, CXCursor,
                                symbols_init_visitor_data* );
static void     visit_FieldDecl( CXCursor, CXCursor,
                                 symbols_init_visitor_data* );
static void     visit_MacroDefinition( CXCursor, CXCursor,
                                       symbols_init_visitor_data* );
static void     visit_MemberRefExpr( CXCursor, CXCursor,
                                     symbols_init_visitor_data* );
static void     visit_most_kinds( CXCursor, CXCursor,
                                  symbols_init_visitor_data* );
static void     visit_OverloadedDeclRef( CXCursor, CXCursor,
                                         symbols_init_visitor_data* );

////////// local variables ////////////////////////////////////////////////////

static rb_tree_t symbol_set;            ///< Set of symbols.
static rb_tree_t typedef_map;           ///< Map of typedefs.

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
    CXTokenKind const kind = clang_getTokenKind( tokens[i] );
    if ( kind != CXToken_Comment )
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
NODISCARD
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
 * For a macro, gets the cursor for the identifier given by \a token within \a
 * scope_cursor, but only if \a token actually is an identifier, neither
 * `__VA_ARGS__` nor `__VA_OPT__`, nor one of the current macro's parameters.
 *
 * @remarks This is a variant of tidy_getCursorByNameToken(), but for a macro
 * that additionally takes \a param_set.
 *
 * @param token The token to get the cursor for.
 * @param scope_cursor The cursor of the scope to search within.
 * @param param_set The set of macro parameter names.
 * @return Returns said cursor; or an invalid cursor if \a token is an
 * identifier, but not found; or the null cursor if \a token is:
 *  + Not an identifier; or:
 *  + Either `__VA_ARGS__` nor `__VA_OPT__`; or:
 *  + In \a param_set.
 *
 * @sa tidy_getCursorByNameToken()
 */
NODISCARD
static CXCursor macro_getCursorByNameToken( CXToken token,
                                            CXCursor scope_cursor,
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
static unsigned macro_get_params( CXToken const tokens[static 2],
                                  unsigned token_count,
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
 * For a macro, gets the cursor for the scoped symbol from \a tokens.
 *
 * @remarks This is a variant of tidy_Token_getScopedNameCursor(), but for a
 * macro that additionally takes \a param_set.
 *
 * @param tokens The array of macro tokens.
 * @param token_count The length of \a tokens.
 * @param ptoken_idx A pointer to the current index within \a tokens.
 * @param param_set The set of macro parameter names.
 * @return Returns said cursor or the null cursor for none.
 *
 * @sa tidy_Token_getScopedNameCursor()
 */
static CXCursor macro_Token_getScopedNameCursor( CXToken const tokens[],
                                                 unsigned token_count,
                                                 unsigned *ptoken_idx,
                                                 rb_tree_t const *param_set ) {
  assert( param_set != NULL );

  CXCursor const tu_cursor = clang_getTranslationUnitCursor( tidy_tu );

  CXCursor rv_cursor =
    macro_getCursorByNameToken( tokens[ *ptoken_idx ], tu_cursor, param_set );

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
    loop_cursor = macro_getCursorByNameToken( tokens[i], rv_cursor, param_set );
  } // while

  return rv_cursor;
}

/**
 * Helper function for symbols_init_visitor that maybe adds a symbol to the
 * global set.
 *
 * @param name_cursor The cursor to use for the name of the symbol.
 * @param sym_cursor The cursor for the symbol.
 * @param sivd The symbols_init_visitor_data to use.
 */
static void maybe_add_symbol( CXCursor name_cursor, CXCursor sym_cursor,
                              symbols_init_visitor_data *sivd ) {
  assert( sivd != NULL );

  enum CXCursorKind const kind = clang_getCursorKind( sym_cursor );
  switch ( kind ) {
    case CXCursor_Constructor:
    case CXCursor_CXXMethod:
    case CXCursor_Destructor:
      //
      // Even though the switch in symbols_init_visitor() doesn't include cases
      // for these, the referenced cursor obtained in visit_most_kinds() may
      // turn out to be one of these.
      //
      // However, adding the symbol for one of these doesn't add anything of
      // value since the file being tidied has to include the declaration for
      // the type anyway to call one of these on.
      //
      // Therefore, skip them.
      //
      return;
    default:
      /* suppress warning */;
  } // switch

  CXFile const sym_file = tidy_getCursorLocation_File( sym_cursor );
  if ( sym_file == NULL )
    return;

  // If the symbol was first declared in the file being tidied, we don't care.
  if ( clang_File_isEqual( sym_file, sivd->source_file ) )
    return;

  tidy_typedef const *const found_tdef = typedef_find( sym_cursor );
  char const *const name = found_tdef != NULL ?
    check_strdup( found_tdef->alias_name ) :
    tidy_Cursor_getScopedName( name_cursor );

  tidy_symbol new_sym = { .name = name };
  if ( config_ignore_symbol( new_sym.name ) )
    goto skip;

  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &symbol_set, &new_sym, sizeof new_sym );
  tidy_symbol *const sym = RB_DINT( rv_rbi.node );
  ++sym->ref_count;

  CXFile include_file = config_get_symbol_include( sym->name );
  if ( include_file == NULL )
    include_file = sym_file;
  tidy_include const *const include_added_to =
    include_add_symbol( include_file, sym );

  if ( !rv_rbi.inserted )
    goto skip;

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
  rb_tree_cleanup(
    &typedef_map, POINTER_CAST( rb_free_fn_t, &tidy_typedef_cleanup )
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
  symbols_init_visitor_data *const sivd = data;

  enum CXCursorKind const kind = clang_getCursorKind( cursor );
  switch ( kind ) {
    case CXCursor_TypeAliasDecl:
    case CXCursor_TypedefDecl:
      typedef_add( cursor );
      break;
    case CXCursor_UnexposedExpr:
      goto skip;
    default:
      /* suppress warning */;
  } // switch

  if ( !tidy_Cursor_isInFile( cursor, sivd->source_file ) )
    goto skip;

  if ( (opt_verbose & TIDY_VERBOSE_CURSORS) != 0 )
    verbose_print_cursor( cursor );

  switch ( kind ) {
    case CXCursor_CallExpr:
      visit_CallExpr( cursor, parent, sivd );
      break;

    case CXCursor_DeclRefExpr:
    case CXCursor_FunctionDecl:
    case CXCursor_MacroExpansion:
    case CXCursor_TemplateRef:
    case CXCursor_TypeAliasDecl:
    case CXCursor_TypedefDecl:
    case CXCursor_TypeRef:
      visit_most_kinds( cursor, parent, sivd );
      break;

    case CXCursor_FieldDecl:
      visit_FieldDecl( cursor, parent, sivd );
      break;

    case CXCursor_MacroDefinition:
      visit_MacroDefinition( cursor, parent, sivd );
      break;

    case CXCursor_MemberRefExpr:
      visit_MemberRefExpr( cursor, parent, sivd );
      break;

    case CXCursor_OverloadedDeclRef:
      visit_OverloadedDeclRef( cursor, parent, sivd );
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
 * Gets the cursor for the scoped symbol from \a tokens.
 *
 * @param tokens The array of macro tokens.
 * @param token_count The length of \a tokens.
 * @param ptoken_idx A pointer to the current index within \a tokens.
 * @param scope_cursor The scope to look in.
 * @return Returns said cursor or the null cursor for none.
 */
static CXCursor tidy_Token_getScopedNameCursor( CXToken const tokens[],
                                                unsigned token_count,
                                                unsigned *ptoken_idx,
                                                CXCursor scope_cursor ) {
  assert( ptoken_idx != NULL );

  CXCursor rv_cursor =
    tidy_getCursorByNameToken( tidy_tu, tokens[ *ptoken_idx ], scope_cursor );

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
    loop_cursor = tidy_getCursorByNameToken( tidy_tu, tokens[i], rv_cursor );
  } // while

  return rv_cursor;
}

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
  return tidy_Cursor_Compare( i_tdef->type_cursor, j_tdef->type_cursor );
}

/**
 * Adds either a `TypedefDecl` or `TypeAliasDecl` to a global map where \a
 * cursor is the key and its scoped alias name is its value.
 *
 * @param cursor The type cursor to add.
 *
 * @sa typedef_find()
 */
static void typedef_add( CXCursor cursor ) {
  CXType const type = clang_getTypedefDeclUnderlyingType( cursor );
  CXType const canonical_type = clang_getCanonicalType( type );
  CXCursor const type_cursor = clang_getTypeDeclaration( canonical_type );

  if ( tidy_Cursor_isInvalid( type_cursor ) )
    return;

  CXString const    alias_name_cxs = clang_getCursorSpelling( cursor );
  char const *const alias_name = clang_getCString( alias_name_cxs );
  CXString const    type_name_cxs = clang_getCursorSpelling( type_cursor );
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

  tidy_typedef new_tdef = { .type_cursor = type_cursor };
  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &typedef_map, &new_tdef, sizeof new_tdef );
  tidy_typedef *const tdef = RB_DINT( rv_rbi.node );
  //
  // We intentionally don't check rv_rbi.inserted because we want to allow
  // later typedefs to replace earlier ones.
  //
  FREE( tdef->alias_name );
  tdef->alias_name = tidy_Cursor_getScopedName( cursor );
}

/**
 * Attempts to find the type \a cursor in the global map of typedefs.
 *
 * @param cursor The type cursor to find.
 * @return Returns a pointer to the corresponding tidy_typedef or NULL if not
 * found.
 *
 * @sa typedef_add()
 */
static tidy_typedef const* typedef_find( CXCursor cursor ) {
  tidy_typedef const find_tdef = { .type_cursor = cursor };
  rb_node_t const *const found_rb = rb_tree_find( &typedef_map, &find_tdef );
  return found_rb != NULL ? RB_DINT( found_rb ) : NULL;
}

/**
 * Visits a `CXCursor_CallExpr` kind of cursor.
 *
 * @remarks
 * @parblock
 * For the case of a C++ member function call, its AST is like:
 *
 *      CallExpr
 *        MemberRefExpr
 *
 * that is the CallExpr has a child of a MemberRefExpr for the member function.
 * Since we handle MemberRefExpr cursors specially in visit_MemberRefExpr(), we
 * want do do nothing for the CallExpr.
 * @endparblock
 *
 * @param call_cursor The call expression's cursor to visit.
 * @param parent Not used.
 * @param sivd The symbols_init_visitor_data to use.
 */
static void visit_CallExpr( CXCursor call_cursor, CXCursor parent,
                            symbols_init_visitor_data *sivd ) {
  assert( sivd != NULL );
  (void)parent;

  CXCursor const child_cursor = tidy_Cursor_getFirstChild( call_cursor );
  if ( !tidy_Cursor_isInvalid( child_cursor ) ) {
    enum CXCursorKind const child_kind = clang_getCursorKind( child_cursor );
    if ( child_kind == CXCursor_MemberRefExpr )
      return;
  }

  visit_most_kinds( call_cursor, parent, sivd );
}

/**
 * Visits a `CXCursor_FieldDecl` kind of cursor.
 *
 * @remarks
 * @parblock
 * Ideally, we'd call visit_most_kinds() for a FieldDecl.  The problem is that,
 * given a field declaration like:
 *
 *      struct rb_node {
 *        // ...
 *        alignas( max_align_t ) char data[];
 *      };
 *
 * libclang's AST does _not_ include the `alignas` part, so we can't check that
 * the header that declares either `alignas` (for C &lt; C23) or `max_align_t`
 * has been included.
 *
 * Therefore, we have to fall back to iterating over all tokens of the field's
 * declaration looking for identifiers to see whether the header that declares
 * them has been included.
 * @endparblock
 *
 * @param field_cursor The field declaration's cursor to visit.
 * @param parent Not used.
 * @param sivd The symbols_init_visitor_data to use.
 */
static void visit_FieldDecl( CXCursor field_cursor, CXCursor parent,
                             symbols_init_visitor_data *sivd ) {
  (void)parent;
  assert( sivd != NULL );

  CXSourceRange const field_range = tidy_getCursorExtent( field_cursor );

  CXToken *tokens;
  unsigned token_count;
  clang_tokenize( tidy_tu, field_range, &tokens, &token_count );

  CXCursor const scope_cursor = clang_getCursorSemanticParent( field_cursor );

  for ( unsigned i = 0; i < token_count; ++i ) {
    CXCursor const sym_cursor =
      tidy_Token_getScopedNameCursor( tokens, token_count, &i, scope_cursor );
    if ( !tidy_Cursor_isInvalid( sym_cursor ) )
      maybe_add_symbol( sym_cursor, sym_cursor, sivd );
  } // for

  clang_disposeTokens( tidy_tu, tokens, token_count );
}

/**
 * Visits a `CXCursor_MacroDefinition` kind of cursor.
 *
 * @remarks
 * @parblock
 * We have to iterate over all tokens of the macro's definition looking for
 * identifiers to see whether the header that declares them has been included.
 * For example, if a header contains:
 *
 *      #define POINTER_CAST(T,EXPR)    ((T)(uintptr_t)(EXPR))
 *
 * it should also `#include <stdint.h>` because `uintptr_t` is used.  The user
 * of the macro shouldn't have to know or care about the declaration, nor be
 * forced to `#include <stdint.h>` explicitly.
 * @endparblock
 *
 * @param macro_cursor The macro definition's cursor to visit.
 * @param parent Not used.
 * @param sivd The symbols_init_visitor_data to use.
 */
static void visit_MacroDefinition( CXCursor macro_cursor, CXCursor parent,
                                   symbols_init_visitor_data *sivd ) {
  (void)parent;
  assert( sivd != NULL );

  CXSourceRange const macro_range = clang_getCursorExtent( macro_cursor );

  CXToken *tokens;
  unsigned token_count;
  clang_tokenize( tidy_tu, macro_range, &tokens, &token_count );

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
      macro_Token_getScopedNameCursor( tokens, token_count, &i, &param_set );
    if ( !tidy_Cursor_isInvalid( sym_cursor ) )
      maybe_add_symbol( sym_cursor, sym_cursor, sivd );
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

  // Gets the cursor for the declaration of the symbol.
  CXCursor dec_cursor = clang_getCursorReferenced( cursor );
  if ( tidy_Cursor_isInvalid( dec_cursor ) )
    return;

  maybe_add_symbol( dec_cursor, dec_cursor, sivd );

  // Now we have to determine whether the definition of a symbol is also
  // necessary in addition to its declaration.

  enum CXCursorKind const kind = clang_getCursorKind( cursor );
  if ( kind != CXCursor_TypeRef )
    return;

  CXType const type = clang_getCanonicalType( clang_getCursorType( parent ) );
  if ( type.kind != CXType_Record )     // class, struct, or union
    return;
  CXCursor const type_cursor = clang_getTypeDeclaration( type );
  if ( tidy_Cursor_isInvalid( type_cursor ) )
    return;
  CXCursor const def_cursor = clang_getCursorDefinition( type_cursor );
  if ( tidy_Cursor_isInvalid( def_cursor ) )
    return;
  if ( tidy_Cursor_isBeforeInTranslationUnit( def_cursor, dec_cursor ) )
    return;
  if ( clang_equalCursors( def_cursor, dec_cursor ) )
    return;

  maybe_add_symbol( dec_cursor, def_cursor, sivd );
}

/**
 * Visits a `CXCursor_MemberRefExpr` kind of cursor.
 *
 * @param member_ref_cursor The member reference's cursor to visit.
 * @param parent Not used.
 * @param sivd The symbols_init_visitor_data to use.
 */
static void visit_MemberRefExpr( CXCursor member_ref_cursor, CXCursor parent,
                                 symbols_init_visitor_data *sivd ) {
  (void)parent;
  assert( sivd != NULL );

  // Gets the cursor for _a_ declaration of the symbol.
  CXCursor dec_cursor = clang_getCursorReferenced( member_ref_cursor );
  if ( tidy_Cursor_isInvalid( dec_cursor ) )
    return;

  CXCursor const parent_cursor = clang_getCursorSemanticParent( dec_cursor );
  enum CXCursorKind const parent_kind = clang_getCursorKind( parent_cursor );
  switch ( parent_kind ) {
    case CXCursor_ClassDecl:
    case CXCursor_ClassTemplate:
    case CXCursor_StructDecl:
    case CXCursor_UnionDecl:
      break;
    default:
      visit_most_kinds( member_ref_cursor, parent_cursor, sivd );
      break;
  } // switch
}

/**
 * Visits a `CXCursor_OverloadedDeclRef` kind of cursor.
 *
 * @remarks We have to iterate over all overloaded functions since calling
 * clang_getCursorReferenced() on a CXCursor_OverloadedDeclRef returns an
 * invalid or null cursor.
 *
 * @param overloaded_cursor The overloaded definition's cursor to visit.
 * @param parent Not used.
 * @param sivd The symbols_init_visitor_data to use.
 */
static void visit_OverloadedDeclRef( CXCursor overloaded_cursor,
                                     CXCursor parent,
                                     symbols_init_visitor_data *sivd ) {
  (void)parent;
  assert( sivd != NULL );

  unsigned const num_decls = clang_getNumOverloadedDecls( overloaded_cursor );
  for ( unsigned i = 0; i < num_decls; ++i ) {
    CXCursor dec_cursor = clang_getOverloadedDecl( overloaded_cursor, i );
    if ( tidy_Cursor_isInvalid( dec_cursor ) )
      continue;
    dec_cursor = clang_getCanonicalCursor( dec_cursor );
    if ( tidy_Cursor_isInvalid( dec_cursor ) )
      continue;
    maybe_add_symbol( dec_cursor, dec_cursor, sivd );
    //
    // It's possible that different overloads will be declared in different
    // headers.  But for now, we stop after the first overload.
    //
    break;
  } // for
}

////////// extern functions ///////////////////////////////////////////////////

void symbols_init( void ) {
  ASSERT_RUN_ONCE();
  rb_tree_init(
    &symbol_set, RB_DINT, POINTER_CAST( rb_cmp_fn_t, &tidy_symbol_cmp )
  );
  rb_tree_init(
    &typedef_map, RB_DINT, POINTER_CAST( rb_cmp_fn_t, &tidy_typedef_cmp )
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
