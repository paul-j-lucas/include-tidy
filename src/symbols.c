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
#include "hash_table.h"
#include "includes.h"
#include "options.h"
#include "print.h"
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
   * The C++ class of the current function or operator we're in.
   *
   * @par Example
   * @parblock
   * Given something like:
   *
   *      // int_set.hpp
   *      #include <set>
   *      struct int_set : std::set<int> {
   *        void f();
   *      };
   *
   *      // int_set.cpp
   *      #include "int_set.h"
   *
   *      void int_set::f() {
   *        auto v = value_type{ 0 };
   *        // ...
   *      }
   *
   * For an inherited type like `value_type` (inherited from `std::set`), when
   * unqualified, we need to know to look up the type in the function's class
   * scope.
   * @endparblock
   *
   * @note This is set while visiting any cursor _inside_ a function.
   */
  CXCursor cxx_current_fn_class_csr;

  /**
   * The cursor of the current C++ function (including member functions and
   * overloaded operators).
   *
   * @par Example
   * @parblock
   * For C++, things are more complicated.  Given something like:
   *
   *      // Base.hpp
   *      class Base {
   *        // ...
   *      };
   *
   *      bool operator==( Base const&, Base const& );
   *
   *      // Derived.hpp
   *      #include "Base.hpp"
   *      class Derived : public Base {
   *        // ...
   *      };
   *
   *      // Derived.cpp
   *      #include "Derived.hpp"
   *
   *      void f( Derived const &i, Derived const &j ) {
   *        if ( i == j )
   *          // ...
   *      }
   *
   * Here, `Derived.cpp` correctly includes `Derived.hpp` that correctly
   * includes `Base.hpp`. `Derived.cpp` uses `operator==()` that's declared in
   * `Base.hpp`, so `Derived.cpp` should include `Base.hpp` according to the
   * include-what-you-use rule (IWYU).
   *
   * However, since `Derived` is derived from `Base`, that means the definition
   * of `Base` was available via `Derived.hpp` including `Base.hpp`; and since
   * the arguments to `operator==()` are `Derived` and `Derived.cpp` includes
   * `Derived.hpp`, that's sufficient --- an exception to IWYU.
   * @endparblock
   *
   * @remarks
   * @parblock
   * To implement this, the decision to add the symbol for a function has to be
   * dererred until after all of its argument types are checked.  Specifically,
   * if the function or operator:
   *
   *  + Has one or more arguments; and:
   *  + At least one of those arguments' type is derived from the relevant
   *    base.
   *
   * then the symbol name does _not_ have to be added.
   * @endparblock
   *
   * @note This is set only while the current cursor being visited is a
   * function and not while _inside_ the function.
   */
  CXCursor cxx_deferred_fn_csr;

  /**
   * The cursor of the current C++ class (`class`, `struct`, or `union`), if
   * any.
   *
   * @par Example
   * @parblock
   * For C++, things are more complicated.  Given something like:
   *
   *      // Base.hpp
   *      struct Base {
   *        using value_type = int;
   *        Base( int );
   *        // ...
   *      };
   *
   *      // Derived.hpp
   *      #include "Base.hpp"
   *      struct Derived : Base {
   *        Derived( int );
   *        // ...
   *      };
   *
   *      // Derived.cpp
   *      #include "Derived.hpp"
   *      using global_type = Derived::value_type;
   *      Derived::Derived( int n ) : Base{ n } { }
   *
   * Here, `Derived.cpp` correctly includes `Derived.hpp` that correctly
   * includes `Base.hpp`. `Derived.cpp` references both `Base::Base(int)` and
   * `Derived::value_type`.  Additionally for `value_type`:
   *
   *  + `Derived` doesn't declare it --- it's inherited from `Base`.
   *  + The reference to it is from the global scope, not a class scope.
   *
   * For all of these reasons, `Derived.cpp` should include `Base.hpp`
   * according to the include-what-you-use rule (IWYU).
   *
   * However, since `Derived` is derived from `Base`, that means the definition
   * of `Base` was available via `Derived.hpp` including `Base.hpp`; and since
   * `Derived.cpp` includes `Derived.hpp`, that should be sufficient --- an
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
   *
   * @note This is set only for the current statement or declaration.  It is
   * reset upon encountering the next statement or declaration.
   */
  CXCursor cxx_statement_class_csr;
};

////////// local functions ////////////////////////////////////////////////////

NODISCARD
static add_cxx_member_fn_rv add_cxx_member_fn( CXCursor, CXCursor );

NODISCARD
static CXCursor symbols_init_data_cxx_scope( symbols_init_data const*,
                                             CXCursor );

static void     tidy_symbol_cleanup( tidy_symbol* );

NODISCARD
static bool     visit_CallExpr( CXCursor, CXCursor, symbols_init_data* );

static void     visit_FieldDecl( CXCursor, CXCursor, symbols_init_data* );
static void     visit_MacroDefinition( CXCursor, CXCursor, symbols_init_data* );
static void     visit_MemberRefExpr( CXCursor, CXCursor, symbols_init_data* );
static void     visit_most_kinds( CXCursor, CXCursor, symbols_init_data* );
static void     visit_OverloadedDeclRef( CXCursor, CXCursor,
                                         symbols_init_data* );

////////// local variables ////////////////////////////////////////////////////

static hash_table_t symbol_set;         ///< Set of symbols.

////////// local functions ////////////////////////////////////////////////////

