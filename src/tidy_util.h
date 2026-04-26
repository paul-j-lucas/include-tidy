/*
**      include-tidy -- #include tidier
**      src/tidy_util.h
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

#ifndef include_tidy_tidy_util_H
#define include_tidy_tidy_util_H

/**
 * @file
 * Declares miscellanous stuff.
 */

// local
#include "pjl_config.h"                 /* must go first */

/**
 * @defgroup tidy-util-group Include Tidy Utility
 * Miscellanous **include-tidy**-specific types, constants, and functions.
 * @{
 */

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct ext_lang_map ext_lang_map;

////////// structures /////////////////////////////////////////////////////////

/**
 * Mapping from source file extension to programming language, either C or C++.
 */
struct ext_lang_map {
  char const *ext;                      ///< Extension (without the `'.'`).
  char const *lang;                     ///< Language: either `"c"` or `"c++"`.
};

////////// extern constants ///////////////////////////////////////////////////

/**
 * Mapping of all common C and C++ filename extensions to programming language.
 *
 * @remarks The last element has NULL values.
 */
extern ext_lang_map const EXT_LANG_MAP[];

////////// extern functions ///////////////////////////////////////////////////

/**
 * Gets the language from \a ext.
 *
 * @param ext A filename extension (without the dot).
 * @return Returns either `"c"` (for C) or `"c++"` (for C++).
 */
NODISCARD
char const* get_ext_language( char const *ext );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tidy_tidy_util_H */
/* vim:set et sw=2 ts=2: */
