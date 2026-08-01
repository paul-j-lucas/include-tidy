/*
**      include-tidy -- #include tidier
**      src/clang_util.h
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

#ifndef include_tidy_clang_util_H
#define include_tidy_clang_util_H

/**
 * @file
 * Declares utility functions for libclang.
 */

// local
#include "pjl_config.h"

/// @cond DOXYGEN_IGNORE

// libclang
#include <clang-c/Index.h>

// standard
#include <stdbool.h>
#include <string.h>                     /* for memcmp(3) */

/// @endcond

/**
 * @defgroup clang-util-group libclang Utility Functions
 * Utility functions for libclang.
 * @{
 */

////////// extern functions ///////////////////////////////////////////////////

/**
 * Compares two CXCursor objects.
 *
 * @remarks What it means for one cursor to be "less than" another is
 * arbitrary, but consistent. Hence, this function is transitive and imposes a
 * strict total ordering.
 *
 * @param i_csr The first cursor.
 * @param j_csr The second cursor.
 * @return Returns a number less than 0, 0, or greater than 0 if \a i_csr is
 * less than, equal to, or greater than \a j_csr, respectively.
 */
NODISCARD
int tidy_Cursor_compare( CXCursor i_csr, CXCursor j_csr );

/**
 * Gets the cursor for the C++ class type as written in the source file.
 *
 * @par Example
 * @parblock
 * Given something like:
 *
 *      class Base {
 *      public:
 *        class iterator {
 *          // ...
 *        };
 *        // ...
 *      };
 *
 *      class Derived : public Base {
 *        // ...
 *      };
 *
 *      void f() {
 *        Derived::iterator i;
 *        // ...
 *
 * the cursor for `i` would be of type `Base::iterator` because `Derived`
 * doesn't contain `iterator` and instead inherits it from `Base`.  But in some
 * cases, we really want to know that it was `Derived` as written in the source
 * file.
 * @endparblock
 *
 * @param cursor The cursor for an expression.
 * @return Returns the cursor for its type as written in the source file.
 */
NODISCARD
CXCursor tidy_Cursor_getClassAsWritten( CXCursor cursor );

/**
 * Gets the first child cursor of \a cursor, if any.
 *
 * @param cursor The cursor to get the first child cursor of, if any.
 * @return Returns the first child cursor of \a cursor, or the null cursor if
 * none.
 *
 * @sa tidy_Cursor_getFirstExposedChild()
 */
NODISCARD
CXCursor tidy_Cursor_getFirstChild( CXCursor cursor );

/**
 * Gets the first exposed (i.e., not UnexposedExpr) child cursor of \a cursor,
 * if any.
 *
 * @param cursor The cursor.
 * @return Returns the first exposed child cursor of \a cursor, or the null
 * cursor if none.
 *
 * @sa tidy_Cursor_getFirstChild()
 */
NODISCARD
CXCursor tidy_Cursor_getFirstExposedChild( CXCursor cursor );

/**
 * Gets the scope for a function or operator.
 *
 * @remarks
 * @parblock
 * For a C++ member function or operator, this function returns the class it's
 * a member of.
 *
 * For a C++ non-member function or operator, iterates over its arguments and
 * returns the cursor for the class of the first argument that is a class; or
 * the cursor for the translation unit if none.
 * @endparblock
 *
 * @par Example
 * @parblock
 * Given something like:
 *
 *      class Base {
 *        // ...
 *      };
 *
 *      bool operator==( Base const&, Base const& );
 *
 * this function would return the cursor for the `Base` class.
 * @endparblock
 *
 * @param fn_csr The cursor for a function or operator to get the scope of.
 * @return Returns said scope.
 */
NODISCARD
CXCursor tidy_Cursor_getFunctionScope( CXCursor fn_csr );

/**
 * Gets the outermost C++ class for a cursor.
 *
 * @par Example
 * @parblock
 * Given:
 *
 *      namespace N {
 *        class A {
 *          class B {
 *            void f();
 *            // ...
 *
 * then the outermost class for `f()` would be `A`.
 * @endparblock
 *
 * @param cursor The cursor to get the outermost class of.
 * @return Returns said class cursor.
 */
NODISCARD
CXCursor tidy_Cursor_getOutermostClass( CXCursor cursor );

/**
 * Given a cursor at a local name of an enumeration, class, class data member,
 * class member function, structure, union, or namespace, gets its fully scoped
 * "display" name that includes template parameters (if any).
 *
 * @param cursor The cursor for a symbol.
 * @return Returns the fully scoped name.  The caller is responsible for
 * freeing it.
 *
 * @sa tidy_Cursor_getScopedSimpleName()
 */
NODISCARD
char* tidy_Cursor_getScopedDisplayName( CXCursor cursor );