/**
 * Helper function for visit_CallExpr() that gets whether the symbol for a
 * C++ function or operator should be added to the global set.
 *
 * @note This function should be called only for C++ files being tidied.
 *
 * @param call_csr A CallExpr cursor.
 * @param fn_csr The cursor of the function being called.
 * @return Returns `true` only if the function should be added.
 */
NODISCARD
static bool add_cxx_fn( CXCursor call_csr, CXCursor fn_csr ) {
  assert( tidy_is_cxx );

  CXFile const fn_file = tidy_getCursorLocation_File( fn_csr );
  if ( fn_file == NULL )
    return true;

  enum CXCursorKind const fn_kind = clang_getCursorKind( fn_csr );
  bool const is_member_fn = fn_kind == CXCursor_CXXMethod ||
                            fn_kind == CXCursor_ConversionFunction;

  if ( is_member_fn && !add_cxx_member_fn( call_csr, fn_csr ) )
    return false;

  // The function is either a non-member function or a static member function.

  int const num_args = clang_Cursor_getNumArguments( call_csr );
  assert( num_args >= 0 && "call_csr is not a function" );
  if ( num_args == 0 )
    return true;

  for ( unsigned i = 0; i < STATIC_CAST( unsigned, num_args ); ++i ) {
    CXCursor const par_csr = clang_Cursor_getArgument( fn_csr, i );
    if ( clang_Cursor_isNull( par_csr ) )
      continue;
    CXCursor const par_type_csr = tidy_Cursor_getUnderlyingType( par_csr );
    CXCursor const par_ocls_csr =
      tidy_Cursor_getOutermostClass( par_type_csr );
    if ( clang_Cursor_isNull( par_ocls_csr ) )
      continue;

    CXFile const par_file = tidy_getCursorLocation_File( par_ocls_csr );
    if ( par_file == NULL || !clang_File_isEqual( fn_file, par_file ) )
      continue;

    CXCursor const arg_csr = clang_Cursor_getArgument( call_csr, i );
    CXCursor const arg_cls_csr = tidy_Cursor_getClassAsWritten( arg_csr );
    if ( clang_Cursor_isNull( arg_cls_csr ) )
      continue;

    //
    // Given:
    //
    //      // int_set.hpp
    //      #include <set>
    //      using int_set = std::set<int>;
    //
    //      // test.cpp
    //      #include "int_set.hpp"
    //
    //      void erase_even( int_set &s ) {
    //        std::erase_if( s, []( auto x ) { return x % 2 == 0; } );
    //      }
    //
    // Even though test.cpp uses std::erase_if() declared in <set>, it's
    // sufficient that only int_set.hpp is included and <set> isn't because:
    //
    //  + s of type int_set is an for std::set; and:
    //  + In order to declare int_set, int_set.hpp must have included <set>.
    //
    // Therefore, we allow the transitive include of <set> as an exception to
    // the include-what-you-use rule.
    //
    if ( tidy_Cursor_isTypeAliasOf( arg_cls_csr, par_type_csr ) )
      return false;

    //
    // Similar to the above case, but instead of one type being the alias of
    // another, it's derived from another:
    //
    //      // int_set.hpp
    //      #include <set>
    //      struct int_set : std::set<int> {
    //        // ...
    //      };
    //
    //      // test.cpp
    //      #include "int_set.hpp"
    //
    //      void erase_even( int_set &s ) {
    //        std::erase_if( s, []( auto x ) { return x % 2 == 0; } );
    //      }
    //
    // Even though test.cpp uses std::erase_if() declared in <set>, it's
    // sufficient that only int_set.hpp is included and <set> isn't because:
    //
    //  + s of type int_set is derived from std::set; and:
    //  + In order to declare int_set, int_set.hpp must have included <set>.
    //
    // Therefore, we allow the transitive include of <set> as an exception to
    // the include-what-you-use rule.
    //
    CXCursor const arg_ocls_csr = tidy_Cursor_getOutermostClass( arg_cls_csr );

    if ( tidy_Cursor_isInheritedFrom( arg_ocls_csr, par_ocls_csr ) )
      return false;
    if ( tidy_Cursor_isTemplateSpecializationOf( arg_ocls_csr, par_ocls_csr ) )
      return false;
  } // for

  return true;
}

/**
 * Helper function for add_cxx_fn() that gets whether the symbol for a C++
 * member function or operator should be added to the global set.
 *
 * @param call_csr A CallExpr cursor.
 * @param fn_csr The cursor of the function being called.
 * @return
 *  + #ADD_CXX_MEMBER_FN_NO only if the symbol for the member function should
 *    not be added.
 *  + #ADD_CXX_MEMBER_FN_UNKNOWN only if it is unknown whether to add the
 *    symbol for the member function (further checks are needed).
 */
NODISCARD
static add_cxx_member_fn_rv add_cxx_member_fn( CXCursor call_csr,
                                               CXCursor fn_csr ) {
  CXCursor const callee_csr = tidy_Cursor_getFirstExposedChild( call_csr );
  if ( clang_getCursorKind( callee_csr ) != CXCursor_MemberRefExpr )
    return ADD_CXX_MEMBER_FN_UNKNOWN;

  CXCursor const obj_expr_csr = tidy_Cursor_getFirstExposedChild( callee_csr );
  if ( clang_Cursor_isNull( obj_expr_csr ) )
    return ADD_CXX_MEMBER_FN_UNKNOWN;

  CXCursor const obj_class_csr = tidy_Cursor_getUnderlyingType( obj_expr_csr );
  CXCursor const fn_class_csr = clang_getCursorSemanticParent( fn_csr );

  if ( tidy_Cursor_isInheritedFrom( obj_class_csr, fn_class_csr ) )
    return ADD_CXX_MEMBER_FN_NO;

  CXCursor base_csr;
  if ( !tidy_Cursor_isInheritedMemberFunctionCall( obj_expr_csr, &base_csr ) )
    return ADD_CXX_MEMBER_FN_NO;

  if ( clang_equalCursors( base_csr, fn_class_csr ) ||
        tidy_Cursor_isInheritedFrom( base_csr, fn_class_csr ) ) {
    return ADD_CXX_MEMBER_FN_NO;
  }

  return ADD_CXX_MEMBER_FN_UNKNOWN;
}

