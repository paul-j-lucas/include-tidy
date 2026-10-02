/*
**      include-tidy -- #include tidier
**      src/verbose.h
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

#ifndef tidy_verbose_h
#define tidy_verbose_h

/**
 * @file
 * Declares macros and functions for printing verbose output.
 */

// local
#include "pjl_config.h"                 /* must go first */

/// @cond DOXYGEN_IGNORE

// libclang
#include <clang-c/Index.h>

// standard
#include <stdbool.h>

/// @endcond

/**
 * @defgroup verbose-group Verbose printing
 * Macros and functions for printing verbose output.
 * @{
 */

////////// macros /////////////////////////////////////////////////////////////

/**
 * Prints a cursor's "spelling", kind, and source location, preceded by a label
 * that's the stringification of \a CURSOR.
 *
 * @param CURSOR The cursor to print.
 * @param LANG The language of the source file being tidied.
 *
 * @sa #verbose_print_cursor()
 * @sa verbose_print_cursor_impl()
 */
#define VERBOSE_DEBUG_CURSOR(CURSOR, LANG) \
  verbose_print_cursor_impl( #CURSOR, (CURSOR), (LANG) )

/**
 * Prints a cursor's "spelling", kind, and source location.
 *
 * @param CURSOR The cursor to print.
 * @param LANG The language of the source file being tidied.
 *
 * @sa #VERBOSE_DEBUG_CURSOR()
 * @sa verbose_print_cursor_impl()
 */
#define verbose_print_cursor(CURSOR, LANG) \
  verbose_print_cursor_impl( "", (CURSOR), (LANG) )

////////// extern functions ///////////////////////////////////////////////////

/**
 * Prints each value of \a argv preceded by its index.
 *
 * @note verbose_section_begin() is called implicitly.
 *
 * @param label A label to print before the word `argv`.  A space is printed
 * after the label.
 * @param argc The argument count of \a argv.
 * @param argv The command-line argument values.
 */
void verbose_print_argv( char const *label, int argc,
                         char const *const argv[] );

/**
 * Prints a cursor's "spelling", kind, and source location.
 *
 * @note This function isn't normally called directly; use either the
 * #verbose_print_cursor() or #VERBOSE_DEBUG_CURSOR() macro instead.
 *
 * @param label A label to print before the cursor.  May be either NULL or the
 * empty string for none.  If neither, prints a space after the label.
 * @param cursor The cursor to print.
 * @param lang The language of the source file being tidied.
 *
 * @sa #VERBOSE_DEBUG_CURSOR()
 * @sa #verbose_print_cursor()
 */
void verbose_print_cursor_impl( char const *label, CXCursor cursor,
                                enum CXLanguageKind lang );

#ifndef NDEBUG
/**
 * Prints the tokens for \a cursor.
 *
 * @param cursor The cursor to print the tokens for.
 */
void verbose_print_tokens( CXCursor cursor );
#endif /* NDEBUG */

/**
 * Prints output preceeded by `"// tidy | "`.
 *
 * @param format The `printf()` format string literal to use.
 * @param ... The `printf()` arguments.
 * @return Returns the number of characters printed.
 */
PJL_DISCARD
PJL_PRINTF_LIKE_FUNC(1)
int verbose_printf( char const *format, ... );

/**
 * Gets whether statistics should be printed.
 *
 * @remarks If so, the `statistics:` header is printed only the first time this
 * function is called.
 *
 * @return Returns `true` only if statistics should be printed.
 */
NODISCARD
bool verbose_print_statistics( void );

/**
 * This should be called once just before starting to print a new verbose
 * output section to print a blank line to separate sections if necessary.
 *
 * @param printed_header If not NULL, a pointer to flag to be tested and, if
 * `false`, sets it to `true`.
 * @return Returns `true` only if \a printed_header is NULL or \a
 * *printed_header was `false` initially.
 */
PJL_DISCARD
bool verbose_section_begin( bool *printed_header );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* tidy_verbose_h */
/* vim:set et sw=2 ts=2: */