/**
 * Given a cursor at a local name of an enumeration, class, class data member,
 * class member function, structure, union, or namespace, gets its fully scoped
 * "simple" name that does _not_ include template parameters (if any).
 *
 * @param cursor The cursor for a symbol.
 * @return Returns the fully scoped name.  The caller is responsible for
 * freeing it.
 *
 * @sa tidy_Cursor_getScopedDisplayName()
 */
NODISCARD
char* tidy_Cursor_getScopedSimpleName( CXCursor cursor );

/**
 * If \a cursor represents either a pointer or reference, gets the cursor for
 * the type to which it either points to or refers, respectively.
 *
 * @param cursor The cursor.
 * @return Returns the cursor for the type to which \a cursor either points to
 * or refers, respectively; or \a cursor if it's neither a pointer nor
 * reference.
 */
NODISCARD
CXCursor tidy_Cursor_getUnderlyingType( CXCursor cursor );

/**
 * Gets whether \a i_csr is before \a j_csr in the translation unit.
 *
 * @param i_csr The first cursor.
 * @param j_csr The second cursor.
 * @return Returns `true` only if \a i_csr is before \a j_csr in the
 * translation unit.
 */
NODISCARD
bool tidy_Cursor_isBeforeInTranslationUnit( CXCursor i_csr, CXCursor j_csr );

/**
 * Gets whether \a cursor is a class, class template, structure, or union
 * declaration.
 *
 * @param cursor The cursor to check.
 * @return Returns `true` only if \a cursor is a class, class template,
 * structure, or union declaration.
 *
 * @sa tidy_Cursor_isScopeDecl()
 */
NODISCARD
bool tidy_Cursor_isClassDecl( CXCursor cursor );

/**
 * Gets whether \a cursor is a function, member function, constructor,
 * destructor, or conversion operator.
 *
 * @param cursor The cursor to check.
 * @return Returns `true` only if \a cursor is a function.
 */
NODISCARD
bool tidy_Cursor_isFunctionDecl( CXCursor cursor );

/**
 * Gets whether \a cursor is referenced from \a file.
 *
 * @param cursor The cursor to use.
 * @param file The file of interest.
 * @return Returns `true` only if the \a cursor is referenced from \a file.
 */
NODISCARD
bool tidy_Cursor_isInFile( CXCursor cursor, CXFile file );

/**
 * Gets whether a class, data member, or member function given by \a cursor is
 * derived, inherited, or specialized from the class given by \a base_csr.
 *
 * @param cursor The candidate cursor.
 * @param base_csr The candidate base class cursor.
 * @return Returns `true` only if \a cursor is inherited from \a base_csr.
 */
NODISCARD
bool tidy_Cursor_isInheritedFrom( CXCursor cursor, CXCursor base_csr );

/**
 * Gets whether a C++ member function is inherited from a base class.
 *
 * @par Example
 * @parblock
 * Given something like:
 *
 *      class Base {
 *      public:
 *        void f();
 *      };
 *
 *      class Derived : public Base {
 *        // ...
 *      };
 *
 *      void g() {
 *        Derived d;
 *        d.f();                        // f() is inherited from Base
 *        // ...
 *
 * the member function `f()` isn't declared in `Derived`, but declared in and
 * inherited from `Base`.
 *
 * @endparblock
 *
 * @param expr_csr The expression the member function is being called on.
 * @param base_class_csr A optional pointer to receive the cursor for the base
 * class that the function was declared in.
 * @return Returns `true` only if the member function was inherited.
 */
NODISCARD
bool tidy_Cursor_isInheritedMemberFunctionCall( CXCursor expr_csr,
                                                CXCursor *base_class_csr );

/**
 * Gets whether \a cursor is either null or invalid.
 *
 * @param cursor The cursor to check.
 * @return Returns `true` only if \a cursor is either null or invalid.
 */
NODISCARD
bool tidy_Cursor_isInvalid( CXCursor cursor );

/**
 * Gets whether \a cursor is a class, class template, enumeration, namespace,
 * structure, or union declaration.
 *
 * @param cursor The cursor to check.
 * @return Returns `true` only if \a cursor is a class, class template,
 * enumeration, namespace, structure, or union.
 *
 * @sa tidy_Cursor_isClassDecl()
 */
NODISCARD
bool tidy_Cursor_isScopeDecl( CXCursor cursor );

/**
 * Compares two CXFile objects by name.
 *
 * @param i_file The first CXFile.
 * @param j_file The second CXFile.
 * @return Returns a number less than 0, 0, or greater than 0 if the name of \a
 * i_file is less than, equal to, or greater than the name of \a j_file,
 * respectively.
 *
 * @sa tidy_FileUniqueID_compare()
 */