/**
 * Gets whether a symbol is referenced via an explicit C++ scope qualifier that
 * acts as its proxy.
 *
 * @par Example
 * @parblock
 * Given:
 *
 *      // int_set.hpp
 *      #include <set>
 *      using int_set = std::set<int>;
 *
 *      // test.cpp
 *      #include "int_set.hpp"
 *
 *      void f() {
 *        int_set::value_type v;
 *      }
 *
 * where \a cursor refers to `value_type`, the actual cursor libclang resolves
 * it to is `std::set<int>::value_type`.  The problem is that \b include-tidy
 * will think `test.cpp` requires `<set>` explicitly even though `test.cpp`
 * includes `int_set.hpp` that declared `int_set`.  The fact that `int_set` is
 * a `std::set` should be irrelevant and `<set>` should not be required.
 * @endparblock
 *
 * @remarks To handle this, we resort to checking the actual tokens before \a
 * cursor to see if they comprise a C++ class qualifier.  If a qualifier is
 * present, it serves as the "proxy" for any nested members within it.
 *
 * @param cursor The cursor for the the symbol.
 * @param parent The parent of \a cursor.
 * @param scope_csr The cursor representing the surrounding C++ class scope, if
 * any.
 * @return Returns `true` only if \a cursor is explicitly qualified.
 */
NODISCARD
static bool has_cxx_qualifier_proxy( CXCursor cursor, CXCursor parent,
                                     CXCursor scope_csr ) {
  assert( tidy_is_cxx );

  CXSourceLocation const cursor_loc = clang_getCursorLocation( cursor );
  unsigned cursor_offset = 0;
  clang_getSpellingLocation(
    cursor_loc, /*file=*/NULL, /*line=*/NULL, /*column=*/NULL, &cursor_offset
  );
  if ( cursor_offset == 0 )
    return false;

  CXSourceRange range = clang_getCursorExtent( parent );
  if ( clang_Range_isNull( range ) )
    range = clang_getCursorExtent( cursor );
  if ( clang_Range_isNull( range ) )
    return false;

  CXTranslationUnit tu = clang_Cursor_getTranslationUnit( cursor );
  CXToken *tokens;
  unsigned token_count;
  clang_tokenize( tu, range, &tokens, &token_count );
  if ( unlikely( token_count == 0 ) )
    return false;

  // Locate the specific token index corresponding to `cursor`
  unsigned cursor_token_idx = token_count;
  for ( unsigned i = 0; i < token_count; ++i ) {
    CXSourceLocation const token_loc = clang_getTokenLocation( tu, tokens[i] );
    unsigned token_offset = 0;
    clang_getSpellingLocation(
      token_loc, /*file=*/NULL, /*line=*/NULL, /*column=*/NULL, &token_offset
    );
    if ( token_offset == cursor_offset ) {
      cursor_token_idx = i;
      break;
    }
  } // for

  bool matched = false;

  if ( cursor_token_idx == 0 || cursor_token_idx == token_count )
    goto done;

  int i = STATIC_CAST( int, cursor_token_idx );
  CXToken const *ptoken = tidy_Token_getPrev( tokens, &i );
  if ( ptoken == NULL || !tidy_Token_isScopeQualifier( tu, *ptoken ) )
    goto done;

  CXCursor *const cursors = MALLOC( CXCursor, token_count );
  clang_annotateTokens( tu, tokens, token_count, cursors );

  // Scan backwards past template brackets <...> to find the qualifier token
  int angle_depth = 0;
  while ( (ptoken = tidy_Token_getPrev( tokens, &i )) != NULL ) {
    switch ( tidy_Token_isEqualToAny( tu, *ptoken, ">", "<" ) ) {
      case 0:
        ++angle_depth;
        continue;
      case 1:
        --angle_depth;
        continue;
      default:
        if ( angle_depth > 0 )
          continue;
        break;
    } // switch

    CXCursor qual_csr = clang_getCursorReferenced( cursors[i] );
    if ( tidy_Cursor_isInvalid( qual_csr ) )
      qual_csr = cursors[i];
    if ( tidy_Cursor_isInvalid( qual_csr ) )
      break;

    CXCursor const qual_parent = clang_getCursorSemanticParent( qual_csr );
    CXCursor const canon_qual_csr =
      tidy_Cursor_getCanonicalTypeDeclaration( qual_csr );

    if ( tidy_Cursor_isClassDecl( qual_csr ) ||
         tidy_Cursor_isClassDecl( canon_qual_csr ) ) {
      matched = true;
    }
    else if ( tidy_Cursor_isClassDecl( scope_csr ) ) {
      if ( clang_equalCursors( scope_csr, qual_parent ) ||
           clang_equalCursors( scope_csr, qual_csr ) ) {
        matched = true;
      }
    }

    break;
  } // for

  free( cursors );

done:
  clang_disposeTokens( tu, tokens, token_count );
  return matched;
}

