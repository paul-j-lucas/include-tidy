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

// local
#include "pjl_config.h"

/// @cond DOXYGEN_IGNORE

// libclang
#include <clang-c/Index.h>

// standard
#include <stdio.h>                      /* for FILE */
#include <string.h>                     /* for memcmp(3) */

/// @endcond

/**
 * @defgroup clang-util-group libclang Utility Functions
 * Utility functions for libclang.
 * @{
 */

////////// extern functions ///////////////////////////////////////////////////

/**
 * Compares two CXFile objects by name.
 *
 * @param i_file The first CXFile.
 * @param j_file The second CXFile.
 * @return Returns a number less than 0, 0, or greater than 0 if the name of \a
 * i_file is less than, equal to, or greater than the name of \a j_file,
 * respectively.
 *
 * @sa tidy_CXFileUniqueID_cmp()
 */
NODISCARD
int tidy_CXFile_cmp_by_name( CXFile i_file, CXFile j_file );

/**
 * Compares two CXFileUniqueID objects.
 *
 * @param i_id The first CXFileUniqueID.
 * @param j_id The second CXFileUniqueID.
 * @return Returns a number less than 0, 0, or greater than 0 if \a i_id is
 * less than, equal to, or greater than \a j_id, respectively.
 *
 * @sa tidy_CXFile_cmp_by_name()
 */
NODISCARD
inline int tidy_CXFileUniqueID_cmp( CXFileUniqueID const *i_id,
                                    CXFileUniqueID const *j_id ) {
  return memcmp( i_id, j_id, sizeof *i_id );
}

/**
 * Prints \a id as a hexadecimal ineger.
 *
 * @param id The CXFileUniqueID to print.
 * @param out The `FILE` to print to.
 */
void tidy_CXFileUniqueID_fput( CXFileUniqueID const *id, FILE *out );

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
 * Calls `clang_getCursorSpelling()` and `clang_getCString()`.
 *
 * @param cursor The cursor to use.
 * @return Returns the C string version of \a cursor.  The caller is
 * responsible for freeing it.
 */
NODISCARD
char* tidy_getCursorSpelling( CXCursor cursor );

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
 * @param loc The `CXSourceLocation` to use.
 * @return Returns its `CXFile`.
 *
 * @sa tidy_getFileLocation_File()
 */
NODISCARD
CXFile tidy_getSpellingLocation_File( CXSourceLocation loc );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tidy_clang_util_H */
/* vim:set et sw=2 ts=2: */
