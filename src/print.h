/*
**      include-tidy -- #include tidier
**      src/print.h
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

#ifndef include_tidy_print_H
#define include_tidy_print_H

/**
 * @file
 * Declares functions for printing error and warning messages.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "util.h"

/**
 * @defgroup printing-errors-warnings-group Printing Errors & Warnings
 * Functions for printing errors and warning messages.
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * Prints an error message to standard error.
 *
 * @note In debug mode, also prints the file & line where the function was
 * called from.
 * @note A newline is _not_ printed.
 *
 * @param SOURCE_PATH The source file's path.
 * @param SOURCE_LINE The source file's error line.
 * @param SOURCE_COL The source file's error column.
 * @param FORMAT The `printf()` style format string.
 * @param ... The `printf()` arguments.
 *
 * @sa fl_print_error()
 * @sa #print_warning()
 */
#define print_error(SOURCE_PATH, SOURCE_LINE, SOURCE_COL, FORMAT, ...)  \
  fl_print_error( __FILE__, __LINE__,                                   \
    (SOURCE_PATH), (SOURCE_LINE), (SOURCE_COL), FORMAT                  \
    VA_OPT( (,), __VA_ARGS__ ) __VA_ARGS__                              \
  )

/**
 * Prints an warning message to standard error.
 *
 * @note In debug mode, also prints the file & line where the function was
 * called from.
 * @note A newline is _not_ printed.
 *
 * @param SOURCE_PATH The source file's path.
 * @param SOURCE_LINE The source file's error line.
 * @param SOURCE_COL The source file's error column.
 * @param FORMAT The `printf()` style format string.
 * @param ... The `printf()` arguments.
 *
 * @sa fl_print_warning()
 * @sa #print_error()
 */
#define print_warning(SOURCE_PATH, SOURCE_LINE, SOURCE_COL, FORMAT, ...)  \
  fl_print_warning( __FILE__, __LINE__,                                   \
    (SOURCE_PATH), (SOURCE_LINE), (SOURCE_COL), FORMAT                    \
    VA_OPT( (,), __VA_ARGS__ ) __VA_ARGS__                                \
  )

////////// extern functions ///////////////////////////////////////////////////

/**
 * Prints an error message to standard error.
 *
 * @note In debug mode, also prints the file & line where the function was
 * called from.
 * @note A newline is _not_ printed.
 * @note This function isn't normally called directly; use the #print_error()
 * macro instead.
 *
 * @param tidy_file The name of the file where this function was called from.
 * @param tidy_line The line number within \a tidy_file where this function was
 * called from.
 * @param source_path The source file's path.
 * @param source_line The source file's error line.
 * @param source_col The source file's error column.
 * @param format The `printf()` style format string.
 * @param ... The `printf()` arguments.
 *
 * @sa fl_print_warning()
 * @sa #print_error()
 */
PJL_PRINTF_LIKE_FUNC(6)
void fl_print_error( char const *tidy_file, int tidy_line,
                     char const *source_path, unsigned source_line,
                     unsigned source_col, char const *format, ... );

/**
 * Prints a warning message to standard error.
 *
 * @note In debug mode, also prints the file & line where the function was
 * called from.
 * @note A newline is _not_ printed.
 * @note This function isn't normally called directly; use the #print_warning()
 * macro instead.
 *
 * @param tidy_file The name of the file where this function was called from.
 * @param tidy_line The line number within \a tidy_file where this function was
 * called from.
 * @param source_path The source file's path.
 * @param source_line The source file's error line.
 * @param source_col The source file's error column.
 * @param format The `printf()` style format string.
 * @param ... The `printf()` arguments.
 *
 * @sa fl_print_error()
 * @sa #print_warning()
 */
PJL_PRINTF_LIKE_FUNC(6)
void fl_print_warning( char const *tidy_file, int tidy_line,
                       char const *source_path, unsigned source_line,
                       unsigned source_col, char const *format, ... );

/**
 * Prints verbose output.
 *
 * @remarks If \ref opt_verbose is #TIDY_VERBOSE_NONE, this function does
 * nothing.
 *
 * @param format The `printf()` format string literal to use.
 * @param ... The `printf()` arguments.
 * @return Returns the number of characters printed.
 */
PJL_DISCARD
PJL_PRINTF_LIKE_FUNC(1)
int verbose_printf( char const *format, ... );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tidy_print_H */
/* vim:set et sw=2 ts=2: */