/**
 * Checks whether \a cursor and its declaration \a dec_csr constitute an
 * include-what-you-use (IWYU) exception for C++.
 *
 * @param cursor The cursor to check.
 * @param parent The parent cursor of \a cursor.
 * @param dec_csr The referenced cursor (declaration) of \a cursor.
 * @param sid The symbols_init_data to use.
 * @return Returns `true` only if \a dec_csr (and the header that declares it)
 * should _not_ be added, i.e., is an IWYU exception.
 */
NODISCARD
static bool is_cxx_iwyu_exception( CXCursor cursor, CXCursor parent,
                                   CXCursor dec_csr, symbols_init_data *sid ) {
  assert( tidy_is_cxx );
  assert( sid != NULL );

  enum CXCursorKind const kind = clang_getCursorKind( cursor );
  CXCursor const scope_csr = symbols_init_data_cxx_scope( sid, parent );

  if ( !clang_isDeclaration( kind ) &&
        has_cxx_qualifier_proxy( cursor, parent, scope_csr ) ) {
    return true;
  }

  //
  // The remaining IWYU exceptions apply only within a C++ class scope.
  //
  if ( !tidy_Cursor_isClassDecl( scope_csr ) )
    return false;

  if ( clang_equalCursors( scope_csr, dec_csr ) ||
       tidy_Cursor_isInheritedFrom( scope_csr, dec_csr ) ) {
    //
    // Don't add the symbol (and the header that declares it) if it's either
    // the current class or one of its base classes.  Given:
    //
    //      // Base.hpp
    //      struct Base {
    //        Base( int );
    //      };
    //
    //      // Derived.hpp
    //      #include "Base.hpp"
    //      struct Derived : Base {
    //        Derived( int n ) : Base{ n } { }
    //      };
    //
    // Here, where scope_csr is Derived and dec_csr is Base, even though Base
    // (declared in Base.hpp) is referenced inside Derived's implementation,
    // Base (and Base.hpp) is not needed because Derived.hpp includes Base.hpp,
    // and that's sufficient --- an exception to IWYU.
    //
    return true;
  }

  CXCursor const dec_parent = clang_getCursorSemanticParent( dec_csr );
  if ( clang_equalCursors( scope_csr, dec_parent ) ||
       tidy_Cursor_isInheritedFrom( scope_csr, dec_parent ) ) {
    //
    // Don't add the symbol (and the header that declares it) if it's a member
    // (e.g., typedef, data member, etc.) declared within the current class or
    // inherited from a base class.  Given:
    //
    //      // Base.hpp
    //      struct Base {
    //        using value_type = int;
    //      };
    //
    //      // Derived.hpp
    //      #include "Base.hpp"
    //      struct Derived : Base {
    //        void f();
    //      };
    //
    //      // Derived.cpp
    //      #include "Derived.hpp"
    //      void Derived::f() {
    //        value_type v = 42;
    //      }
    //
    // Here, where scope_csr is Derived and dec_parent is Base, despite
    // referencing value_type (declared in Base.hpp) inside Derived, Base (and
    // Base.hpp) is not needed because Derived.cpp includes Derived.hpp that
    // includes Base.hpp, and that's sufficient --- an exception to IWYU.
    //
    return true;
  }

  return false;
}

#ifdef NEED_II_MATRIX                   /* See comment above ii_matrix def. */
/**
 * Gets whether it's possible to go from a cursor that refernces a symbol to
 * the cursor that defines said symbol via the set of files that were included.
 *
 * @param ref_csr A cursor referencing a symbol.
 * @param def_csr A cursor defining a symbol.
 * @return Returns `true` only if it's possible.
 */
