/*
**      include-tidy -- #include tidier
**      src/path_util.h
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

#ifndef include_tidy_path_util_H
#define include_tidy_path_util_H

/**
 * @file
 * Declares path utility functions.
 */

// local
#include "pjl_config.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <limits.h>                     /* for PATH_MAX */
#include <stdbool.h>
#include <stddef.h>

/// @endcond

/**
 * @defgroup path-util-group Path Utility Functions
 * Path utility functions.
 * @{
 */

////////// extern functions ///////////////////////////////////////////////////

/**
 * Extracts the base portion of a \a path_name.
 * Unlike **basename**(3):
 *  + Trailing `/` characters are not deleted.
 *  + \a path_name is never modified (hence can therefore be `const`).
 *  + Returns a pointer within \a path_name (hence is multi-call safe).
 *
 * @param path_name The path-name to extract the base portion of.
 * @return Returns a pointer to the last component of \a path_name.
 * If \a path_name consists entirely of '/' characters, a pointer to the string
 * "/" is returned.
 */
NODISCARD
char const* path_basename( char const *path_name );

/**
 * Gets the absolute path of the current working directory.
 *
 * @param plen If not NULL, the length of the path is put here.
 * @return Returns the absolute path of the current working directory.  The
 * path is guarenteed to end with `'/'`.
 */
NODISCARD
char const* path_cwd( size_t *plen );

/**
 * Gets whether \a abs_path ends with \a rel_path.
 *
 * @remarks This function ensures that \a rel_path will match only at directory
 * boundaries, i.e., either the character preceding the match in \a abs_path
 * must be <tt>'/'</tt> or \a abs_path equals \a rel_path.
 *
 * @par Examples
 * @parblock
 *
 * `abs_path`                | `rel_path`         | Result
 * ------------------------- | ------------------ | ------
 *  `/var/log/bar/error.log` | `bar/error.log`    | `true`
 *  `/var/log/bar/error.log` | `foobar/error.log` | `false`
 * @endparblock
 *
 * @param abs_path The absolute path to check against.
 * @param rel_path The relative path to check.
 * @param rel_path_len The length of \a rel_path.
 * @return Returns `true` only if \a abs_path ends with \a rel_path at a
 * directory boundary.
 */
NODISCARD
bool path_ends_with( char const *abs_path, char const *rel_path,
                     size_t rel_path_len );

/**
 * Gets the filename extension of \a path, if any.
 *
 * @param path The path to get the filename extension of.
 * @return Returns a pointer into \a path pointing at the first character of
 * the extension (not the dot) or NULL if \a path has no extension.
 *
 * @sa path_no_ext()
 */
NODISCARD
char const* path_ext( char const *path );

/**
 * Gets whether \a path is an absolute path.
 *
 * @param path the path to check.
 * @return Returns `true` only if \a path is absolute.
 *
 * @sa path_is_relative()
 */
NODISCARD
inline bool path_is_absolute( char const *path ) {
  return path[0] == '/';
}

/**
 * Gets whether \a abs_path refers to a local file relative to the current
 * working directory.
 *
 * @par Examples
 * @parblock
 * Assuming the current working directory is `/home/pjl/src/include-tidy/src`:
 *
 * `abs_path`                              | Local?
 * --------------------------------------- | ------
 * `/home/pjl/src/include-tidy/src/util.h` | true
 * `/usr/include/stdio.h`                  | false
 * @endparblock
 *
 * @param abs_path The absolute path of a file.
 * @return Returns `true` only if \a abs_path refers to a local file.
 */
NODISCARD
bool path_is_local( char const *abs_path );

/**
 * Gets whether \a path is a relative path.
 *
 * @param path the path to check.
 * @return Returns `true` only if \a path is relative.
 *
 * @sa path_is_absolute()
 */
NODISCARD
inline bool path_is_relative( char const *path ) {
  return !path_is_absolute( path );
}

/**
 * Strips leading dot-slashes, if any, from \a path.
 *
 * @param path The path to strip `./` from.
 * @return Returns \a path without leading `./`.
 */
char const* path_no_dot_slash( char const *path );

/**
 * Gets the pathname without the filename extension of \a path, if any.
 *
 * @param path The path.
 * @param path_buf A path buffer to use only if \a path has an extension.
 * @return If \a path has no extension, returns \a path as-is; otherwise copies
 * \a path into \a path_buf without the extension and returns \a path_buf.
 *
 * @sa path_ext()
 */
NODISCARD
char const* path_no_ext( char const *path, char path_buf[static PATH_MAX] );

/**
 * Normalizes a path by:
 *
 *  + Making a relative path absolute.
 *  + Eliminating all occurrences of `./` or `../`.
 *
 * @param path The path to normalize.
 * @return Returns a normalized path.  The caller is responsible for freeing
 * it.
 */
char* path_normalize( char const *path );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tidy_path_util_H */
/* vim:set et sw=2 ts=2: */
