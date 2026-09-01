/*
**      include-tidy -- #include tidier
**      src/cxx.c
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
 * Defines functions for checking for C++ include-what-you-use (IWYU)
 * exceptions.
 */

// local
#include "pjl_config.h"
#include "cxx.h"
#include "clang_util.h"
#include "cli_options.h"
#include "include.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

/// @endcond

/**
 * @addtogroup tidy-cxx-group
 * @{
 */

////////// local functions ////////////////////////////////////////////////////

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
 *
 * @note This function should be called only when the file being tidied is C++.
 */
NODISCARD
static bool has_cxx_qualifier_proxy( CXCursor cursor, CXCursor parent,
                                     CXCursor scope_csr ) {
  assert( tidy_source_is_cxx );

  CXSourceLocation const cursor_loc = clang_getCursorLocation( cursor );
  unsigned cursor_offset = tidy_getSpellingLocation_offset( cursor_loc );
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

  // Locate the specific token index corresponding to cursor.
  unsigned cursor_token_idx = token_count;
  for ( unsigned i = 0; i < token_count; ++i ) {
    CXSourceLocation const token_loc = clang_getTokenLocation( tu, tokens[i] );
    unsigned const token_offset = tidy_getSpellingLocation_offset( token_loc );
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

  // Scan backwards past template brackets <...> to find the qualifier token.
  int angle_depth = 0;
  while ( (ptoken = tidy_Token_getPrev( tokens, &i )) != NULL ) {
    int const match = clang_getTokenKind( *ptoken ) == CXToken_Punctuation ?
                      tidy_Token_isEqualToAny( tu, *ptoken, ">", "<" ) : -1;
    switch ( match ) {
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
 * Gets whether \a call_csr and the called C++ function (or operator) \a fn_csr
 * constitutes an include-what-you-use (IWYU) exception.
 *
 * @param call_csr A CallExpr cursor.
 * @param fn_csr The cursor of the function or operator being called.
 * @return Returns `true` only if the member function or operator (and the
 * header that declares it) should _not_ be added ---  an IWYU exception.
 *
 * @note This function should be called only when the file being tidied is C++.
 */
NODISCARD
static bool is_cxx_mbr_fn_iwyu_exception( CXCursor call_csr, CXCursor fn_csr ) {
  assert( tidy_source_is_cxx );

  CXCursor const callee_csr = tidy_Cursor_getFirstExposedChild( call_csr );
  // Ensure the callee is a member function, e.g., obj.f() or ptr->f().
  if ( clang_getCursorKind( callee_csr ) != CXCursor_MemberRefExpr )
    return false;

  CXCursor const obj_csr = tidy_Cursor_getFirstExposedChild( callee_csr );
  if ( clang_Cursor_isNull( obj_csr ) )
    return false;

  //
  // Check whether the object's class inherits from the member function's
  // class.  If it does, including the object's class header provides the
  // function's declaration --- an IWYU exception.
  //
  CXCursor const obj_cls_csr = tidy_Cursor_getUnderlyingType( obj_csr );
  CXCursor const fn_cls_csr = clang_getCursorSemanticParent( fn_csr );
  if ( tidy_Cursor_isInheritedFrom( obj_cls_csr, fn_cls_csr ) )
    return true;

  //
  // Check whether the member function call is on one inherited from a base
  // class: if not, it must be on our own class whose declaration must have
  // already been seen so we don't need its header --- an IWYU exception.
  //
  CXCursor base_csr;
  if ( !tidy_Cursor_isInheritedMemberFunctionCall( obj_csr, &base_csr ) )
    return true;

  //
  // Check whether the base class through which the member function is called
  // either is or derived from the function's class. If so, the base class's
  // header provides the function's declaration --- an IWYU exception.
  //
  if ( clang_equalCursors( base_csr, fn_cls_csr ) ||
        tidy_Cursor_isInheritedFrom( base_csr, fn_cls_csr ) ) {
    return true;
  }

  return false;
}

////////// extern functions ///////////////////////////////////////////////////

bool is_cxx_arrow_iwyu_exception( CXCursor call_csr, CXCursor mbr_cls_csr ) {
  assert( tidy_source_is_cxx );

  enum CXCursorKind const kind = clang_getCursorKind( call_csr );
  if ( kind != CXCursor_CallExpr )
    return false;

  CXCursor const op_csr = clang_getCursorReferenced( call_csr );
  if ( !tidy_Cursor_isSpellingEqualTo( op_csr, "operator->" ) )
    return false;

  // For obj->mbr, get obj.
  CXCursor const obj_csr = tidy_Cursor_getFirstExposedChild( call_csr );
  if ( tidy_Cursor_isInvalid( obj_csr ) )
    return true;

  // If obj is a pointer or reference, get the underlying type.
  CXCursor obj_cls_csr = tidy_Cursor_getUnderlyingType( obj_csr );
  if ( tidy_Cursor_isInvalid( obj_cls_csr ) )
    return true;

  //
  // If the proxy object's class (e.g., std::map<K,V>::iterator) is not the
  // same as the member's class (e.g., std::pair<T1,T2>), then operator-> is a
  // proxy for the member --- an IWYU exception.
  //
  obj_cls_csr = clang_getCanonicalCursor( obj_cls_csr );
  mbr_cls_csr = clang_getCanonicalCursor( mbr_cls_csr );
  return !clang_equalCursors( obj_cls_csr, mbr_cls_csr );
}

bool is_cxx_fn_iwyu_exception( CXCursor call_csr, CXCursor fn_csr ) {
  assert( tidy_source_is_cxx );

  enum CXCursorKind const fn_kind = clang_getCursorKind( fn_csr );
  switch ( fn_kind ) {
    case CXCursor_ConversionFunction:
    case CXCursor_CXXMethod:
      if ( is_cxx_mbr_fn_iwyu_exception( call_csr, fn_csr ) )
        return true;
      break;
    default:
      /* suppress warning */;
  } // switch

  // At this point, the function is either a non-member function or a class
  // static member function.

  int const num_args = clang_Cursor_getNumArguments( call_csr );
  assert( num_args >= 0 && "call_csr is not a function" );
  if ( num_args == 0 )
    return false;

  CXFile const fn_file = tidy_getCursorLocation_File( fn_csr );
  if ( fn_file == NULL )
    return false;
  tidy_include const *fn_include = include_find_by_File( fn_file );
  if ( fn_include == NULL )
    return false;
  fn_include = include_get_proxy( fn_include );

  //
  // Check all of the function's parameters: only one is needed to trigger an
  // IWYU exception.
  //
  for ( unsigned i = 0; i < STATIC_CAST( unsigned, num_args ); ++i ) {
    CXCursor const par_csr = clang_Cursor_getArgument( fn_csr, i );
    if ( clang_Cursor_isNull( par_csr ) )
      continue;
    CXCursor const par_type_csr = tidy_Cursor_getUnderlyingType( par_csr );
    CXCursor const par_ocls_csr = tidy_Cursor_getOutermostClass( par_type_csr );
    if ( clang_Cursor_isNull( par_ocls_csr ) )
      continue;

    //
    // Check whether the parameter's outermost class type is declared in the
    // same header (or proxy) as the function itself.
    //
    // If they're in different headers, including the header that declares the
    // parameter's type can't guarantee that the function is declared, so this
    // pararameter doesn't trigger an IWYU exception.
    //
    CXFile const par_file = tidy_getCursorLocation_File( par_ocls_csr );
    if ( par_file == NULL )
      continue;
    tidy_include const *par_include = include_find_by_File( par_file );
    if ( par_include == NULL )
      continue;
    par_include = include_get_proxy( par_include );
    if ( fn_include != par_include )
      continue;

    // Now compare the argument's type against the parameter's type.

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
    // Therefore, we allow the transitive include of <set> --- an IWYU
    // exception.
    //
    if ( tidy_Cursor_isTypeAliasOf( arg_cls_csr, par_type_csr ) )
      return true;

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
    // Therefore, we allow the transitive include of <set> --- an IWYU
    // exception.
    //
    CXCursor const arg_ocls_csr = tidy_Cursor_getOutermostClass( arg_cls_csr );

    if ( tidy_Cursor_isInheritedFrom( arg_ocls_csr, par_ocls_csr ) )
      return true;
    if ( tidy_Cursor_isTemplateSpecializationOf( arg_ocls_csr, par_ocls_csr ) )
      return true;
  } // for

  return false;
}

bool is_cxx_mbr_ref_iwyu_exception( CXCursor obj_csr ) {
  assert( tidy_source_is_cxx );

  if ( tidy_Cursor_isInvalid( obj_csr ) )
    return false;

  // Fully unwrap address-of, dereferences, parens, casts, and variable
  // initializers.
  CXCursor const init_csr = tidy_Cursor_getVarInitNoUnaryOps( obj_csr );
  if ( tidy_Cursor_isInvalid( init_csr ) )
    return false;

  //
  // Check whether the object's underlying type is declared within a C++ class,
  // e.g., a nested typedef like std::set<T>::value_type or a nested class like
  // std::set<T>::iterator.
  //
  // If it is, the class's header must have already provided the declaration
  // for the class that the type is declared within --- an IWYU exception.
  //
  CXCursor const type_dec_csr = tidy_Cursor_getUnderlyingType( init_csr );
  if ( !tidy_Cursor_isInvalid( type_dec_csr ) ) {
    CXCursor const sem_parent = clang_getCursorSemanticParent( type_dec_csr );
    if ( tidy_Cursor_isClassDecl( sem_parent ) )
      return true;
  }

  //
  // Get what initialized the object.  If it's a CXCursor_CallExpr, we have to
  // dig down to the callee.
  //
  CXCursor child_csr = tidy_Cursor_skipUnexposedDown( init_csr );
  if ( tidy_Cursor_isInvalid( child_csr ) )
    return false;
  CXCursor ref_csr = clang_getCursorReferenced( child_csr );
  if ( clang_Cursor_isNull( ref_csr ) ) {
    enum CXCursorKind const kind = clang_getCursorKind( child_csr );
    if ( kind == CXCursor_CallExpr ) {
      CXCursor const callee_csr = tidy_Cursor_getFirstExposedChild( child_csr );
      ref_csr = clang_getCursorReferenced( callee_csr );
    }
  }
  if ( tidy_Cursor_isInvalid( ref_csr ) )
    return false;

  enum CXCursorKind const kind = clang_getCursorKind( ref_csr );
  switch ( kind ) {
    case CXCursor_ConversionFunction:
    case CXCursor_CXXMethod:
    case CXCursor_FieldDecl:;
      //
      // The object is either a data member or the return value of either a
      // member function or conversion operator.
      //
      // If its semantic parent is a class declaration, the header defining the
      // class must have already provided the declaration for the object's type
      // and all its members --- an IWYU exception.
      //
      CXCursor const cls_csr = clang_getCursorSemanticParent( ref_csr );
      if ( tidy_Cursor_isClassDecl( cls_csr ) )
        return true;
      break;

    case CXCursor_ParmDecl:;
      //
      // If the parameter is for a C++ member function, the header declaring
      // the function's class already provided the declaration for the
      // parameter's type --- an IWYU exception.
      //
      CXCursor const fn_csr = clang_getCursorSemanticParent( ref_csr );
      if ( clang_getCursorKind( fn_csr ) == CXCursor_CXXMethod ) {
        CXCursor const fn_cls_csr = clang_getCursorSemanticParent( fn_csr );
        if ( tidy_Cursor_isClassDecl( fn_cls_csr ) )
          return true;
      }
      break;

    default:
      /* suppress warning */;
  } // switch

  return false;
}

bool is_cxx_iwyu_exception( CXCursor cursor, CXCursor parent, CXCursor dec_csr,
                            CXCursor scope_csr ) {
  assert( tidy_source_is_cxx );

  enum CXCursorKind const kind = clang_getCursorKind( cursor );

  if ( !clang_isDeclaration( kind ) &&
        has_cxx_qualifier_proxy( cursor, parent, scope_csr ) ) {
    return true;
  }

  // The remaining IWYU exceptions apply only within a C++ class scope.

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
    // and that's sufficient --- an IWYU exception.
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
    // includes Base.hpp, and that's sufficient --- an IWYU exception.
    //
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
