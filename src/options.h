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

#ifndef tidy_options_h
#define tidy_options_h

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
 * @defgroup options-group Include-Tidy Options
 * Types, global variables, and functions for **include-tidy** options.
 * @{
 */

////////// macros /////////////////////////////////////////////////////////////

/**
 * Convenience macro that checks whether being verbose about \a WHAT was
 * requested.
 *
 * @param WHAT What to be verbose about without the `TIDY_VERBOSE_` prefix.
 *
 * @sa opt_verbose
 * @sa tidy_verbose
 */
#define IS_VERBOSE(WHAT)          ((opt_verbose & TIDY_VERBOSE_ ## WHAT) != 0)

#define OPT_ALIGN_COLUMN_DEFAULT  41      /**< Default column alignment. */
#define OPT_ALIGN_COLUMN_MAX      256     /**< Maximum column alignment. */
#define OPT_COMPILER_DEFAULT      "clang" /**< Default compiler path. */
#define OPT_LINE_LENGTH_DEFAULT   80      /**< Default line length. */
#define OPT_LINE_LENGTH_MAX       512     /**< Maximum line length. */
#define OPT_VERBOSE_ALL           "acCdfFipPsSz" /**< All verbose values. */

////////// enums //////////////////////////////////////////////////////////////

/**
 * Which symbols and in what order to include in a comment.
 */
enum tidy_comment {
  TIDY_COMMENT_SYM_ALPHA,               ///< Sorted alphabetically.
  TIDY_COMMENT_SYM_LENGTH,              ///< Sorted by name length, ascending.
  TIDY_COMMENT_SYM_MOST_REF,            ///< Only most-referenced symbol.
  TIDY_COMMENT_SYM_REF_COUNT            ///< Sorted by ref. count, descending.
};

/**
 * When to exit with a non-zero status code.
 */
enum tidy_error {
  TIDY_ERROR_IF_VIOLATIONS,             ///< Non-zero if violations.
  TIDY_ERROR_ALWAYS,                    ///< Always exit with non-zero.
  TIDY_ERROR_NEVER                      ///< Never exit with non-zero.
};

/**
 * Verbose mode.
 *
 * @note If this is updated, ensure #OPT_VERBOSE_ALL matches.
 *
 * @sa #IS_VERBOSE()
 * @sa opt_verbose
 * @sa opt_verbose_parse()
 */
enum tidy_verbose {
  TIDY_VERBOSE_NONE,                            ///< Don't be verbose.
  TIDY_VERBOSE_ARGS                 = 1 << 0,   ///< Command-line arguments.
  TIDY_VERBOSE_CONFIG_FILES         = 1 << 1,   ///< Configuration files.
  TIDY_VERBOSE_CONFIG_SYMBOLS       = 1 << 2,   ///< Configuration symbols.
  TIDY_VERBOSE_CURSORS              = 1 << 3,   ///< Libclang cursors.
  TIDY_VERBOSE_DIRECTORY            = 1 << 4,   ///< Changing directory.
  TIDY_VERBOSE_INCLUDES             = 1 << 5,   ///< Files included.
  TIDY_VERBOSE_PROXIES_EXPLICIT     = 1 << 6,   ///< Explicit include proxies.
  TIDY_VERBOSE_PROXIES_IMPLICIT     = 1 << 7,   ///< Implicit include proxies.
  TIDY_VERBOSE_SRC_FILE_VIOLATIONS  = 1 << 8,   ///< Source file in violation.
  TIDY_VERBOSE_SRC_FILE_ALWAYS      = 1 << 9,   ///< Always source file.
  TIDY_VERBOSE_STATISTICS           = 1 << 10,  ///< Print statistics?
  TIDY_VERBOSE_SYMBOLS              = 1 << 11,  ///< Symbols referenced.
};

////////// typedefs ///////////////////////////////////////////////////////////

typedef enum tidy_comment tidy_comment;
typedef enum tidy_error   tidy_error;
typedef enum tidy_verbose tidy_verbose;

////////// extern option variables ////////////////////////////////////////////

extern unsigned     opt_align_column;     ///< Comment alignment column.
extern bool         opt_all_includes;     ///< Print all includes?
extern color_when   opt_color_when;       ///< When to colorize.
extern char const  *opt_comment_style[2]; ///< Comment delimiters to use.
extern tidy_comment opt_comment_symbols;  ///< How to list symbols in comments.
extern bool         opt_config_layers;    ///< Do configuration file layering?
extern char const  *opt_config_path;      ///< Configuration file path.
extern bool         opt_debug;            ///< Print debugging output?
extern tidy_error   opt_error;            ///< When to exit with non-zero.
extern unsigned     opt_line_length;      ///< Line length.
extern tidy_verbose opt_verbose;          ///< Print verbose output?

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
 * Parses the comment symbols.
 *
 * @param s The comment symbols to parse.
 * @return Returns `true` only if \a s parsed successfully.
 */
NODISCARD
bool opt_comment_symbols_parse( char const *s );

/**
 * Parses when to exit with a non-zero exit status.
 *
 * @param s The error to parse.
 * @return Returns `true` only if \a s parsed successfully.
 */
NODISCARD
bool opt_error_parse( char const *s );

/**
 * Parses the line length.
 *
 * @param s The string to parse.
 * @return Returns the line length.
 */
NODISCARD
bool opt_line_length_parse( char const *s );

/**
 * Parses the value of the **include-tidy** verbose option.
 *
 * @param verbose_format
 * @parblock
 * The null-terminated **include-tidy** verbose format string to parse.
 * Valid formats are:
 *
 * Format | Be verbose about ...
 * -------|-----------------------------------------------------------------
 * `a`    | Command-line arguments.
 * `c`    | Configuration files read or attempted.
 * `C`    | Libclang cursors.
 * `d`    | Directory changing to from the `--directory` or `-d` option.
 * `f`    | Name of source-file only if in violation.
 * `F`    | Name of source-file.
 * `i`    | Files included.
 * `P`    | Implicit include proxies.
 * `P`    | Explicit include proxies.
 * `s`    | Symbols referenced and the include files declaring them.
 * `S`    | Configuration file symbols.
 * `z`    | Statistics.
 *
 * Multiple formats may be given, one immediately after the other, e.g., `ai`.
 * Alternatively, `*` may be given to mean "all" or either the empty string or
 * `-` may be given to mean "none."
 * @endparblock
 * @return Returns the parsed value.
 */
NODISCARD
bool opt_verbose_parse( char const *verbose_format );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* tidy_options_h */
/* vim:set et sw=2 ts=2: */
