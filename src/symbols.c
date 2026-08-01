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
#include "cli_options.h"
#include "config_file.h"
#include "includes.h"
#include "options.h"
#include "print.h"
#include "red_black.h"
#include "trans_unit.h"
#include "typedefs.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>                     /* for unreachable(3) */
#include <stdlib.h>
#include <string.h>

/// @endcond

/**
 * @addtogroup tidy-symbols-group
 * @{
 */

////////// enums //////////////////////////////////////////////////////////////

/**
 * The return value of add_cxx_member_fn().
 *
 * @remarks Having add_cxx_member_fn() return no/unknown is clearer than having
 * it return `false`/`true` since `true` implies "yes, add it."
 */
enum add_cxx_member_fn_rv {
  ADD_CXX_MEMBER_FN_NO,                 ///< Do not add symbol for function.
  ADD_CXX_MEMBER_FN_UNKNOWN             ///< Unknown whether to add symbol.
};

////////// typedefs ///////////////////////////////////////////////////////////

/// @cond DOXYGEN_IGNORE
typedef enum    add_cxx_member_fn_rv  add_cxx_member_fn_rv;
typedef struct  symbols_init_data     symbols_init_data;
/// @endcond

////////// structs ////////////////////////////////////////////////////////////

/**
 * Additional data passed to symbols_init_visitor.
 */
struct symbols_init_data {
  CXFile    source_file;                ///< The file being tidied.
  bool      verbose_printed;            ///< Printed any verbose output?

  /**
   * The cursor of the current C++ function (including member functions and
   * overloaded operators).
   *
   * @par Example
   * @parblock
   * For C++, things are more complicated.  Given something like:
   *
   *      // Base.h
   *      class Base {
   *        // ...
   *      };
   *
   *      bool operator==( Base const&, Base const& );
   *
   *      // Derived.h
   *      #include "Base.h"
   *      class Derived : public Base {
   *        // ...
   *      };
   *
   *      // Derived.cpp
   *      #include "Derived.h"
   *
   *      void f( Derived i, Derived j ) {
   *        if ( i == j )
   *          // ...
   *
   * Here, `Derived.cpp` correctly includes `Derived.h` that correctly includes
   * `Base.h`. `Derived.cpp` uses `operator==()` that's declared in `Base.h`,
   * so `Derived.cpp` should include `Base.h` according to the include-what-
   * you-use rule (IWYU).
   *
   * However, since `Derived` is derived from `Base`, that means the definition
   * of `Base` was available via `Derived.h` including `Base.h`; and since
   * the arguments to `operator==()` are `Derived` and `Derived.cpp` includes
   * `Derived.h`, that should be sufficient --- an exception to IWYU.
   * @endparblock
   *
   * @remarks
   * @parblock
   * To implement this, the decision to add the symbol for the function has to
   * be dererred until after all of its argument types are checked.
   * Specifically, if the function or operator:
   *
   *  + Has one or more arguments; and:
   *  + At least one of those arguments' type is derived from the relevant
   *    base.
   *
   * then the symbol name does not have to be added.
   * @endparblock
   */
  CXCursor cxx_func_cursor;

  /**
   * The cursor of the current C++ scope (class, structure, union, enumeration,
   * or namespace), if any.
   *
   * @par Example
   * @parblock
   * For C++, things are more complicated.  Given something like:
   *
   *      // Base.h
   *      class Base {
   *      public:
   *        using value_type = int;
   *        Base( int );
   *        // ...
   *      };
   *
   *      // Derived.h
   *      #include "Base.h"
   *      class Derived : public Base {
   *        Derived( int );
   *        // ...
   *      };
   *
   *      // Derived.cpp
   *      #include "Derived.h"
   *      using global_type = Derived::value_type;
   *      Derived::Derived( int n ) : Base{ n } { }
   *
   * Here, `Derived.cpp` correctly includes `Derived.h` that correctly includes
   * `Base.h`. `Derived.cpp` references both `Base::Base(int)` and
   * `Derived::value_type`.  Additionally for `value_type`:
   *
   *  + `Derived` doesn't declare it --- it's inherited from `Base`.
   *  + The reference to it is from the global scope, not a class scope.
   *
   * For all of these reasons, `Derived.cpp` should include `Base.h` according
   * to the include-what-you-use rule (IWYU).
   *
   * However, since `Derived` is derived from `Base`, that means the definition
   * of `Base` was available via `Derived.h` including `Base.h`; and since
   * `Derived.cpp` includes `Derived.h`, that should be sufficient --- an
   * exception to IWYU.  Furthermore, `value_type` has to be looked up via the
   * `Derived` scope, not the global scope.
   *
   * The exception applies to any referenced symbol that's inherited: data
   * members, constructors, destructors, or member functions.
   * @endparblock
   *
   * @remarks
   * @parblock
   * This needs to be maintained inside <code>%symbols_init_data</code> rather
   * than just a local variable inside \ref symbols_init_visitor() because of
   * the way libclang handles type aliases.  For one like `global_type`, the
   * AST is like:
   *
   *      global_type (TypeAliasDecl)
   *        |
   *        +-- Derived (TypeRef)
   *        +-- value_type (TypeRef)
   *
   * i.e., `value_type` is a sibling of `Derived`, not a child of it, so we
   * have to remember the scope of `Derived` between calls of \ref
   * symbols_init_visitor().
   * @endparblock
   */
  CXCursor cxx_scope_cursor;
};

