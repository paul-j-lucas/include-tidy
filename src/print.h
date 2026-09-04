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

// libclang
#include <clang-c/Index.h>

// standard
#include <stdbool.h>
#include <stddef.h>                     /* for NULL */

/// @endcond

/**
 * @defgroup printing-group Printing Errors, Warnings, Etc.
 * Functions for printing errors, warnings, and other things.
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * Hash table load factor `printf` format.
 */
#define TIDY_STAT_LF_FMT          "%4.2f"

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
 * @sa #print_file_error()
 * @sa #print_file_warning()
 */
#define print_error(FORMAT, ...)                                \
  fl_print_error( __FILE__, __LINE__,                           \
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
 * @sa #print_file_warning()
 */
#define print_file_error(SOURCE_PATH, SOURCE_LINE, SOURCE_COL, FORMAT, ...) \
  fl_print_error( __FILE__, __LINE__,                                       \
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
 * @sa #print_file_warning()
 */
#define print_warning(FORMAT, ...)          \
  fl_print_warning( __FILE__, __LINE__,     \
    NULL, 0, 0, (FORMAT)                    \
    VA_OPT( (,), __VA_ARGS__ ) __VA_ARGS__  \
  )

/**
 * Prints an error message from libclang.
 *
 * @note In debug mode, also prints the file & line where the function was
 * called from.
 * @note A newline is _not_ printed.
 *
 * @param FORMAT The `printf()` style format string.
 * @param ... The `printf()` arguments.
 */
#define print_libclang_error(FORMAT, ...)                               \
  fl_print_libclang_error(                                              \
    __FILE__, __LINE__, (FORMAT) VA_OPT( (,), __VA_ARGS__ ) __VA_ARGS__ \
  )

/**
 * Prints a cursor's "spelling", kind, and source location, preceded by a label
 * that's the stringification of \a CURSOR.
 *
 * @param CURSOR The cursor to print.
 *
 * @sa #verbose_print_cursor()
 * @sa verbose_print_cursor_impl()
 */
#define VERBOSE_DEBUG_CURSOR(CURSOR) \
  verbose_print_cursor_impl( #CURSOR, (CURSOR) )

/**
 * Prints a cursor's "spelling", kind, and source location.
 *
 * @param CURSOR The cursor to print.
 *
 * @sa #VERBOSE_DEBUG_CURSOR()
 * @sa verbose_print_cursor_impl()
 */
#define verbose_print_cursor(CURSOR) \
  verbose_print_cursor_impl( "", (CURSOR) )

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
 * @param tidy_file The name of the file where this function was called from.
 * @param tidy_line The line number within \a tidy_file where this function was
 * called from.
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
PJL_PRINTF_LIKE_FUNC(6)
void fl_print_error( char const *tidy_file, int tidy_line,
                     char const *source_path, unsigned source_line,
                     unsigned source_col, char const *format, ... );

/**
 * Prints an error message from libclang.
 *
 * @note In debug mode, also prints the file & line where the function was
 * called from.
 * @note A newline is _not_ printed.
 * @note This function isn't normally called directly; use the
 * #print_libclang_error() macro macro instead.
 *
 * @param tidy_file The name of the file where this function was called from.
 * @param tidy_line The line number within \a tidy_file where this function was
 * called from.
 * @param format The `printf()` style format string.
 * @param ... The `printf()` arguments.
 *
 * @sa print_libclang_error()
 */
PJL_PRINTF_LIKE_FUNC(3)
void fl_print_libclang_error( char const *tidy_file, int tidy_line,
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
 * @param tidy_file The name of the file where this function was called from.
 * @param tidy_line The line number within \a tidy_file where this function was
 * called from.
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
void fl_print_warning( char const *tidy_file, int tidy_line,
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
 *
 * @sa #VERBOSE_DEBUG_CURSOR()
 * @sa #verbose_print_cursor()
 */
void verbose_print_cursor_impl( char const *label, CXCursor cursor );

/**
 * Prints the tokens for \a cursor.
 *
 * @param cursor The cursor to print the tokens for.
 */
void verbose_print_tokens( CXCursor cursor );

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

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* tidy_print_h */
/* vim:set et sw=2 ts=2: */
