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

// local
#include "pjl_config.h"                 /* must go first */

/// @cond DOXYGEN_IGNORE

// standard
#include <getopt.h>

/// @endcond

/**
 * @defgroup options-group Ad Options
 * Global variables and functions for **include-tiny** options.
 * @{
 */

/**
 * Convenience macro for iterating over all **include-tiny** command-line
 * options.
 *
 * @param VAR The `struct option` loop variable.
 *
 * @sa cli_option_next()
 */
#define FOREACH_CLI_OPTION(VAR) \
  for ( struct option const *VAR = NULL; (VAR = cli_option_next( VAR )) != NULL; )

////////// extern variables ///////////////////////////////////////////////////

// TODO

////////// extern functions ///////////////////////////////////////////////////

/**
 * Iterates to the next **include-tiny** command-line option.
 *
 * @param opt A pointer to the previous option. For the first iteration, NULL
 * should be passed.
 * @return Returns the next command-line option or NULL for none.
 *
 * @note This function isn't normally called directly; use the
 * #FOREACH_CLI_OPTION() macro instead.
 *
 * @sa #FOREACH_CLI_OPTION()
 */
NODISCARD
struct option const* cli_option_next( struct option const *opt );

/**
 * Initializes **include-tiny** options from the command-line.
 *
 * @param argc The argument count from \c main().
 * @param argv The argument values from \c main().
 *
 * @note This function must be called exactly once.
 */
void options_init( int argc, char const *argv[] );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tiny_options_H */
/* vim:set et sw=2 ts=2: */
