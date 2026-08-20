/*
**      include-tidy -- #include tidier
**      src/cxx.h
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

#ifndef tidy_cxx_h
#define tidy_cxx_h

/**
 * @file
 * Declares functions for checking for C++ include-what-you-use (IWYU)
 * exceptions.
 */

// local
#include "pjl_config.h"

/// @cond DOXYGEN_IGNORE

// libclang
#include <clang-c/Index.h>

// standard
#include <stdbool.h>

/// @endcond

/**
 * @defgroup tidy-cxx-group C++ IWYU Exceptions
 * Tidying C++ code is much harder than tidying C code since C++ has
 * inheritance and nested types that require exceptions to the include-what-
 * you-use (IWYU) principle.
 * @{
 */

////////// extern functions ///////////////////////////////////////////////////

/**
 * Gets whether a call expression is via a C++ overloaded `operator->` and
 * whether it's a proxy for some other class and therefore constitites an
 * include-what-you-use (IWYU) exception.
 *
 * @par Example
 * @parblock
 * Given:
 *
 *      struct point {
 *        int x, y;
 *      };
 *
 *      class proxy {
 *      public:
 *        point* operator->() const {
 *          return _p;
 *        }
 *      private:
 *        point *_p;
 *      };
 *
 *      void f( proxy p ) {
 *        p->x = 0;
 *      }
 *
 * then \a call_csr refers to `p->` and \a mbr_cls_csr refers to `point`.
 * @endparblock
 *
 * @param call_csr The cursor for the call expresssion.
 * @param mbr_cls_csr The cursor for the class of the member.
 * @return Returns `true` only if \a call_csr is via an overloaded `operator->`
 * and \a mbr_cls_csr is _not_ the same as the class that defines (or inherits)
 * the operator, i.e., it's a proxy for \a mbr_cls_csr --- an IWYU exception.
 *
 * @note This function should be called only when the file being tidied is C++.
 */
NODISCARD
bool is_cxx_arrow_iwyu_exception( CXCursor call_csr, CXCursor mbr_cls_csr );

/**
 * Gets whether the symbol for a C++ function or operator (and the header that
 * declares it) should be added to the global set or constitites an include-
 * what-you-use (IWYU) exception.
 *
 * @param call_csr A CallExpr cursor.
 * @param fn_csr The cursor of the function being called.
 * @return Returns `true` only if the function (and the header that declares
 * it) should _not_ be added --- an IWYU exception.
 *
 * @note This function should be called only when the file being tidied is C++.
 *
 * @sa symbols_init_data::cxx_deferred_fn_csr
 */
NODISCARD
bool is_cxx_fn_iwyu_exception( CXCursor call_csr, CXCursor fn_csr );

/**
 * Gets whether the referenced C++ class member \a obj_csr constitutes an
 * include-what-you-use (IWYU) exception.
 *
 * @param obj_csr The C++ object whose member is being referenced.
 * @return Returns `true` only if \a obj_csr (and the header that declares it)
 * referencing the class member should _not_ be added --- an IWYU exception.
 *
 * @note This function should be called only when the file being tidied is C++.
 */
NODISCARD
bool is_cxx_mbr_ref_iwyu_exception( CXCursor obj_csr );

/**
 * Checks whether \a cursor and its declaration \a dec_csr constitute an
 * include-what-you-use (IWYU) exception for C++.
 *
 * @param cursor The cursor to check.
 * @param parent The parent cursor of \a cursor.
 * @param dec_csr The referenced cursor (declaration) of \a cursor.
 * @param scope_csr The cursor for the scope that should be used.
 * @return Returns `true` only if \a dec_csr (and the header that declares it)
 * should _not_ be added --- an IWYU exception.
 *
 * @note This function should be called only when the file being tidied is C++.
 */
NODISCARD
bool is_cxx_iwyu_exception( CXCursor cursor, CXCursor parent, CXCursor dec_csr,
                            CXCursor scope_csr );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* tidy_cxx_h */
/* vim:set et sw=2 ts=2: */
