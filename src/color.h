/*
**      include-tidy -- #include tidier
**      src/color.h
**
**      Copyright (C) 2017-2026  Paul J. Lucas
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

#ifndef include_tidy_color_H
#define include_tidy_color_H

/**
 * @file
 * Declares constants, macros, types, global variables, and functions for
 * printing to an ANSI terminal in color using [Select Graphics Rendition
 * (SGR)](https://en.wikipedia.org/wiki/ANSI_escape_code#SGR) codes.
 */

// local
#include "pjl_config.h"                 /* must go first */

/// @cond DOXYGEN_IGNORE

// standard
#include <stdio.h>

/// @endcond

/**
 * @defgroup printing-color-group Printing Color
 * Constants, macros, types, global variables, and functions for printing to an
 * ANSI terminal in color using [Select Graphics Rendition
 * (SGR)](https://en.wikipedia.org/wiki/ANSI_escape_code#SGR) codes.
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * When to colorize output.
 */
enum color_when {
  COLOR_NEVER,                          ///< Never colorize.
  COLOR_ISATTY,                         ///< Colorize only if **isatty**(3).
  COLOR_NOT_FILE,                       ///< Colorize only if `!ISREG` stdout.
  COLOR_ALWAYS                          ///< Always colorize.
};
typedef enum color_when color_when;

// extern variables
extern char const  *sgr_caret;          ///< Color of the caret `^`.
extern char const  *sgr_error;          ///< Color of `error`.
extern char const  *sgr_include_add;    ///< Color for `#include` to add.
extern char const  *sgr_include_del;    ///< Color for `#include` to delete.
extern char const  *sgr_locus;          ///< Color of error location.
extern char const  *sgr_warning;        ///< Color of `warning`.

////////// extern functions ///////////////////////////////////////////////////

/**
 * Ends printing in \a sgr_color.
 *
 * @param file The `FILE` to print to.
 * @param sgr_color The predefined color.  If NULL, does nothing.  This _must_
 * be the same value that was passed to color_start().
 *
 * @sa color_start()
 */
void color_end( FILE *file, char const *sgr_color );

/**
 * Starts printing in the predefined \a sgr_color.
 *
 * @param file The `FILE` to print to.
 * @param sgr_color The predefined color.  If NULL, does nothing.
 *
 * @sa color_end()
 */
void color_start( FILE *file, char const *sgr_color );

/**
 * Initializes when to print in color and the colors.
 *
 * @note This function must be called exactly once.
 */
void colors_init( void );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tidy_color_H */
/* vim:set et sw=2 ts=2: */
