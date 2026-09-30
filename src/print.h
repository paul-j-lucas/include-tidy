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

#ifndef tidy_print_h
#define tidy_print_h

/**
 * @file
 * Declares functions for printing error, warnings, and other things.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <stddef.h>                     /* for NULL */

/// @endcond

/**
 * @defgroup printing-group Printing Errors, Warnings, Etc.
 * Functions for printing errors, warnings, and other things.
 * @{
 */

////////// macros /////////////////////////////////////////////////////////////

/**
 * Prints an error message to standard error.
 *
 * @note In debug mode, also prints the file & line where the function was
 * called from.
 * @note A newline is _not_ printed.
 *
 * @param FORMAT The `printf()` style format string.
 * @param ... The `printf()` arguments.
 *
 * @sa fl_print_error()
 * @sa #print_error_from()
 * @sa #print_file_error()
 * @sa #print_file_warning()
 */
#define print_error(FORMAT, ...)                                \
  fl_print_error( __FILE__, __LINE__, /*origin=*/NULL,          \
    NULL, 0, 0, (FORMAT) VA_OPT( (,), __VA_ARGS__ ) __VA_ARGS__ \
  )

/**
 * Prints an error message from \a ORIGIN.
 *
 * @note In debug mode, also prints the file & line where the function was
 * called from.
 * @note A newline is _not_ printed.
 *
 * @param ORIGIN Message origin.
 * @param FORMAT The `printf()` style format string.
 * @param ... The `printf()` arguments.
 *
 * @sa #print_error()
 * @sa fl_print_error()
 */
#define print_error_from(ORIGIN, FORMAT, ...)                   \
  fl_print_error( __FILE__, __LINE__, (ORIGIN),                 \
    NULL, 0, 0, (FORMAT) VA_OPT( (,), __VA_ARGS__ ) __VA_ARGS__ \
  )

/**
 * Prints an error message about \a SOURCE_PATH to standard error.
 *
 * @note In debug mode, also prints the file & line where the function was
 * called from.
 * @note A newline is _not_ printed.
 *
 * @param SOURCE_PATH The source file's path or NULL for none.
 * @param SOURCE_LINE The source file's error line or zero for none.
 * @param SOURCE_COL The source file's error column or zero for none.
 * @param FORMAT The `printf()` style format string.
 * @param ... The `printf()` arguments.
 *
 * @sa fl_print_error()
 * @sa #print_error()
 * @sa #print_error_from()
 * @sa #print_file_warning()
 */
#define print_file_error(SOURCE_PATH, SOURCE_LINE, SOURCE_COL, FORMAT, ...) \
  fl_print_error( __FILE__, __LINE__, /*origin=*/NULL,                      \
    (SOURCE_PATH), (SOURCE_LINE), (SOURCE_COL), (FORMAT)                    \
    VA_OPT( (,), __VA_ARGS__ ) __VA_ARGS__                                  \
  )

/**
 * Prints an warning message about \a SOURCE_PATH to standard error.
 *
 * @note In debug mode, also prints the file & line where the function was
 * called from.
 * @note A newline is _not_ printed.
 *
 * @param SOURCE_PATH The source file's path or NULL for none.
 * @param SOURCE_LINE The source file's error line or zero for none.
 * @param SOURCE_COL The source file's error column or zero for none.
 * @param FORMAT The `printf()` style format string.
 * @param ... The `printf()` arguments.
 *
 * @sa fl_print_warning()
 * @sa #print_error()
 * @sa #print_error_from()
 * @sa #print_file_error()
 * @sa #print_warning()
 */
#define print_file_warning(SOURCE_PATH, SOURCE_LINE, SOURCE_COL, FORMAT, ...) \
  fl_print_warning( __FILE__, __LINE__,                                       \
    (SOURCE_PATH), (SOURCE_LINE), (SOURCE_COL), (FORMAT)                      \
    VA_OPT( (,), __VA_ARGS__ ) __VA_ARGS__                                    \
  )

/**
 * Prints an warning message to standard error.
 *
 * @note In debug mode, also prints the file & line where the function was
 * called from.
 * @note A newline is _not_ printed.
 *
 * @param FORMAT The `printf()` style format string.
 * @param ... The `printf()` arguments.
 *
 * @sa fl_print_warning()
 * @sa #print_error()
 * @sa #print_error_from()
 * @sa #print_file_warning()
 */
#define print_warning(FORMAT, ...)          \
  fl_print_warning( __FILE__, __LINE__,     \
    NULL, 0, 0, (FORMAT)                    \
    VA_OPT( (,), __VA_ARGS__ ) __VA_ARGS__  \
  )

////////// extern functions ///////////////////////////////////////////////////

/**
 * Prints an error message to standard error.
 *
 * @note In debug mode, also prints the file & line where the function was
 * called from.
 * @note A newline is _not_ printed.
 * @note This function isn't normally called directly; use the #print_error()
 * or #print_file_error() macros instead.
 *
 * @param caller_file The name of the file where this function was called from.
 * @param caller_line The line number within \a caller_file where this function
 * was called from.
 * @param origin Message origin, if any.
 * @param source_path The source file's path or NULL for none.
 * @param source_line The source file's error line or zero for none.
 * @param source_col The source file's error column or zero for none.
 * @param format The `printf()` style format string.
 * @param ... The `printf()` arguments.
 *
 * @sa fl_print_warning()
 * @sa #print_error()
 * @sa #print_file_error()
 */
PJL_PRINTF_LIKE_FUNC(7)
void fl_print_error( char const *caller_file, int caller_line,
                     char const *origin, char const *source_path,
                     unsigned source_line, unsigned source_col,
                     char const *format, ... );

/**
 * Prints a warning message to standard error.
 *
 * @note In debug mode, also prints the file & line where the function was
 * called from.
 * @note A newline is _not_ printed.
 * @note This function isn't normally called directly; use the
 * #print_file_warning() macro instead.
 *
 * @param caller_file The name of the file where this function was called from.
 * @param caller_line The line number within \a caller_file where this function
 * was called from.
 * @param source_path The source file's path or NULL for none.
 * @param source_line The source file's error line or zero for none.
 * @param source_col The source file's error column or zero for none.
 * @param format The `printf()` style format string.
 * @param ... The `printf()` arguments.
 *
 * @sa fl_print_error()
 * @sa #print_file_warning()
 */
PJL_PRINTF_LIKE_FUNC(6)
void fl_print_warning( char const *caller_file, int caller_line,
                       char const *source_path, unsigned source_line,
                       unsigned source_col, char const *format, ... );

/**
 * Prints an `#include` preprocessor directive.
 *
 * @param sgr_color The SGR color to use, if any.
 * @param delims The include delimiters.
 * @param rel_path The include's relative path.
 * @param comment The comment, if any.
 */
void print_include( char const *sgr_color, char const delims[static 2],
                    char const *rel_path, char const *comment );

/**
 * Prints the given \a line of \a path, presumably where an error occurred,
 * followed by a line with a `^` at \a col.
 *
 * @param path The file's path.
 * @param line The line to print.
 * @param col The column to print.
 * @param offset The offset within \a path of the error.
 */
void print_source_line( char const *path, unsigned line, unsigned col,
                        unsigned offset );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* tidy_print_h */
/* vim:set et sw=2 ts=2: */
