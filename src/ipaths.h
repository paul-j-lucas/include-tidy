/*
**      include-tidy -- #include tidier
**      src/ipaths.h
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

#ifndef include_tidy_ipaths_H
#define include_tidy_ipaths_H

/**
 * @file
 * Declares structures and functions for `-I` paths.
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
 * @defgroup tidy-ipaths-group Include Paths
 * Structures and functions for `-I` paths.
 * @{
 */

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct  tidy_ipath  tidy_ipath;

////////// structs ////////////////////////////////////////////////////////////

/**
 * An include path given via the `-I` command-line option.
 */
struct tidy_ipath {
  char   *abs_path;                     ///< The absolute path.
  size_t  abs_path_len;                 ///< Length of \ref abs_path.
};

////////// extern functions ///////////////////////////////////////////////////

/**
 * Adds \a include_path to the global list of include (`-I`) paths.
 *
 * @param include_path The include path to add.
 */
void ipath_add( char const *include_path );

/**
 * Attempts to find \a rel_path among the include paths.
 *
 * @param rel_path The relative path to find.
 * @param abs_path If found, the absolute path of \a rel_path is copied here.
 * @return Returns `true` only if \a rel_path is found.
 */
NODISCARD
bool ipath_find( char const *rel_path, char abs_path[static PATH_MAX] );

/**
 * Relativizes \a abs_path against one of the `-I` absolute paths.
 *
 * @par Example
 * If the option `-I/opt/local/libexec/llvm-21/include` were given and \a
 * abs_path were `/opt/local/libexec/llvm-21/include/clang-c/Index.h`, then
 * this function would return `clang-c/Index.h`.
 *
 * @param abs_path The absolute path of a file being included.
 * @return Returns the shortened path of \a abs_path relative to one of the
 * `-I` absolute paths.
 *
 * @note The pointer returned points to within \a abs_path.
 */
NODISCARD
char const* ipath_relativize( char const *abs_path );

/**
 * Initializes the list of `-I` paths.
 */
void ipaths_init( void );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tidy_ipaths_H */
/* vim:set et sw=2 ts=2: */
