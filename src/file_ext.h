/*
**      include-tidy -- #include tidier
**      src/file_ext.h
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
**      along with this program.  If not, see <http
*/

#ifndef tidy_file_ext_h
#define tidy_file_ext_h

/**
 * @file
 * Declares a structure and functions for supported C/C++ filename extensions.
 */

// local
#include "pjl_config.h"                 /* must go first */

// standard
#include <stdbool.h>

/**
 * @defgroup tidy-file-ext-group File Extensions
 * A structure and functions for supported C/C++ filename extensions.
 * @{
 */

/**
 * Convenience macro for iterating over all supported C/C++ file extensions.
 *
 * @param VAR The \ref tidy_file_ext loop variable.
 *
 * @sa file_ext_next()
 */
#define FOREACH_FILE_EXT(VAR) \
  for ( tidy_file_ext const *VAR = NULL; (VAR = file_ext_next( VAR )) != NULL; )

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct tidy_file_ext tidy_file_ext;

////////// structs ////////////////////////////////////////////////////////////

/**
 * Information about supported C/C++ source file extensions.
 */
struct tidy_file_ext {
  char const *ext;                      ///< Extension (without the `'.'`).
  bool        is_header;                ///< Is \ref ext for a header?
  char const *lang;                     ///< Language: either `"c"` or `"c++"`.
};

////////// extern functions ///////////////////////////////////////////////////

/**
 * Attempts to find \a ext among the set of supported C/C++ file extensions.
 *
 * @param ext A filename extension (without the dot).
 * @return Returns the corresponding tidy_file_ext if found or NULL if not.
 */
NODISCARD
tidy_file_ext const* file_ext_find( char const *ext );

/**
 * Iterates to the next C/C++ file extension.
 *
 * @param fe A pointer to the previous tidy_file_ext. For the first iteration,
 * NULL should be passed.
 * @return Returns the next tidy_file_ext or NULL for none.
 *
 * @note This function isn't normally called directly; use the
 * #FOREACH_FILE_EXT() macro instead.
 *
 * @sa #FOREACH_FILE_EXT()
 */
NODISCARD
tidy_file_ext const* file_ext_next( tidy_file_ext const *fe );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* tidy_file_ext_h */
/* vim:set et sw=2 ts=2: */