////////// local functions ////////////////////////////////////////////////////

NODISCARD
static unsigned get_next_token_index( CXToken const[], unsigned, unsigned );

static void     tidy_symbol_cleanup( tidy_symbol* );
static bool     visit_CallExpr( CXCursor, CXCursor, symbols_init_data* );
static void     visit_FieldDecl( CXCursor, CXCursor, symbols_init_data* );
static void     visit_MacroDefinition( CXCursor, CXCursor, symbols_init_data* );
static void     visit_MemberRefExpr( CXCursor, CXCursor, symbols_init_data* );
static void     visit_most_kinds( CXCursor, CXCursor, symbols_init_data* );
static void     visit_OverloadedDeclRef( CXCursor, CXCursor,
                                         symbols_init_data* );

////////// local variables ////////////////////////////////////////////////////

static rb_tree_t symbol_set;            ///< Set of symbols.

////////// local functions ////////////////////////////////////////////////////

/**
 * Helper function for should_add_cxx_fn() that gets whether the symbol for a
 * C++ member function or operator should be added to the global set.
 *
 * @param call_cursor A CallExpr cursor.
 * @param fn_cursor The cursor of the function being called.
 * @return
 *  + #ADD_CXX_MEMBER_FN_NO only if the symbol for the member function should
 *    not be added.
 *  + #ADD_CXX_MEMBER_FN_UNKNOWN only if it is unknown whether to add the
 *    symbol for the member function (further checks are needed).
 */
