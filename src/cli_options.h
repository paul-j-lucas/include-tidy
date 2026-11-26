/*
**      include-tidy -- #include tidier
**      src/cli_options.h
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

#ifndef tidy_cli_options_h
#define tidy_cli_options_h

/**
 * @file
 * Declares macros and functions for command-line options.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "source.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>

// in ascending option character ASCII order; sort using: sort -k3b,3f -k3b,3r
#define OPT_ALIGN_COLUMN          a
#define OPT_ALL_INCLUDES          A
#define OPT_CONFIG                c
#define OPT_COMPILER              C
#define OPT_DIRECTORY             d
#define OPT_DEBUG                 D
#define OPT_ERROR                 e
#define OPT_HELP                  h
#define OPT_INCLUDE               I
#define OPT_COLOR                 k
#define OPT_LINE_LENGTH           l
#define OPT_NO_CONFIG_LAYERS      L
#define OPT_COMMENT_STYLE         s
#define OPT_COMMENT_SYMBOLS       S
#define OPT_VERSION               v
#define OPT_VERBOSE               V

/// @endcond

/**
 * @defgroup cli-options-group Command-Line Options
 * Functions for command-line options.
 * @{
 */

////////// macros /////////////////////////////////////////////////////////////

/// Command-line option character as a character literal.
#define COPT(X)                   CHARIFY(OPT_##X)

/// Command-line option as a string literal.
#define SOPT(X)                   STRINGIFY(OPT_##X)

/**
 * Convenience macro for iterating over all command-line options.
 *
 * @param VAR The `struct option` loop variable.
 */
#define FOREACH_CLI_OPTION(VAR) \
  for ( struct option const *VAR = NULL; (VAR = cli_option_next( VAR )) != NULL; )

////////// extern functions ///////////////////////////////////////////////////

/**
 * Iterates to the next **include-tidy** command-line option.
 *
 * @param option A pointer to the previous option. For the first iteration,
 * NULL should be passed.
 * @return Returns the next command-line option or NULL for none.
 *
 * @note This function isn't normally called directly; use the
 * #FOREACH_CLI_OPTION() macro instead.
 *
 * @sa #FOREACH_CLI_OPTION()
 */
NODISCARD
struct option const* cli_option_next( struct option const *option );

/**
 * Initializes **include-tidy** options from the command-line.
 *
 * @param pargc A pointer to the argument count from \c main().
 * @param pargv A pointer to the argument values from \c main().
 *
 * @note This function must be called exactly once.
 */
tidy_source cli_options_init( int *pargc, char const **pargv[] );

/**
 * Gets the help message for \a short_opt.
 *
 * @param short_opt The option to get the help for.
 * @return Returns said help message.
 */
NODISCARD
char const* option_help( int short_opt );

/**
 * Gets whether the option has been set.
 *
 * @param short_opt The short option character.
 * @return Returns `true` only if the option has been set.
 *
 * @sa option_mark_set()
 */
NODISCARD
bool option_is_set( int short_opt );

/**
 * Marks an option as having been set.
 *
 * @param short_opt The short option character.  It must be alphanumeric.
 *
 * @sa option_is_set()
 */
void option_mark_set( int short_opt );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* tidy_cli_options_h */
/* vim:set et sw=2 ts=2: */
