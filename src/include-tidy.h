/*
**      include-tidy -- #include tidier
**      src/include-tidy.h
**
**      Copyright (C) 2026  Paul J. Lucas, et al.
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

#ifndef include_tidy_include_tidy_H
#define include_tidy_include_tidy_H

/**
 * @file
 * Declares miscellaneous macros, global variables, and functions.
 */

// local
#include "pjl_config.h"                 /* IWYU pragma: keep */

/**
 * @defgroup misc-globals Miscellaneous Globals
 * Miscellaneous global macros, variables, and functions.
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * Program name when composing or deciphering C.
 */
#define INCLUDE_TIDY                    PACKAGE

/**
 * **cdecl** latest copyright year.
 */
#define INCLUDE_TIDY_COPYRIGHT_YEAR     "2026"

/**
 * **cdecl** license.
 *
 * @sa #INCLUDE_TIDY_LICENSE_URL
 */
#define INCLUDE_TIDY_LICENSE            "GPLv3+: GNU GPL version 3 or later"

/**
 * **cdecl** license URL.
 *
 * @sa #INCLUDE_TIDY_LICENSE
 */
#define INCLUDE_TIDY_LICENSE_URL        "https://gnu.org/licenses/gpl.html"

/**
 * **cdecl** primary author.
 */
#define INCLUDE_TIDY_AUTHOR             "Paul J. Lucas"

///////////////////////////////////////////////////////////////////////////////

// extern variables
extern char const  *prog_name;            ///< Program name.
extern char const  *tidy_source_path;     ///< The file being tidied.

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tidy_include_tidy_H */
/* vim:set et sw=2 ts=2: */