NODISCARD
static add_cxx_member_fn_rv add_cxx_member_fn( CXCursor call_cursor,
                                               CXCursor fn_cursor ) {
  CXCursor const callee = tidy_Cursor_getFirstExposedChild( call_cursor );
  if ( clang_getCursorKind( callee ) != CXCursor_MemberRefExpr )
    return ADD_CXX_MEMBER_FN_UNKNOWN;

  CXCursor const obj_expr = tidy_Cursor_getFirstExposedChild( callee );
  if ( clang_Cursor_isNull( obj_expr ) )
    return ADD_CXX_MEMBER_FN_UNKNOWN;

  CXCursor const obj_class = tidy_Cursor_getUnderlyingType( obj_expr );
  CXCursor const fn_class = clang_getCursorSemanticParent( fn_cursor );

  if ( tidy_Cursor_isInheritedFrom( obj_class, fn_class ) )
    return ADD_CXX_MEMBER_FN_NO;

  CXCursor base_cursor = clang_getNullCursor();
  if ( !tidy_Cursor_isInheritedMemberFunctionCall( obj_expr, &base_cursor ) )
    return ADD_CXX_MEMBER_FN_NO;

  if ( clang_equalCursors( base_cursor, fn_class ) ||
        tidy_Cursor_isInheritedFrom( base_cursor, fn_class ) ) {
    return ADD_CXX_MEMBER_FN_NO;
  }

  return ADD_CXX_MEMBER_FN_UNKNOWN;
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
 * @param sid The symbols_init_data to use.
 */
static void maybe_add_symbol( CXCursor name_cursor, CXCursor sym_cursor,
                              symbols_init_data *sid ) {
  assert( sid != NULL );

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
  if ( clang_File_isEqual( sym_file, sid->source_file ) )
    return;

  tidy_typedef const *const found_tdef = typedef_find( sym_cursor );
  char *const simple_name = found_tdef != NULL ?
    check_strdup( found_tdef->alias_name ) :
    tidy_Cursor_getScopedSimpleName( name_cursor );

  if ( config_ignore_symbol( simple_name ) )
    goto done;

  tidy_symbol new_sym = {
    .name = tidy_Cursor_getScopedDisplayName( name_cursor )
  };
  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &symbol_set, &new_sym, sizeof new_sym );
  tidy_symbol *const sym = RB_DINT( rv_rbi.node );
  ++sym->ref_count;

  CXFile include_file = config_get_symbol_include( simple_name );
  if ( include_file == NULL )
    include_file = sym_file;
  tidy_include const *const include_added_to =
    include_add_symbol( include_file, sym );

  if ( !rv_rbi.inserted ) {
    tidy_symbol_cleanup( &new_sym );
    goto done;
  }

  if ( (opt_verbose & TIDY_VERBOSE_SYMBOLS) != 0 ) {
    if ( false_set( &sid->verbose_printed ) )
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

done:
  free( simple_name );
}

/**
 * Helper function for visit_CallExpr() that gets whether the symbol for a
 * C++ function or operator should be added to the global set.
 *
 * @note This function should be called only for C++ files being tidied.
 *
 * @param call_cursor A CallExpr cursor.
 * @param fn_cursor The cursor of the function being called.
 * @return Returns `true` only if the function should be added.
 */
NODISCARD
static bool should_add_cxx_fn( CXCursor call_cursor, CXCursor fn_cursor ) {
  assert( tidy_is_cxx );

  enum CXCursorKind const fn_kind = clang_getCursorKind( fn_cursor );
  bool const is_member_fn = fn_kind == CXCursor_CXXMethod ||
                            fn_kind == CXCursor_ConversionFunction;

  if ( is_member_fn && !add_cxx_member_fn( call_cursor, fn_cursor ) )
    return false;

  int const num_args = clang_Cursor_getNumArguments( call_cursor );
  assert( num_args >= 0 && "call_cursor is not a function" );
  if ( num_args == 0 )
    return true;

  CXCursor const scope_cursor = tidy_Cursor_getFunctionScope( fn_cursor );
  enum CXCursorKind const scope_kind = clang_getCursorKind( scope_cursor );
  bool const is_tu = clang_isTranslationUnit( scope_kind );

  for ( unsigned i = 0; i < STATIC_CAST( unsigned, num_args ); ++i ) {
    CXCursor const arg_cursor = clang_Cursor_getArgument( call_cursor, i );
    CXCursor const arg_class_cursor =
      tidy_Cursor_getClassAsWritten( arg_cursor );
    if ( clang_Cursor_isNull( arg_class_cursor ) )
      continue;

    if ( tidy_Cursor_isInheritedMemberFunctionCall( arg_cursor, NULL ) )
      return false;

    // Parameter inheritance check (e.g. operator!=(Base::iterator,
    // Base::iterator) called with Derived::iterator)
    CXCursor const param_cursor = clang_Cursor_getArgument( fn_cursor, i );
    if ( !clang_Cursor_isNull( param_cursor ) ) {
      CXCursor const arg_class =
        tidy_Cursor_getOutermostClass( arg_class_cursor );
      CXCursor const param_type_cursor =
        tidy_Cursor_getUnderlyingType( param_cursor );
      CXCursor const param_class =
        tidy_Cursor_getOutermostClass( param_type_cursor );
      if ( tidy_Cursor_isInheritedFrom( arg_class, param_class ) )
        return false;
    }

    // Scope / Namespace matching
    CXCursor const arg_parent =
      clang_getCursorSemanticParent( arg_class_cursor );

    if ( is_member_fn ) {
      // Mandate header if argument type is defined nested inside the member
      // function's class scope
      if ( clang_equalCursors( arg_parent, scope_cursor ) )
        return true;
    }
    else {
      // Suppress header if the free function is in a namespace and the argument
      // belongs to that same namespace
      if ( !is_tu && clang_equalCursors( arg_parent, scope_cursor ) )
        return false;
    }
  } // for

  return true;
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
 * @param parent The parent cursor of \a cursor.
 * @param data A pointer to a symbols_init_data.
 * @return Always returns `CXChildVisit_Continue`.
 */
static enum CXChildVisitResult symbols_init_visitor( CXCursor cursor,
                                                     CXCursor parent,
                                                     CXClientData data ) {
  (void)parent;
  assert( data != NULL );
  symbols_init_data *const sid = data;

  bool is_scope_change = false;
  CXCursor const prev_cxx_scope_cursor = sid->cxx_scope_cursor;

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

  if ( !tidy_Cursor_isInFile( cursor, sid->source_file ) )
    goto skip;

  if ( (opt_verbose & TIDY_VERBOSE_CURSORS) != 0 )
    verbose_print_cursor( "", cursor );

  if ( tidy_is_cxx ) {
    //
    // Since a non-null value of cxx_scope_cursor must span across multiple
    // calls to symbols_init_visitor() for siblings, we have to know when to
    // reset it.  Once way to do it is whenever the declaration or statement
    // changes.
    //
    is_scope_change = clang_isDeclaration( kind ) || clang_isStatement( kind );
    if ( is_scope_change )
      sid->cxx_scope_cursor = clang_getNullCursor();
  }

  switch ( kind ) {
    case CXCursor_CallExpr:
      if ( visit_CallExpr( cursor, parent, sid ) )
        goto skip_children;
      break;

    case CXCursor_DeclRefExpr:
    case CXCursor_FunctionDecl:
    case CXCursor_MacroExpansion:
    case CXCursor_TemplateRef:
    case CXCursor_TypeAliasDecl:
    case CXCursor_TypedefDecl:
    case CXCursor_TypeRef:
      visit_most_kinds( cursor, parent, sid );
      break;

    case CXCursor_FieldDecl:
      visit_FieldDecl( cursor, parent, sid );
      break;

    case CXCursor_MacroDefinition:
      visit_MacroDefinition( cursor, parent, sid );
      break;

    case CXCursor_MemberRefExpr:
      visit_MemberRefExpr( cursor, parent, sid );
      break;

    case CXCursor_NamespaceRef:
      //
      // Unlike most things in C++, a namespace can appear in multiple headers,
      // so there is no way to choose which is _the_ header for it.  Therefore,
      // we don't add namespaces to symbol_set.
      //
      break;

    case CXCursor_OverloadedDeclRef:
      visit_OverloadedDeclRef( cursor, parent, sid );
      break;

    default:
      /* suppress warning */;
  } // switch

  if ( tidy_is_cxx ) {
    //
    // If it's a scope, set cxx_scope_cursor.
    //
    switch ( kind ) {
      case CXCursor_NamespaceRef:
      case CXCursor_TemplateRef:
      case CXCursor_TypeRef:;
        CXCursor const ref_cursor = clang_getCursorReferenced( cursor );
        if ( tidy_Cursor_isScopeDecl( ref_cursor ) )
          sid->cxx_scope_cursor = ref_cursor;
        break;
      default:
        /* suppress warning */;
    } // switch
  }

skip:
  //
  // Returning CXChildVisit_Recurse causes clang_visitChildren() to do only
  // pre-order traversal, but we need to reset cxx_scope_cursor after visiting
  // a child node. Therefore, recurse manually.
  //
  clang_visitChildren( cursor, &symbols_init_visitor, data );

skip_children:
  if ( is_scope_change )
    sid->cxx_scope_cursor = prev_cxx_scope_cursor;
  return CXChildVisit_Continue;
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
 * Visits a `CXCursor_CallExpr` kind of cursor.
 *
 * @par Example
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
 * @param parent The parent cursor of \a call_cursor.
 * @param sid The symbols_init_data to use.
 */
static bool visit_CallExpr( CXCursor call_cursor, CXCursor parent,
                            symbols_init_data *sid ) {
  assert( sid != NULL );

  if ( tidy_is_cxx ) {
    //
    //
    //
    CXCursor const child_cursor = tidy_Cursor_getFirstChild( call_cursor );
    if ( !tidy_Cursor_isInvalid( child_cursor ) ) {
      enum CXCursorKind const child_kind = clang_getCursorKind( child_cursor );
      if ( child_kind == CXCursor_MemberRefExpr )
        return false;
    }

    CXCursor const func_cursor = clang_getCursorReferenced( call_cursor );
    bool const is_function = tidy_Cursor_isFunctionDecl( func_cursor );

    CXCursor const prev_func_cursor = sid->cxx_func_cursor;
    sid->cxx_func_cursor = is_function ? func_cursor : clang_getNullCursor();
    clang_visitChildren( call_cursor, &symbols_init_visitor, sid );
    sid->cxx_func_cursor = prev_func_cursor;

    if ( !is_function || !should_add_cxx_fn( call_cursor, func_cursor ) )
      return true;
  }

  visit_most_kinds( call_cursor, parent, sid );
  return tidy_is_cxx;
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
 * @param parent The parent cursor of \a field_cursor.
 * @param sid The symbols_init_data to use.
 */
static void visit_FieldDecl( CXCursor field_cursor, CXCursor parent,
                             symbols_init_data *sid ) {
  (void)parent;
  assert( sid != NULL );

  CXSourceRange const field_range = tidy_getCursorExtent( field_cursor );

  CXToken *tokens;
  unsigned token_count;
  clang_tokenize( tidy_tu, field_range, &tokens, &token_count );

  CXCursor const scope_cursor = clang_getCursorSemanticParent( field_cursor );

  for ( unsigned i = 0; i < token_count; ++i ) {
    CXCursor const sym_cursor =
      tidy_Token_getScopedNameCursor( tokens, token_count, &i, scope_cursor );
    if ( !tidy_Cursor_isInvalid( sym_cursor ) )
      maybe_add_symbol( sym_cursor, sym_cursor, sid );
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
 * @param parent The parent cursor of \a macro_cursor.
 * @param sid The symbols_init_data to use.
 */
static void visit_MacroDefinition( CXCursor macro_cursor, CXCursor parent,
                                   symbols_init_data *sid ) {
  (void)parent;
  assert( sid != NULL );

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
      maybe_add_symbol( sym_cursor, sym_cursor, sid );
  } // for

  rb_tree_cleanup( &param_set, /*free_fn=*/NULL );
  clang_disposeTokens( tidy_tu, tokens, token_count );
}

/**
 * Visit most kinds of cursor.
 *
 * @param cursor The cursor to visit.
 * @param parent The parent cursor of \a cursor.
 * @param sid The symbols_init_data to use.
 */
static void visit_most_kinds( CXCursor cursor, CXCursor parent,
                              symbols_init_data *sid ) {
  assert( sid != NULL );

  // Gets the cursor for the declaration of the symbol.
  CXCursor const dec_cursor = clang_getCursorReferenced( cursor );
  if ( tidy_Cursor_isInvalid( dec_cursor ) )
    return;

  if ( tidy_is_cxx ) {
    // See the comment for symbols_init_data::cxx_func_cursor.
    if ( clang_equalCursors( dec_cursor, sid->cxx_func_cursor ) )
      return;

    // See the comment for symbols_init_data::cxx_scope_cursor.
    CXCursor const base_cursor = clang_getCursorSemanticParent( dec_cursor );
    CXCursor const scope_cursor =
      !clang_Cursor_isNull( sid->cxx_scope_cursor ) ?
        sid->cxx_scope_cursor :
        parent;
    if ( tidy_Cursor_isInheritedFrom( scope_cursor, base_cursor ) )
      return;
  }

  maybe_add_symbol( dec_cursor, dec_cursor, sid );

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
  if ( clang_equalCursors( def_cursor, dec_cursor ) )
    return;
  if ( tidy_Cursor_isBeforeInTranslationUnit( def_cursor, dec_cursor ) )
    return;

  maybe_add_symbol( dec_cursor, def_cursor, sid );
}

/**
 * Visits a `CXCursor_MemberRefExpr` kind of cursor.
 *
 * @param member_ref_cursor The member reference's cursor to visit.
 * @param parent Not used.
 * @param sid The symbols_init_data to use.
 */
static void visit_MemberRefExpr( CXCursor member_ref_cursor, CXCursor parent,
                                 symbols_init_data *sid ) {
  (void)parent;
  assert( sid != NULL );

  // Gets the cursor for _a_ declaration of the symbol.
  CXCursor dec_cursor = clang_getCursorReferenced( member_ref_cursor );
  if ( tidy_Cursor_isInvalid( dec_cursor ) )
    return;

  CXCursor const parent_cursor = clang_getCursorSemanticParent( dec_cursor );
  if ( !tidy_Cursor_isClassDecl( parent_cursor ) )
    visit_most_kinds( member_ref_cursor, parent_cursor, sid );
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
 * @param sid The symbols_init_data to use.
 */
static void visit_OverloadedDeclRef( CXCursor overloaded_cursor,
                                     CXCursor parent,
                                     symbols_init_data *sid ) {
  (void)parent;
  assert( sid != NULL );

  unsigned const num_decls = clang_getNumOverloadedDecls( overloaded_cursor );
  for ( unsigned i = 0; i < num_decls; ++i ) {
    CXCursor dec_cursor = clang_getOverloadedDecl( overloaded_cursor, i );
    if ( tidy_Cursor_isInvalid( dec_cursor ) )
      continue;
    dec_cursor = clang_getCanonicalCursor( dec_cursor );
    if ( tidy_Cursor_isInvalid( dec_cursor ) )
      continue;
    maybe_add_symbol( dec_cursor, dec_cursor, sid );
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
  ATEXIT( &symbols_cleanup );
  typedefs_init();

  CXCursor const cursor = clang_getTranslationUnitCursor( tidy_tu );
  symbols_init_data sid = {
    .source_file = clang_getFile( tidy_tu, tidy_source_path ),
    .cxx_func_cursor = clang_getNullCursor(),
    .cxx_scope_cursor = clang_getNullCursor()
  };
  clang_visitChildren( cursor, &symbols_init_visitor, &sid );
  if ( sid.verbose_printed )
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
