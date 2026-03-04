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

#ifndef include_tiny_options_H
#define include_tiny_options_H

/**
 * @file
 * Declares global variables and functions for **include-tidy** options.
 */

/**
 * @defgroup options-group Ad Options
 * Global variables and functions for **include-tiny** options.
 * @{
 */

////////// extern variables ///////////////////////////////////////////////////

// TODO

////////// extern functions ///////////////////////////////////////////////////

/**
 * Initializes **include-tiny** options from the command-line.
 *
 * @param argc The argument count from \c main().
 * @param argv The argument values from \c main().
 *
 * @note This function must be called exactly once.
 */
void options_init( int argc, char const *const argv[] );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tiny_options_H */
/* vim:set et sw=2 ts=2: */
