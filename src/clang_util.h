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
 * Gets whether \a i_cursor is before \a j_cursor in the translation unit.
 *
 * @param i_cursor The first cursor.
 * @param j_cursor The second cursor.
 * @return Returns `true` only if \a i_cursor is before \a j_cursor in the
 * translation unit.
 */
NODISCARD
bool tidy_Cursor_isBeforeInTranslationUnit( CXCursor i_cursor,
                                            CXCursor j_cursor );

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
 * Gets whether \a cursor is either null or invalid.
 *
 * @param cursor The cursor to check.
 * @return Returns `true` only if \a cursor is either null or invalid.
 */
NODISCARD
bool tidy_Cursor_isInvalid( CXCursor cursor );

/**
 * Compares two CXFile objects by name.
 *
 * @param i_file The first CXFile.
 * @param j_file The second CXFile.
 * @return Returns a number less than 0, 0, or greater than 0 if the name of \a
 * i_file is less than, equal to, or greater than the name of \a j_file,
 * respectively.
 *
 * @sa tidy_FileUniqueID_cmp()
 */
NODISCARD
int tidy_File_cmp_by_name( CXFile i_file, CXFile j_file );

/**
 * Compares two CXFileUniqueID objects.
 *
 * @param i_id The first CXFileUniqueID.
 * @param j_id The second CXFileUniqueID.
 * @return Returns a number less than 0, 0, or greater than 0 if \a i_id is
 * less than, equal to, or greater than \a j_id, respectively.
 *
 * @sa tidy_File_cmp_by_name()
 */
NODISCARD
inline int tidy_FileUniqueID_cmp( CXFileUniqueID const *i_id,
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
 * Attempts to cwt the cursor for the identifier having \a name within \a
 * scope_cursor.
 *
 * @param name The name to get the cursor for.
 * @param scope_cursor The scope to look in.
 * @return Returns said cursor or the null cursor if not found.
 *
 * @sa tidy_getCursorByToken()
 */
NODISCARD
CXCursor tidy_getCursorByName( char const *name, CXCursor scope_cursor );

/**
 * Attempts to get the cursor for the identifier given by \a token within \a
 * scope_cursor, but only if \a token actually is an identifier.
 *
 * @param token The token to get the cursor for.
 * @param scope_cursor The scope to look in.
 * @return Returns said cursor, or the null cursor if \a token is not an
 * identifier or not found.
 *
 * @sa tidy_getCursorByName()
 */
NODISCARD
CXCursor tidy_getCursorByToken( CXToken token, CXCursor scope_cursor );

/**
 * Similar to `clang_getCursorExtent()` except that it works better when macros
 * are involved.
 *
 * @remarks
 * @parblock
 * For example, given this code to tidy:
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
 * Gets the file the given cursor is in, if any.
 *
 * @param cursor The cursor to use.
 * @return Returns the file \a cursor is in, if any.
 */
NODISCARD
CXFile tidy_getCursorLocation_File( CXCursor cursor );

/**
 * Given a cursor at a local name of an enumeration, class, class data member,
 * class member function, structure, union, or namespace, gets its fully scoped
 * name.
 *
 * @param cursor The cursor for a symbol.
 * @return Returns the fully scoped name.  The caller is responsible for
 * freeing it.
 */
NODISCARD
char const* tidy_getCursorScopedName( CXCursor cursor );

/**
 * Calls `clang_getCursorSpelling()` and `clang_getCString()`.
 *
 * @param cursor The cursor to use.
 * @return Returns the C string version of \a cursor.  The caller is
 * responsible for freeing it.
 */
NODISCARD
char* tidy_getCursorSpelling( CXCursor cursor );

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
 * where \a cursor is at a use of `foo_t`, we want to get the cursor for the
 * underlying type, in this case for `foo`.
 *
 * Additionally, for a case like:
 *
 *      typedef struct foo *foo_ptr_t;
 *
 * we have to traverse through all pointers (and references for C++) to get to
 * the cursor for the underlying type.
 * @endparblock
 *
 * @param cursor The original cursor to get the underlying cursor for.
 * @return Returns the underlying cursor for \a cursor, if necessary, or the
 * null cursor if not or none.
 */
NODISCARD
CXCursor tidy_getCursorUnderlying( CXCursor cursor );

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
 * @remarks Unlike `clang_getFileUniqueID()`, this function never fails.
 *
 * @param file The file to get the unique ID for.
 * @return Returns said unique ID.
 */
NODISCARD
CXFileUniqueID tidy_getFileUniqueID( CXFile file );

/**
 * Calls `clang_getSpellingLocation()` and returns the `CXFile`.
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
