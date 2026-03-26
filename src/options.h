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

// local
#include "pjl_config.h"                 /* must go first */

// standard
#include <stdbool.h>

/**
 * @defgroup options-group **include-tidy** Options
 * Global variables and functions for **include-tidy** options.
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * Verbose mode.
 */
enum tidy_verbose {
  TIDY_VERBOSE_NONE,                    ///< Don't be verbose.
  TIDY_VERBOSE_ARGS     = 1 << 0,       ///< Command-line arguments.
  TIDY_VERBOSE_INCLUDES = 1 << 1,       ///< Files includes.
  TIDY_VERBOSE_SYMBOLS  = 1 << 2,       ///< Symbols referenced.
};
typedef enum tidy_verbose tidy_verbose;

// extern option variables
extern bool         opt_all_includes;     ///< Print all includes?
extern unsigned     opt_comment_align;    ///< Comment alignment column.
extern char const  *opt_comment_style[2]; ///< Comment delimiters to use.
extern char const  *opt_config_path;      ///< Configuration file path.
extern unsigned     opt_line_length;      ///< Line length.
extern tidy_verbose opt_verbose;          ///< Print verbose output?

// extern argument variables
extern char const  *arg_source_path;      ///< The file being tidied.

#define OPT_CLANG_DEFAULT         "clang" /**< Default `clang` path. */
#define OPT_COMMENT_ALIGN_DEFAULT 41      /**< Default column alignment. */
#define OPT_COMMENT_ALIGN_MAX     256     /**< Maximum column alignment. */
#define OPT_LINE_LENGTH_DEFAULT   80      /**< Default line length. */
#define OPT_LINE_LENGTH_MAX       256     /**< Maximum line length. */

////////// extern functions ///////////////////////////////////////////////////

/**
 * Adds \a include_path to the global list of include (`-I`) paths.
 *
 * @param include_path The include path to add.
 */
void opt_include_paths_add( char const *include_path );

/**
 * Relativizes \a abs_path against one of the `-I` absolute paths.
 *
 * @param abs_path The absolte path of a file being included.
 * @return Returns the shorted path of \a abs_path relative to one of the `-I`
 * absolute paths.
 */
char const* opt_include_paths_relativize( char const *abs_path );

/**
 * Initializes **include-tidy** options.
 *
 * @note This function must be called exactly once.
 */
void options_init( void );

/**
 * Parses the alignment column number for comments.
 *
 * @param s The string to parse.
 * @return Returns `true` only if \a s was parsed successfully.
 */
NODISCARD
bool parse_comment_alignment( char const *s );

/**
 * Parses a comment delimiter.
 *
 * @param delim_format The comment delimiter to parse.
 * @return Returns `true` only if \a delim_format parsed successfully.
 */
NODISCARD
bool parse_comment_style( char const *delim_format );

/**
 * Parses the line length.
 *
 * @param s The string to parse.
 * @return Returns the line length.
 */
NODISCARD
bool parse_line_length( char const *s );

/**
 * Parses the value of the **include-tidy** verbose option.
 *
 * @param verbose_format
 * @parblock
 * The null-terminated **include-tidy** debug format string to parse.
 * Value format are:
 *
 * Format | Meaning
 * -------|-----------------------------------------------------------------
 * `a`    | Be verbose about command-line arguments.
 * `i`    | Be verbose about include files.
 * `s`    | Be verbose about symbols referenced.
 *
 * Multiple formats may be given, one immediately after the other, e.g., `ai`.
 * Alternatively, `*` may be given to mean "all" or either the empty string or
 * `-` may be given to mean "none."
 * @endparblock
 * @return Returns the parsed value.
 */
NODISCARD
bool parse_tidy_verbose( char const *verbose_format );

/**
 * Prints verbose output.
 *
 * @param format The `printf()` format string literal to use.
 * @param ... The `printf()` arguments.
 * @return Returns the number of characters printed.
 */
PJL_PRINTF_LIKE_FUNC(1)
int verbose_printf( char const *format, ... );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tidy_options_H */
/* vim:set et sw=2 ts=2: */
