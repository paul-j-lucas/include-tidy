/*
**      include-tidy -- #include tidier
**      src/toml_lite.h
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

#ifndef include_tidy_toml_lite_H
#define include_tidy_toml_lite_H

/**
 * @file
 * Declares types and functions for reading a TOML file.
 */

// local
#include "pjl_config.h"
#include "red_black.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <stdbool.h>
#include <stdio.h>

/// @endcond

/**
 * @defgroup toml-public-group TOML
 * Types and functions for reading a TOML file.
 *
 * This implements a "lite" version of the TOML 1.0.0 specification in that the
 * following are _not_ supported:
 *
 *  + Floating-point numbers.
 *  + Dates or times.
 *  + UTF-8.
 *  + Array of tables.
 *  + Inline tables.
 * @{
 */

////////// enumerations ///////////////////////////////////////////////////////

// Enumerations have to be declared before typedefs of them since ISO C doesn't
// allow forward declarations of enums.

/**
 * TOML error.
 */
enum toml_error {
  TOML_ERR_NONE,                        ///< No error.
  TOML_ERR_INT_INVALID,                 ///< Invalid integer.
  TOML_ERR_INT_RANGE,                   ///< Integer out of range.
  TOML_ERR_KEY_DUPLICATE,               ///< Duplicate key.
  TOML_ERR_KEY_INVALID,                 ///< Invalid key.
  TOML_ERR_STR_INVALID,                 ///< Invalid string.
  TOML_ERR_UNEX_CHAR,                   ///< Unexpected character.
  TOML_ERR_UNEX_EOF,                    ///< Unexpected end of file.
  TOML_ERR_UNEX_NEWLINE,                ///< Unexpected newline.
  TOML_ERR_UNEX_VALUE,                  ///< Unexpected value.
};

/**
 * TOML value type.
 */
enum toml_type {
  TOML_BOOL,                            ///< Boolean type.
  TOML_INT,                             ///< Integer type.
  TOML_STRING,                          ///< String type.
  TOML_ARRAY                            ///< Array type.
};

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct  toml_array      toml_array;
typedef enum    toml_error      toml_error;
typedef struct  toml_file       toml_file;
typedef struct  toml_key_value  toml_key_value;
typedef struct  toml_loc        toml_loc;
typedef struct  toml_table      toml_table;
typedef enum    toml_type       toml_type;
typedef struct  toml_value      toml_value;

////////// structs ////////////////////////////////////////////////////////////

/**
 * TOML array.
 */
struct toml_array {
  toml_value *values;                   ///< Values.
  unsigned    size;                     ///< Size of \ref values.
};

/**
 * TOML file location.
 */
struct toml_loc {
  unsigned  line;                       ///< Line number.
  unsigned  col;                        ///< Column number (1 based).
};

/**
 * TOML file.
 */
struct toml_file {
  FILE       *file;                     ///< `FILE` to read.
  toml_error  error;                    ///< Error code, if any.
  char const *error_msg;                ///< Error message, if any.
  unsigned    array_depth;              ///< Array depth.
  toml_loc    loc;                      ///< Current source file location.
  unsigned    col_prev;                 ///< Previous column within file.
  bool        in_key_value;             ///< Started parsing _key_ = _value_?
};

/**
 * TOML value.
 */
struct toml_value {
  toml_type     type;                   ///< The type of value.
  toml_loc      loc;                    ///< Source file location.
  union {
    bool        b;                      ///< The Boolean value.
    long        i;                      ///< The integer value.
    char       *s;                      ///< The string value.
    toml_array  a;                      ///< The array value.
  };
};

/**
 * TOML key/value pair.
 */
struct toml_key_value {
  char const *key;                      ///< Key.
  toml_loc    key_loc;                  ///< Key's source file location.
  toml_value  value;                    ///< Value.
};

/**
 * TOML table.
 */
struct toml_table {
  char const *name;                     ///< Table name, if any.
  rb_tree_t   keys_values;              ///< Keys & values.
};

////////// extern functions ///////////////////////////////////////////////////

/**
 * Cleans-up the resources of a toml_file.
 *
 * @param toml The toml_file to clean up.  If NULL, does nothing.
 *
 * @note The `FILE` that the toml_file was using is _not_ closed since
 * toml_init() didn't open it.
 *
 * @sa toml_open()
 */
void toml_cleanup( toml_file *toml );

/**
 * Gets the error message corresponding to \ref toml_file::error "error".
 *
 * @param toml The toml_file to get the error message for.
 * @return Returns said message.
 */
NODISCARD
char const* toml_error_msg( toml_file const *toml );

/**
 * Initializes a toml_file.
 *
 * @param toml The toml_file to initialize.
 * @param file The `FILE` to read.
 *
 * @sa toml_cleanup()
 */
void toml_init( toml_file *toml, FILE *file );

/**
 * Cleans-up a toml_table.
 *
 * @param table The toml_table to clean up.  If NULL, does nothing.
 *
 * @sa toml_table_init()
 */
void toml_table_cleanup( toml_table *table );

/**
 * Attempts to find \a key in \a table.
 *
 * @param table The toml_table to find \a key in.
 * @param key The key to find in \a table.
 * @return Returns the associated value if \a key is found or NULL if not.
 */
NODISCARD
toml_value const* toml_table_find( toml_table const *table, char const *key );

/**
 * Initialzes a toml_table.
 *
 * @param table The toml_table to initialize.
 *
 * @sa toml_table_cleanup()
 */
void toml_table_init( toml_table *table );

/**
 * Gets the next table from \a toml, if any.
 *
 * @param toml The toml_file to get the next table from.
 * @param table A pointer to receive the table.
 * @return Returns `true` only if there is a next table and it was parsed
 * successfully.
 *
 * @sa toml_table_cleanup()
 * @sa toml_table_init()
 */
NODISCARD
bool toml_table_next( toml_file *toml, toml_table *table );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tidy_toml_lite_H */
/* vim:set et sw=2 ts=2: */