NODISCARD
static bool is_include_path( CXCursor ref_csr, CXCursor def_csr ) {
  if ( tidy_Cursor_isInvalid( def_csr ) )
    return false;
  CXFile const def_file = tidy_getCursorLocation_File( def_csr );
  if ( def_file == NULL )
    return false;
  tidy_include const *const def_include = include_find_by_File( def_file );
  if ( def_include == NULL )
    return false;
  if ( includes_include( NULL, def_include ) > 0 )
    return true;

  if ( tidy_Cursor_isInvalid( ref_csr ) )
    return false;
  CXFile const ref_file = tidy_getCursorLocation_File( ref_csr );
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
 * scope_csr, but only if \a token actually is an identifier, neither
 * `__VA_ARGS__` nor `__VA_OPT__`, nor one of the current macro's parameters.
 *
 * @remarks This is a variant of tidy_getCursorByNameToken(), but for a macro
 * that additionally takes \a param_set.
 *
 * @param token The token to get the cursor for.
 * @param scope_csr The cursor of the scope to search within.
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
static CXCursor macro_getCursorByNameToken( CXToken token, CXCursor scope_csr,
                                            hash_table_t const *param_set ) {
  assert( param_set != NULL );

  if ( clang_getTokenKind( token ) != CXToken_Identifier )
    return clang_getNullCursor();

  CXString const    token_cxs = clang_getTokenSpelling( tidy_tu, token );
  char const *const token_cs = clang_getCString( token_cxs );

  CXCursor const rv_csr =
    strcmp( token_cs, "__VA_ARGS__" ) != 0 &&
    strcmp( token_cs, "__VA_OPT__" ) != 0 &&
    ht_find( param_set, token_cs ) == NULL ?
      tidy_getCursorByName( token_cs, scope_csr )
    :
      clang_getNullCursor();

  clang_disposeString( token_cxs );
  return rv_csr;
}

/**
 * Gets the names of all of a macro's parameters.
 *
 * @param tokens The array of macro tokens.
 * @param token_count The length of \a tokens.
 * @param param_set The set to add the parameter names to.
 * @return Returns the index of the token one past the `)`.
 */
NODISCARD
static unsigned macro_get_params( CXToken const tokens[static 2],
                                  unsigned token_count,
                                  hash_table_t *param_set ) {
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
        PJL_DISCARD(
          ht_insert(
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
NODISCARD
static
CXCursor macro_Token_getScopedNameCursor( CXToken const tokens[],
                                          unsigned token_count,
                                          unsigned *ptoken_idx,
                                          hash_table_t const *param_set ) {
  assert( param_set != NULL );
  unsigned token_idx = *ptoken_idx;

  CXCursor const tu_csr = clang_getTranslationUnitCursor( tidy_tu );
  CXCursor rv_csr =
    macro_getCursorByNameToken( tokens[ *ptoken_idx ], tu_csr, param_set );

  while ( !tidy_Cursor_isInvalid( rv_csr ) ) {
    unsigned next_idx = token_idx;
    CXToken const *t;

    // Look for "::".
    if ( (t = tidy_Token_getNext( tokens, token_count, &next_idx )) == NULL )
      break;
    if ( !tidy_Token_isScopeQualifier( tidy_tu, *t ) )
      break;

    // Look for an identifier.
    if ( (t = tidy_Token_getNext( tokens, token_count, &next_idx )) == NULL )
      break;
    CXCursor const next_csr =
      macro_getCursorByNameToken( *t, rv_csr, param_set );
    if ( tidy_Cursor_isInvalid( next_csr ) )
      break;

    rv_csr = next_csr;
    token_idx = next_idx;
  } // while

  *ptoken_idx = token_idx;
  return rv_csr;
}

/**
 * Helper function for symbols_init_visitor that maybe adds a symbol to the
 * global set.
 *
 * @param name_csr The cursor to use for the name of the symbol.  It may be
 * (and often is) the same as \a sym_csr.
 * @param sym_csr The cursor for the symbol to add (maybe).
 * @param sid The symbols_init_data to use.
 */
static void maybe_add_symbol( CXCursor name_csr, CXCursor sym_csr,
                              symbols_init_data *sid ) {
  assert( sid != NULL );

  enum CXCursorKind const sym_kind = clang_getCursorKind( sym_csr );
  switch ( sym_kind ) {
    case CXCursor_CXXMethod:
    case CXCursor_Constructor:
    case CXCursor_ConversionFunction:
    case CXCursor_Destructor:
      //
      // Even though the switch in symbols_init_visitor() doesn't include cases
      // for these, the referenced cursor obtained in visit_most_kinds() may
      // turn out to be one of these.
      //
      // However, adding the symbol for one of these would trigger a false-
      // positive include dependency for merely _calling_ the symbol when
      // inherited, e.g.:
      //
      //      // Base.hpp
      //      struct Base {
      //        void f();
      //      };
      //
      //      // Derived.hpp
      //      #include "Base.hpp"
      //      struct Derived : Base {
      //        void g();
      //      };
      //
      //      // Derived.cpp
      //      #include "Derived.hpp"
      //      void Derived::g() {
      //        f();
      //      }
      //
      // If these cases weren't skipped, then the call of f() in Derived.cpp
      // would trigger a dependency on Base.hpp because that's where f() is
      // declared.
      //
      // However, since Derived is derived from Base, that means the definition
      // of Base was available via Derived.hpp including Base.hpp and that's
      // sufficient --- an exception to IWYU.
      //
      return;
    default:
      /* suppress warning */;
  } // switch

  CXFile const sym_file = tidy_getCursorLocation_File( sym_csr );
  if ( unlikely( sym_file == NULL ) )
    return;

  // If the symbol was first declared in the file being tidied, we don't care.
  if ( clang_File_isEqual( sym_file, sid->source_file ) )
    return;

  tidy_typedef const *const found_tdef = typedef_find( sym_csr );
  char *const simple_name = found_tdef != NULL ?
    check_strdup( found_tdef->alias_name ) :
    tidy_Cursor_getScopedSimpleName( name_csr );

  if ( config_is_symbol_ignored( simple_name ) )
    goto done;

  tidy_symbol new_sym = {
    .name = tidy_Cursor_getScopedDisplayName( name_csr )
  };
  ht_insert_rv_t const hti = ht_insert( &symbol_set, &new_sym, sizeof new_sym );
  tidy_symbol *const sym = HT_DINT( hti.entry );
  ++sym->ref_count;

  CXFile include_file = config_get_symbol_include( simple_name );
  if ( include_file == NULL )
    include_file = sym_file;
  tidy_include const *const include_added_to =
    include_add_symbol( include_file, sym );

  if ( !hti.inserted ) {
    tidy_symbol_cleanup( &new_sym );
    goto done;
  }
  if ( include_added_to == NULL )
    goto done;

  if ( (opt_verbose & TIDY_VERBOSE_SYMBOLS) != 0 ) {
    if ( false_set( &sid->verbose_printed ) )
      verbose_printf( "symbols:\n" );
    char delims[2];
    include_get_delims( include_added_to, delims );
    verbose_printf(
      "  \"%s\" -> %c%s%c\n",
      sym->name, delims[0], include_added_to->abs_path, delims[1]
    );
  }

done:
  free( simple_name );
}

/**
 * Cleans-up all symbols.
 */
static void symbols_cleanup( void ) {
  ht_cleanup( &symbol_set, POINTER_CAST( ht_free_fn_t, &tidy_symbol_cleanup ) );
}

/**
 * Gets the cursor for the scope that should be used from \a sid for C++
 * tidy_Cursor_isInheritedFrom() look-ups.
 *
 * @param sid The symbols_init_data to use.
 * @param else_csr The cursor for the scope to return if \a sid doesn't have
 * one.
 * @return Returns The cursor for the scope that should be used.
 *
 * @sa symbols_init_data::cxx_statement_class_csr
 * @sa symbols_init_data::cxx_current_fn_class_csr
 * @sa visit_most_kinds()
 */
NODISCARD
static CXCursor symbols_init_data_cxx_scope( symbols_init_data const *sid,
                                             CXCursor else_csr ) {
  assert( sid != NULL );

  if ( !clang_Cursor_isNull( sid->cxx_current_fn_class_csr ) )
    return sid->cxx_current_fn_class_csr;

  if ( !clang_Cursor_isNull( sid->cxx_statement_class_csr ) )
    return sid->cxx_statement_class_csr;

  return else_csr;
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

  bool is_new_statement = false;
  CXCursor const prev_cxx_statement_class_csr = sid->cxx_statement_class_csr;

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
    verbose_print_cursor( cursor );

  if ( tidy_is_cxx ) {
    //
    // Since a non-null value of cxx_statement_class_csr must span across
    // multiple calls to symbols_init_visitor() for siblings, we have to know
    // when to reset it.  Once way to do it is whenever the declaration or
    // statement changes.
    //
    is_new_statement = clang_isDeclaration( kind ) || clang_isStatement( kind );
    if ( is_new_statement )
      sid->cxx_statement_class_csr = clang_getNullCursor();
  }

  switch ( kind ) {
    case CXCursor_CallExpr:
      if ( visit_CallExpr( cursor, parent, sid ) )
        goto skip_children;
      break;

    case CXCursor_CXXMethod:
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
    // If it's a class scope, set cxx_statement_class_csr.
    //
    switch ( kind ) {
      case CXCursor_TemplateRef:
      case CXCursor_TypeRef:;
        CXCursor const ref_csr = clang_getCursorReferenced( cursor );
        if ( tidy_Cursor_isClassDecl( ref_csr ) )
          sid->cxx_statement_class_csr = ref_csr;
        break;
      default:
        /* suppress warning */;
    } // switch
  }

skip:;
  // See the comment for symbols_init_data::cxx_current_fn_class_csr.
  CXCursor const prev_cxx_current_fn_class_csr = sid->cxx_current_fn_class_csr;
  if ( tidy_is_cxx && tidy_Cursor_isFunctionDecl( cursor ) ) {
    CXCursor const fn_class_csr = clang_getCursorSemanticParent( cursor );
    sid->cxx_current_fn_class_csr = tidy_Cursor_isClassDecl( fn_class_csr ) ?
      fn_class_csr :
      clang_getNullCursor();
  }

  //
  // Returning CXChildVisit_Recurse causes clang_visitChildren() to do only
  // pre-order traversal, but we need to reset both cxx_current_fn_class_csr
  // and cxx_statement_class_csr after visiting a child node. Therefore,
  // recurse manually.
  //
  clang_visitChildren( cursor, &symbols_init_visitor, data );

  sid->cxx_current_fn_class_csr = prev_cxx_current_fn_class_csr;

skip_children:
  if ( is_new_statement )
    sid->cxx_statement_class_csr = prev_cxx_statement_class_csr;
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
 * @param scope_csr The scope to look in.
 * @return Returns said cursor or the null cursor for none.
 */
NODISCARD
static CXCursor tidy_Token_getScopedNameCursor( CXToken const tokens[],
                                                unsigned token_count,
                                                unsigned *ptoken_idx,
                                                CXCursor scope_csr ) {
  assert( ptoken_idx != NULL );
  unsigned token_idx = *ptoken_idx;

  CXCursor rv_csr =
    tidy_getCursorByNameToken( tidy_tu, tokens[ *ptoken_idx ], scope_csr );

  while ( !tidy_Cursor_isInvalid( rv_csr ) ) {
    unsigned next_idx = token_idx;
    CXToken const *t;

    // Look for "::".
    if ( (t = tidy_Token_getNext( tokens, token_count, &next_idx )) == NULL )
      break;
    if ( !tidy_Token_isScopeQualifier( tidy_tu, *t ) )
      break;

    // Look for an identifier.
    if ( (t = tidy_Token_getNext( tokens, token_count, &next_idx )) == NULL )
      break;
    CXCursor const next_csr = tidy_getCursorByNameToken( tidy_tu, *t, rv_csr );
    if ( tidy_Cursor_isInvalid( next_csr ) )
      break;

    rv_csr = next_csr;
    token_idx = next_idx;
  } // while

  *ptoken_idx = token_idx;
  return rv_csr;
}

/**
 * Visits a `CXCursor_CallExpr` kind of cursor.
 *
 * @param call_csr The call expression's cursor to visit.
 * @param parent The parent cursor of \a call_csr.
 * @param sid The symbols_init_data to use.
 * @return Returns `true` only if we've already visited our child AST nodes (so
 * symbols_init_visitor() shouldn't).
 */
static bool visit_CallExpr( CXCursor call_csr, CXCursor parent,
                            symbols_init_data *sid ) {
  assert( sid != NULL );

  if ( tidy_is_cxx ) {
    CXCursor const child_csr = tidy_Cursor_getFirstChild( call_csr );
    if ( !tidy_Cursor_isInvalid( child_csr ) ) {
      enum CXCursorKind const child_kind = clang_getCursorKind( child_csr );
      //
      // For the case of a C++ member function call, its AST is like:
      //
      //      CallExpr
      //        MemberRefExpr
      //
      // that is the CallExpr has a child of a MemberRefExpr for the member
      // function.  Since we handle MemberRefExpr cursors specially in
      // visit_MemberRefExpr(), we want do do nothing for the CallExpr.
      //
      if ( child_kind == CXCursor_MemberRefExpr )
        return false;
    }

    CXCursor const fn_csr = clang_getCursorReferenced( call_csr );
    bool const is_function = tidy_Cursor_isFunctionDecl( fn_csr );

    // See the comment for symbols_init_data::cxx_deferred_fn_csr.
    CXCursor const prev_deferred_fn_csr = sid->cxx_deferred_fn_csr;
    sid->cxx_deferred_fn_csr = is_function ? fn_csr : clang_getNullCursor();
    clang_visitChildren( call_csr, &symbols_init_visitor, sid );
    sid->cxx_deferred_fn_csr = prev_deferred_fn_csr;

    if ( !is_function || !add_cxx_fn( call_csr, fn_csr ) )
      return true;
  }

  visit_most_kinds( call_csr, parent, sid );
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
 * @param field_csr The field declaration's cursor to visit.
 * @param parent The parent cursor of \a field_csr.
 * @param sid The symbols_init_data to use.
 */
static void visit_FieldDecl( CXCursor field_csr, CXCursor parent,
                             symbols_init_data *sid ) {
  (void)parent;
  assert( sid != NULL );

  CXSourceRange const field_range = tidy_getCursorExtent( field_csr );

  CXToken *tokens;
  unsigned token_count;
  clang_tokenize( tidy_tu, field_range, &tokens, &token_count );
  if ( unlikely( token_count == 0 ) )
    return;

  CXString const    field_name_cxs = clang_getCursorSpelling( field_csr );
  char const *const field_name_cs  = clang_getCString( field_name_cxs );

  CXCursor const class_csr = clang_getCursorSemanticParent( field_csr );

  for ( unsigned i = 0; i < token_count; ++i ) {
    if ( clang_getTokenKind( tokens[i] ) != CXToken_Identifier )
      continue;

    CXString const    token_cxs = clang_getTokenSpelling( tidy_tu, tokens[i] );
    char const *const token_cs  = clang_getCString( token_cxs );
    bool const        is_field_name = strcmp( token_cs, field_name_cs ) == 0;

    clang_disposeString( token_cxs );

    if ( is_field_name )
      continue;

    CXCursor const sym_csr =
      tidy_Token_getScopedNameCursor( tokens, token_count, &i, class_csr );
    if ( !tidy_Cursor_isInvalid( sym_csr ) )
      maybe_add_symbol( sym_csr, sym_csr, sid );
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
 * @param macro_csr The macro definition's cursor to visit.
 * @param parent The parent cursor of \a macro_csr.
 * @param sid The symbols_init_data to use.
 */
static void visit_MacroDefinition( CXCursor macro_csr, CXCursor parent,
                                   symbols_init_data *sid ) {
  (void)parent;
  assert( sid != NULL );

  CXSourceRange const macro_range = clang_getCursorExtent( macro_csr );

  CXToken *tokens;
  unsigned token_count;
  clang_tokenize( tidy_tu, macro_range, &tokens, &token_count );
  if ( unlikely( token_count == 0 ) )
    return;

  //
  // While iterating over all tokens of the macro, we have to skip identifers
  // of macro parameters for function-like macros because those are obviously
  // defined by the macro itself.  To skip them, we first have to collect the
  // set of them.
  //
  hash_table_t param_set;
  ht_init(
    &param_set, HT_DINT, 2.0, 10,
    POINTER_CAST( ht_cmp_fn_t, &strcmp ),
    POINTER_CAST( ht_hash_fn_t, &fnv1a_s )
  );

  unsigned i = clang_Cursor_isMacroFunctionLike( macro_csr ) ?
    macro_get_params( tokens, token_count, &param_set ) :
    1;                                  // tokens[0] = macro name; start at 1

  for ( ; i < token_count; ++i ) {
    CXCursor const sym_csr =
      macro_Token_getScopedNameCursor( tokens, token_count, &i, &param_set );
    if ( !tidy_Cursor_isInvalid( sym_csr ) )
      maybe_add_symbol( sym_csr, sym_csr, sid );
  } // for

  ht_cleanup( &param_set, /*free_fn=*/NULL );
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
  CXCursor const dec_csr = clang_getCursorReferenced( cursor );
  if ( tidy_Cursor_isInvalid( dec_csr ) )
    return;

  if ( tidy_is_cxx ) {
    // See the comment for symbols_init_data::cxx_deferred_fn_csr.
    if ( clang_equalCursors( dec_csr, sid->cxx_deferred_fn_csr ) )
      return;

    if ( is_cxx_iwyu_exception( cursor, parent, dec_csr, sid ) )
      return;
  }

  maybe_add_symbol( dec_csr, dec_csr, sid );

  //
  // Now we have to determine whether the definition of a symbol is also
  // necessary in addition to its declaration.  Given:
  //
  //      // Point.hpp
  //      struct point {
  //        int x, y;
  //      };
  //
  //      // Foo.cpp
  //      void pass_thru( point *p ) {
  //        f( p );
  //      }
  //
  //      // Bar.cpp
  //      void point_init( point *p ) {
  //        p->x = p->y = 0;
  //      }
  //
  // `Foo.cpp` doesn't access any member of `point`, so its declaration is
  // sufficient whereas `Bar.cpp` accesses members, so its definition (and the
  // header that defines it) is necessary.
  //

  CXCursor class_csr;
  CXCursor def_csr;

  if ( tidy_is_cxx &&
       tidy_Cursor_isOutOfLineDefinition( cursor, parent, &class_csr ) ) {
    //
    // For C++ out-of-line member definitions, e.g.:
    //
    //      // S.hpp
    //      struct S {
    //        void f();
    //      };
    //
    //      // S.cpp
    //      #include "S.h"
    //      void S::f() {
    //        // ...
    //      }
    //
    // S::f() requires the definition of S, i.e., its class, and this is what
    // we pass to maybe_add_symbol(), not the original cursor for the member,
    // since maybe_add_symbol() ignores C++ methods (see its comment for why).
    //
    def_csr = clang_getCursorDefinition( class_csr );
    if ( tidy_Cursor_isInvalid( def_csr ) )
      return;
  }
  else {
    enum CXCursorKind const kind = clang_getCursorKind( cursor );
    switch ( kind ) {
      case CXCursor_TypeRef:
      case CXCursor_TemplateRef:
        break;
      default:
        return;
    } // switch

    CXType const type = clang_getCanonicalType( clang_getCursorType( parent ) );
    if ( type.kind != CXType_Record )     // class, struct, or union
      return;
    CXCursor const type_csr = clang_getTypeDeclaration( type );
    if ( tidy_Cursor_isInvalid( type_csr ) )
      return;
    def_csr = clang_getCursorDefinition( type_csr );
    if ( tidy_Cursor_isInvalid( def_csr ) )
      return;
    if ( clang_equalCursors( def_csr, dec_csr ) )
      return;

    // If we've already seen the definition, we don't need this declaration.
    if ( tidy_Cursor_isBeforeInTranslationUnit( def_csr, dec_csr ) )
      return;
  }

  maybe_add_symbol( dec_csr, def_csr, sid );
}

/**
 * Visits a `CXCursor_MemberRefExpr` kind of cursor.
 *
 * @param member_ref_csr The member reference's cursor to visit.
 * @param parent Not used.
 * @param sid The symbols_init_data to use.
 */
static void visit_MemberRefExpr( CXCursor member_ref_csr, CXCursor parent,
                                 symbols_init_data *sid ) {
  (void)parent;
  assert( sid != NULL );

  // Gets the cursor for _a_ declaration of the symbol.
  CXCursor const dec_csr = clang_getCursorReferenced( member_ref_csr );
  if ( tidy_Cursor_isInvalid( dec_csr ) )
    return;

  CXCursor const dec_class_csr = clang_getCursorSemanticParent( dec_csr );
  if ( !tidy_Cursor_isClassDecl( dec_class_csr ) )
    visit_most_kinds( member_ref_csr, dec_class_csr, sid );
}

/**
 * Visits a `CXCursor_OverloadedDeclRef` kind of cursor.
 *
 * @remarks We have to iterate over all overloaded functions since calling
 * clang_getCursorReferenced() on a CXCursor_OverloadedDeclRef returns an
 * invalid or null cursor.
 *
 * @param overloaded_csr The overloaded definition's cursor to visit.
 * @param parent Not used.
 * @param sid The symbols_init_data to use.
 */
static void visit_OverloadedDeclRef( CXCursor overloaded_csr, CXCursor parent,
                                     symbols_init_data *sid ) {
  (void)parent;
  assert( sid != NULL );

  unsigned const num_decls = clang_getNumOverloadedDecls( overloaded_csr );
  for ( unsigned i = 0; i < num_decls; ++i ) {
    CXCursor dec_csr = clang_getOverloadedDecl( overloaded_csr, i );
    if ( tidy_Cursor_isInvalid( dec_csr ) )
      continue;
    dec_csr = clang_getCanonicalCursor( dec_csr );
    if ( tidy_Cursor_isInvalid( dec_csr ) )
      continue;
    maybe_add_symbol( dec_csr, dec_csr, sid );
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
  ht_init(
    &symbol_set, HT_DINT, 2.0, 1024,
    POINTER_CAST( ht_cmp_fn_t, &tidy_symbol_cmp ),
    POINTER_CAST( ht_hash_fn_t, &tidy_symbol_hash )
  );
  ATEXIT( &symbols_cleanup );
  typedefs_init();

  CXCursor const cursor = clang_getTranslationUnitCursor( tidy_tu );
  symbols_init_data sid = {
    .source_file = clang_getFile( tidy_tu, tidy_source_path ),
    .cxx_current_fn_class_csr = clang_getNullCursor(),
    .cxx_deferred_fn_csr = clang_getNullCursor(),
    .cxx_statement_class_csr = clang_getNullCursor()
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

ht_hash_val_t tidy_symbol_hash( tidy_symbol const *sym ) {
  return fnv1a_s( sym->name );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
