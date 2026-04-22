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
 * Declares types, global variables, and functions for **include-tidy**
 * options.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "color.h"

// standard
#include <stdbool.h>

/**
 * @defgroup options-group **include-tidy** Options
 * Types, global variables, and functions for **include-tidy** options.
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * When to exit with a non-zero status code.
 */
enum tidy_error {
  TIDY_ERROR_IF_VIOLATIONS,             ///< Non-zero if violations.
  TIDY_ERROR_ALWAYS,                    ///< Always exit with non-zero.
  TIDY_ERROR_NEVER                      ///< Always exit with zero.
};
typedef enum tidy_error tidy_error;

/**
 * Verbose mode.
 */
enum tidy_verbose {
  TIDY_VERBOSE_NONE,                      ///< Don't be verbose.
  TIDY_VERBOSE_ARGS             = 1 << 0, ///< Command-line arguments.
  TIDY_VERBOSE_CONFIG_FILES     = 1 << 1, ///< Configuration files.
  TIDY_VERBOSE_CONFIG_SYMBOLS   = 1 << 2, ///< Configuration symbols.
  TIDY_VERBOSE_CURSORS          = 1 << 3, ///< Libclang cursors.
  TIDY_VERBOSE_DIRECTORY        = 1 << 4, ///< Changing directory.
  TIDY_VERBOSE_INCLUDES         = 1 << 5, ///< Files included.
  TIDY_VERBOSE_PROXIES_EXPLICIT = 1 << 6, ///< Explicit include proxies.
  TIDY_VERBOSE_PROXIES_IMPLICIT = 1 << 7, ///< Implicit include proxies.
  TIDY_VERBOSE_SOURCE_FILE      = 1 << 8, ///< Source file.
  TIDY_VERBOSE_SYMBOLS          = 1 << 9, ///< Symbols referenced.
};
typedef enum tidy_verbose tidy_verbose;

/**
 * Shorthand for either #TIDY_VERBOSE_PROXIES_EXPLICIT or
 * #TIDY_VERBOSE_PROXIES_IMPLICIT.
 */
#define TIDY_VERBOSE_PROXIES_EITHER \
  ( TIDY_VERBOSE_PROXIES_EXPLICIT | TIDY_VERBOSE_PROXIES_IMPLICIT )

// extern option variables
extern unsigned     opt_align_column;     ///< Comment alignment column.
extern bool         opt_all_includes;     ///< Print all includes?
extern color_when   opt_color_when;       ///< When to colorize.
extern char const  *opt_comment_style[2]; ///< Comment delimiters to use.
extern bool         opt_config_layers;    ///< Do configuration file layering?
extern char const  *opt_config_path;      ///< Configuration file path.
extern tidy_error   opt_error;            ///< When to exit with non-zero.
extern unsigned     opt_line_length;      ///< Line length.
extern tidy_verbose opt_verbose;          ///< Print verbose output?

// extern argument variables
extern char const  *arg_source_path;      ///< The file being tidied.

#define OPT_ALIGN_COLUMN_DEFAULT  41      /**< Default column alignment. */
#define OPT_ALIGN_COLUMN_MAX      256     /**< Maximum column alignment. */
#define OPT_CLANG_DEFAULT         "clang" /**< Default `clang` path. */
#define OPT_LINE_LENGTH_DEFAULT   80      /**< Default line length. */
#define OPT_LINE_LENGTH_MAX       256     /**< Maximum line length. */

////////// extern functions ///////////////////////////////////////////////////

/**
 * Parses the alignment column number for comments.
 *
 * @param s The string to parse.
 * @return Returns `true` only if \a s was parsed successfully.
 */
NODISCARD
bool opt_align_column_parse( char const *s );

/**
 * Parses when to colorize.
 *
 * @param s The string to parse.
 * @return Returns `true` only if \a s was parsed successfully.
 */
NODISCARD
bool opt_color_parse( char const *s );

/**
 * Parses the comment style.
 *
 * @param s The comment style to parse.
 * @return Returns `true` only if \a s parsed successfully.
 */
NODISCARD
bool opt_comment_style_parse( char const *s );

/**
 * Parses when to exit with a non-zero exit status.
 *
 * @param s The error to parse.
 * @return Returns `true` only if \a s parsed successfully.
 */
NODISCARD
bool opt_error_parse( char const *s );

/**
 * Adds \a include_path to the global list of include (`-I`) paths.
 *
 * @param include_path The include path to add.
 */
void opt_include_paths_add( char const *include_path );

/**
 * Relativizes \a abs_path against one of the `-I` absolute paths.
 *
 * @par Example
 * If the option `-I/opt/local/libexec/llvm-21/include` were given and \a
 * abs_path were `/opt/local/libexec/llvm-21/include/clang-c/Index.h`, then
 * this function would return `clang-c/Index.h`.
 *
 * @param abs_path The absolte path of a file being included.
 * @return Returns the shortened path of \a abs_path relative to one of the
 * `-I` absolute paths.
 *
 * @note The pointer returned points to within \a abs_path.
 */
NODISCARD
char const* opt_include_paths_relativize( char const *abs_path );

/**
 * Gets whether the option has been set.
 *
 * @param short_opt The short option character.
 * @return Returns `true` only if the option has been set.
 *
 * @sa opt_mark_set()
 */
NODISCARD
bool opt_is_set( int short_opt );

/**
 * Parses the line length.
 *
 * @param s The string to parse.
 * @return Returns the line length.
 */
NODISCARD
bool opt_line_length_parse( char const *s );

/**
 * Marks an option as having been set.
 *
 * @param short_opt The short option character.
 *
 * @sa opt_is_set()
 */
void opt_mark_set( int short_opt );

/**
 * Parses the value of the **include-tidy** verbose option.
 *
 * @param verbose_format
 * @parblock
 * The null-terminated **include-tidy** verbose format string to parse.
 * Value format are:
 *
 * Format | Be verbose about ...
 * -------|-----------------------------------------------------------------
 * `a`    | Command-line arguments.
 * `i`    | Files included.
 * `P`    | Connfiration file proxies.
 * `s`    | Symbols referenced.
 * `S`    | Configuration file symbols.
 *
 * Multiple formats may be given, one immediately after the other, e.g., `ai`.
 * Alternatively, `*` may be given to mean "all" or either the empty string or
 * `-` may be given to mean "none."
 * @endparblock
 * @return Returns the parsed value.
 */
NODISCARD
bool opt_verbose_parse( char const *verbose_format );

/**
 * Initializes **include-tidy** options.
 *
 * @note This function must be called exactly once.
 */
void options_init( void );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tidy_options_H */
/* vim:set et sw=2 ts=2: */
