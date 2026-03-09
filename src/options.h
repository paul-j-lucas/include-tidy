/*
**      include-tidy -- #include tidier
**      src/options.h
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

#ifndef include_tidy_options_H
#define include_tidy_options_H

/**
 * @file
 * Declares global variables and functions for **include-tidy** options.
 */

/**
 * @defgroup options-group Ad Options
 * Global variables and functions for **include-tidy** options.
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

// extern option variables
extern char const *opt_comments[2];     ///< Comment delimiters to use.

////////// extern functions ///////////////////////////////////////////////////

/**
 * TODO.
 *
 * @param included_path The path being included.
 * @return Returns TODO.
 */
char const* include_resolve( char const *included_path );

/**
 * Initializes **include-tidy** options from the command-line.
 *
 * @param pargc A pointer to the argument count from \c main().
 * @param pargv A pointer to the argument values from \c main().
 *
 * @note This function must be called exactly once.
 */
void options_init( int *pargc, char const **pargv[] );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tidy_options_H */
/* vim:set et sw=2 ts=2: */