NODISCARD
int tidy_File_compareByName( CXFile i_file, CXFile j_file );

/**
 * Compares two CXFileUniqueID objects.
 *
 * @param i_id The first CXFileUniqueID.
 * @param j_id The second CXFileUniqueID.
 * @return Returns a number less than 0, 0, or greater than 0 if \a i_id is
 * less than, equal to, or greater than \a j_id, respectively.
 *
 * @sa tidy_File_compareByName()
 */
NODISCARD
inline int tidy_FileUniqueID_compare( CXFileUniqueID const *i_id,
                                      CXFileUniqueID const *j_id ) {
  return memcmp( i_id, j_id, sizeof *i_id );
}

/**
 * Gets the real path of \a file.
 *
 * @param file The file to get the real path of.
 * @return Returns the string containing the real path of \a file.  The caller
 * _must_ call `clang_disposeString()` on it.
 *
 */
NODISCARD
CXString tidy_File_getRealPathName( CXFile file );

/**
 * Attempts to get the cursor for the identifier having \a name within \a
 * scope_csr.
 *
 * @param name The name to get the cursor for.
 * @param scope_csr The scope to look in.
 * @return Returns said cursor or an invalid cursor if not found.
 *
 * @sa tidy_getCursorByNameToken()
 */
NODISCARD
CXCursor tidy_getCursorByName( char const *name, CXCursor scope_csr );

/**
 * Gets the cursor for the identifier given by \a token within \a scope_csr,
 * but only if \a token actually is an identifier.
 *
 * @param tu The translation unit to use.
 * @param token The token to get the cursor for.
 * @param scope_csr The cursor of the scope to search within.
 * @return Returns said cursor; or an invalid cursor if \a token is an
 * identifier, but not found; or the null cursor if \a token is not an
 * identifier.
 *
 * @sa tidy_getCursorByName()
 */
NODISCARD
CXCursor tidy_getCursorByNameToken( CXTranslationUnit tu, CXToken token,
                                    CXCursor scope_csr );

/**
 * Similar to `clang_getCursorExtent()` except that it works better when macros
 * are involved.
 *
 * @par Example
 * @parblock
 * Given something like:
 *
 *      #include <stdalign.h>           // alignas
 *      #include <stddef.h>             // max_align_t
 *
 *      struct rb_node {
 *        // ...
 *        alignas( max_align_t ) char data[];
 *      };
 *
 * where `alignas` is a macro defined in `stdalign.h`, libclang never visits
 * the macro.  Furthermore, attempting to tokenize the extent of the FieldDecl
 * using clang_getCursorExtent() does _not_ return any tokens because libclang
 * can't tokenize a contiguous range across file boundaries (`alignas` is in
 * `stdalign.h` and the rest is in the file being tidied).
 *
 * However, _this_ function can do such a tokenization.
 * @endparblock
 *
 * @param cursor The cursor to get the source range for.
 * @return Returns said source range.
 */
NODISCARD
CXSourceRange tidy_getCursorExtent( CXCursor cursor );

/**
 * Calls `clang_getCursorLocation()` and returns only the file.
 *
 * @param cursor The cursor to use.
 * @return Returns the file \a cursor is in, if any.
 */
NODISCARD
CXFile tidy_getCursorLocation_File( CXCursor cursor );

/**
 * Calls `clang_getFileLocation()` and returns the `CXFile`.
 *
 * @param loc The `CXSourceLocation` to use.
 * @return Returns its `CXFile`.
 *
 * @sa tidy_getSpellingLocation_File()
 */
NODISCARD
CXFile tidy_getFileLocation_File( CXSourceLocation loc );

/**
 * Gets a unique ID for \a file.
 *
 * @note Unlike `clang_getFileUniqueID()`, this function never fails.
 *
 * @param file The file to get the unique ID for.
 * @return Returns said unique ID.
 */
NODISCARD
CXFileUniqueID tidy_getFileUniqueID( CXFile file );

/**
 * Calls `clang_getSpellingLocation()` and returns only the file.
 *
 * @param loc The location to use.
 * @return Returns the file of \a loc.
 *
 * @sa tidy_getFileLocation_File()
 */
NODISCARD
CXFile tidy_getSpellingLocation_File( CXSourceLocation loc );

/**
 * Gets whether the spelling of \a token equals \a value.
 *
 * @param tu The translation unit to use.
 * @param token The token to compare.
 * @param value The value to compare against.
 * @return Returns `true` only if the spelling of \a token equals \a value.
 */
NODISCARD
bool tidy_Token_isEqual( CXTranslationUnit tu, CXToken token,
                         char const *value );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tidy_clang_util_H */
/* vim:set et sw=2 ts=2: */
