/*
**      include-tidy -- #include tidier
**      src/config_file.c
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

/**
 * @file
 * Defines functions for reading and querying an **include-tidy**
 * configuration file.
 */

// local
#include "pjl_config.h"
#include "config_file.h"
#include "array.h"
#include "bit_util.h"
#include "cli_options.h"
#include "fnv1a.h"
#include "hash_table.h"
#include "include.h"
#include "include-tidy.h"
#include "options.h"
#include "path_util.h"
#include "print.h"
#include "proxies.h"
#include "red_black.h"
#include "strbuf.h"
#include "toml_lite.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <errno.h>
#include <fnmatch.h>
#if HAVE_PWD_H
# include <pwd.h>                       /* for getpwuid() */
#endif /* HAVE_PWD_H */
#include <stdbool.h>                    /* for uint64_t */
#include <stdio.h>
#include <stdlib.h>                     /* for getenv(), ... */
#include <string.h>
#include <sysexits.h>
#include <unistd.h>                     /* for geteuid() */

/// @endcond

/**
 * @addtogroup config-file-group
 * @{
 */

////////// enums //////////////////////////////////////////////////////////////

/**
 * Options for the config_open() function.
 */
enum config_opts {
  CONFIG_OPT_NONE           = 0,        ///< No options.
  CONFIG_OPT_ERROR_IS_FATAL = 1 << 0,   ///< An error is fatal.
  CONFIG_OPT_IGNORE_ENOENT  = 1 << 1    ///< Ignore file not found.
};

/**
 * Table kind.
 */
enum config_table_kind {
  TABLE_NONE          = 0,              ///< No table.
  TABLE_INCLUDE_TIDY  = 1 << 0,         ///< The `include-tidy` table.
  TABLE_HEADER        = 1 << 1,         ///< Header table.
  TABLE_SOURCE        = 1 << 2,         ///< Source table.
  TABLE_SYMBOL        = 1 << 3          ///< Symbol table.
};

/// @cond DOXYGEN_IGNORE
#define TABLE_HEADER_SOURCE       (TABLE_HEADER | TABLE_SOURCE)
/// @endcond

////////// typedefs ///////////////////////////////////////////////////////////

typedef enum    config_opts           config_opts;
typedef struct  config_key            config_key;
typedef struct  config_parse_fn_args  config_parse_fn_args;
typedef enum    config_table_kind     config_table_kind;
typedef struct  symbol_includes       symbol_includes;

/**
 * Signature for a function that parses the value of a configuration key.
 *
 * @note Functions ending in `_string` are functions that assume the \ref
 * toml_value::type "type" of \a value is #TOML_STRING.
 *
 * @param config The configuration arguments to use.
 */
typedef void (*config_parse_fn)( config_parse_fn_args const *config );

////////// structs ////////////////////////////////////////////////////////////

/**
 * Configuration key (in a table).
 */
struct config_key {
  char const       *name;               ///< Key name.
  config_table_kind table_kinds;        ///< Table kind(s) allowed in.
  config_parse_fn   parse_fn;           ///< Value parsing function.
};

/**
 * Arguments to a config_parse_fn.
 */
struct config_parse_fn_args {
  char const       *config_path;        ///< Configuration file full path.
  toml_table const *table;              ///< Current table.
  toml_key const   *key;                ///< Current key.
  toml_value const *value;              ///< Value to parse.
};

/**
 * Mapping from a symbol name to a set of include CXFiles that declare it.
 *
 * @remarks A set of include files is needed since some symbols are declared in
 * multiple include files, e.g., `NULL` and `size_t`.
 *
 * @par Example
 *  ```
 *  [NULL]
 *  includes = [
 *      "locale.h",
 *      "stddef.h",
 *      "stdio.h",
 *      "stdlib.h",
 *      "string.h",
 *      "time.h",
 *      "wchar.h",
 *  ]
 *  ```
 */
struct symbol_includes {
  char const *from_sym_name;            ///< Symbol name.

  /**
   * The set of include file(s) that declare \ref from_sym_name.
   *
   * @remarks This is a red-black tree and not a hash table because, when we
   * iterate over it, we want it to be in sorted order.
   */
  rb_tree_t   to_include_set;
};

////////// local functions ////////////////////////////////////////////////////

static void         add_c_includes_parse( config_parse_fn_args const* );
static void         add_cxx_includes_parse( config_parse_fn_args const* );
static void         align_column_parse( config_parse_fn_args const* );
static void         all_includes_parse( config_parse_fn_args const* );
static void         associated_header_parse( config_parse_fn_args const* );
static void         color_parse( config_parse_fn_args const* );
static void         comment_style_parse( config_parse_fn_args const* );
static void         comment_symbols_parse( config_parse_fn_args const* );

NODISCARD
static FILE*        config_open( char const*, config_opts );

static void         elide_includes_parse( config_parse_fn_args const* );
static void         elide_include_parse_string( config_parse_fn_args const* );
static void         error_parse( config_parse_fn_args const* );
static void         first_parse( config_parse_fn_args const* );

NODISCARD
static char const*  home_dir( void );

static void         ignore_as_argument_parse( config_parse_fn_args const* );
static void         ignore_parse( config_parse_fn_args const* );
static void         ignore_symbols_parse( config_parse_fn_args const* );
static void         ignore_symbols_parse_string( config_parse_fn_args const* );
static void         include_handle( char const*, tidy_handling );
static void         includes_parse( config_parse_fn_args const* );
static void         includes_parse_string( config_parse_fn_args const* );
static void         keep_includes_parse( config_parse_fn_args const* );
static void         keep_includes_parse_string( config_parse_fn_args const* );
static void         keep_parse( config_parse_fn_args const* );
static void         line_length_parse( config_parse_fn_args const* );
static void         proxy_parse( config_parse_fn_args const* );
static void         proxy_parse_string( config_parse_fn_args const* );
static void         std_c_includes_parse( config_parse_fn_args const* );
static void         std_cxx_includes_parse( config_parse_fn_args const* );
static void         symbol_include_add( char const*, tidy_include* );
static void         symbol_includes_cleanup( symbol_includes* );
static void         symbols_parse( config_parse_fn_args const* );
static void         symbols_parse_string( config_parse_fn_args const* );

NODISCARD
static int          tidy_include_cmp_by_rel_path( tidy_include const*,
                                                  tidy_include const* );

////////// local constants ////////////////////////////////////////////////////

/**
 * Configuration keys.
 */
static config_key const CONFIG_KEYS[] = {
  { "add-c-includes",     TABLE_INCLUDE_TIDY,   &add_c_includes_parse     },
  { "add-cxx-includes",   TABLE_INCLUDE_TIDY,   &add_cxx_includes_parse   },
  { "align-column",       TABLE_INCLUDE_TIDY,   &align_column_parse       },
  { "all-includes",       TABLE_INCLUDE_TIDY,   &all_includes_parse       },
  { "associated-header",  TABLE_SOURCE,         &associated_header_parse  },
  { "color",              TABLE_INCLUDE_TIDY,   &color_parse              },
  { "comment-style",      TABLE_INCLUDE_TIDY,   &comment_style_parse      },
  { "comment-symbols",    TABLE_INCLUDE_TIDY,   &comment_symbols_parse    },
  { "elide-includes",     TABLE_HEADER_SOURCE,  &elide_includes_parse     },
  { "error",              TABLE_INCLUDE_TIDY,   &error_parse              },
  { "first",              TABLE_HEADER,         &first_parse              },
  { "ignore",             TABLE_SYMBOL,         &ignore_parse             },
  { "ignore-as-argument", TABLE_HEADER_SOURCE,  &ignore_as_argument_parse },
  { "ignore-symbols",     TABLE_HEADER_SOURCE,  &ignore_symbols_parse     },
  { "includes",           TABLE_SYMBOL,         &includes_parse           },
  { "keep",               TABLE_HEADER,         &keep_parse               },
  { "keep-includes",      TABLE_HEADER_SOURCE,  &keep_includes_parse      },
  { "line-length",        TABLE_INCLUDE_TIDY,   &line_length_parse        },
  { "proxy",              TABLE_HEADER,         &proxy_parse              },
  { "std-c-includes",     TABLE_INCLUDE_TIDY,   &std_c_includes_parse     },
  { "std-cxx-includes",   TABLE_INCLUDE_TIDY,   &std_cxx_includes_parse   },
  { "symbols",            TABLE_HEADER,         &symbols_parse            },
};

/**
 * Strings for table kinds.
 */
static char const *const TABLE_KINDS[] = {
  [ TABLE_NONE ] = "none",
  [ TABLE_INCLUDE_TIDY ] = "include-tidy",
  [ TABLE_HEADER ] = "header",
  [ TABLE_HEADER_SOURCE ] = "source or header",
  [ TABLE_SOURCE ] = "source",
  [ TABLE_SYMBOL ] = "symbol"
};

////////// extern variables ///////////////////////////////////////////////////

/// @cond DOXYGEN_IGNORE
/// Otherwise Doxygen generates two entries.

char const       *tidy_associated_header_rel_path;
bool              tidy_is_source_path_ignored;

/// @endcond

////////// local variables ////////////////////////////////////////////////////

static unsigned     error_count;          ///< Configuration file error count.
static hash_table_t ignore_symbol_set;    ///< Set of symbols to ignore.
static array_t      std_c_includes;       ///< Standard-ish C include files.
static array_t      std_cxx_includes;     ///< Standard C++ include files.
static unsigned     warning_count;        ///< Configuration file warning count.

/**
 * Mapping from symbols to the include file(s) they're declared in.
 *
 * @remarks This is a red-black tree and not a hash table because, when we
 * iterate over it, we want it to be in sorted order.
 *
 * @sa symbol_includes
 */
static rb_tree_t symbol_includes_map;

////////// local TOML type parsing functions //////////////////////////////////

/**
 * Parses a TOML bool value.
 *
 * @param config The config_parse_fn_args to use.
 * @return Returns the parsed bool value.
 */
NODISCARD
static bool toml_bool_parse( config_parse_fn_args const *config ) {
  assert( config != NULL );

  if ( config->value->type != TOML_BOOL ) {
    print_file_error(
      config->config_path, config->value->loc.line, config->value->loc.col,
      "invalid value for \"%s\"; expected boolean\n",
      config->key->name
    );
    ++error_count;
    return false;
  }

  return config->value->b;
}

/**
 * Parses a TOML integer value.
 *
 * @param config The config_parse_fn_args to use.
 * @param value_min The minimum allowed value.
 * @param value_max The maximum allowed value.
 * @return Returns the parsed integer value.
 */
NODISCARD
static long toml_int_parse( config_parse_fn_args const *config, long value_min,
                            long value_max ) {
  assert( config != NULL );

  if ( config->value->type != TOML_INT ) {
    print_file_error(
      config->config_path, config->value->loc.line, config->value->loc.col,
      "invalid value for \"%s\"; expected integer\n",
      config->key->name
    );
    ++error_count;
    return 0;
  }

  if ( config->value->i < value_min || config->value->i > value_max ) {
    print_file_error(
      config->config_path, config->value->loc.line, config->value->loc.col,
      "\"%ld\": invalid value for \"%s\"; must be %ld-%ld\n",
      config->value->i, config->key->name, value_min, value_max
    );
    ++error_count;
    return 0;
  }

  return config->value->i;
}

/**
 * Parses a TOML array of strings values.
 *
 * @param config The config_parse_fn_args to use.
 * @return Returns an array of the parsed string values.
 *
 * @sa toml_string_or_string_array_parse()
 * @sa toml_string_parse()
 */
NODISCARD
static array_t toml_string_array_parse( config_parse_fn_args const *config ) {
  assert( config != NULL );

  array_t array = ARRAY_INIT( sizeof(char*) );

  if ( config->value->type != TOML_ARRAY ) {
    print_file_error(
      config->config_path, config->value->loc.line, config->value->loc.col,
      "invalid value for \"%s\"; expected array\n",
      config->key->name
    );
    ++error_count;
    goto done;
  }

  if ( config->value->a.size > 0 ) {
    array_reserve( &array, config->value->a.size );

    for ( unsigned i = 0; i < config->value->a.size; ++i ) {
      toml_value *const a_value = &config->value->a.values[i];
      if ( a_value->type != TOML_STRING ) {
        print_file_error(
          config->config_path, a_value->loc.line, a_value->loc.col,
          "invalid value for \"%s\"; expected string\n",
          config->key->name
        );
        ++error_count;
        continue;
      }
      *(char**)array_push_back( &array ) = a_value->s;
      a_value->s = NULL;                // steal value's string
    } // for
  }

done:
  return array;
}

/**
 * Parses either a TOML string value or an array of string values.
 *
 * @param config The config_parse_fn_args to use.
 * @param parse_fn The parse function to use.
 *
 * @sa toml_string_array_parse()
 * @sa toml_string_parse()
 */
static
void toml_string_or_string_array_parse( config_parse_fn_args const *config,
                                        config_parse_fn parse_fn ) {
  assert( config != NULL );
  assert( parse_fn != NULL );

  switch ( config->value->type ) {
    case TOML_STRING:
      (*parse_fn)( config );
      break;

    case TOML_ARRAY:
      for ( unsigned i = 0; i < config->value->a.size; ++i ) {
        toml_value const *const a_value = &config->value->a.values[i];
        if ( a_value->type != TOML_STRING ) {
          print_file_error(
            config->config_path, a_value->loc.line, a_value->loc.col,
            "invalid value for \"%s\" key array; expected string\n",
            config->key->name
          );
          ++error_count;
          continue;
        }
        config_parse_fn_args const a_value_config = {
          .config_path = config->config_path,
          .table = config->table,
          .key = config->key,
          .value = a_value
        };
        (*parse_fn)( &a_value_config );
      } // for
      break;

    default:
      print_file_error(
        config->config_path, config->value->loc.line, config->value->loc.col,
        "invalid value for \"%s\" key; expected string or array\n",
        config->key->name
      );
      ++error_count;
  } // switch
}

/**
 * Parses a TOML string value.
 *
 * @param config The config_parse_fn_args to use.
 * @return Returns the parsed string value.
 *
 * @sa toml_string_array_parse()
 * @sa toml_string_or_string_array_parse()
 */
NODISCARD
static char const* toml_string_parse( config_parse_fn_args const *config ) {
  assert( config != NULL );

  if ( config->value->type != TOML_STRING ) {
    print_file_error(
      config->config_path, config->value->loc.line, config->value->loc.col,
      "invalid value for \"%s\"; expected string\n",
      config->key->name
    );
    ++error_count;
    return "";
  }

  return config->value->s;
}

////////// local configutation key parsing functions //////////////////////////

/**
 * Parses the value of an `"add-c-includes"` key.
 *
 * @param config The config_parse_fn_args to use.
 *
 * @sa add_cxx_includes_parse()
 */
static void add_c_includes_parse( config_parse_fn_args const *config ) {
  array_t add_c_includes = toml_string_array_parse( config );
  if ( error_count == 0 )
    array_push_array_back( &std_c_includes, &add_c_includes );
  array_cleanup( &add_c_includes, /*free_fn=*/NULL );
}

/**
 * Parses the value of an `"add-cxx-includes"` key.
 *
 * @param config The config_parse_fn_args to use.
 *
 * @sa add_c_includes_parse()
 */
static void add_cxx_includes_parse( config_parse_fn_args const *config ) {
  array_t add_cxx_includes = toml_string_array_parse( config );
  if ( error_count == 0 )
    array_push_array_back( &std_cxx_includes, &add_cxx_includes );
  array_cleanup( &add_cxx_includes, /*free_fn=*/NULL );
}

/**
 * Parses the value of an `"align-column"` key.
 *
 * @param config The config_parse_fn_args to use.
 */
static void align_column_parse( config_parse_fn_args const *config ) {
  long const int_value = toml_int_parse( config, 0, OPT_ALIGN_COLUMN_MAX );
  if ( error_count == 0 && !option_is_set( COPT(ALIGN_COLUMN) ) ) {
    opt_align_column = STATIC_CAST( unsigned, int_value );
    option_mark_set( COPT(ALIGN_COLUMN) );
  }
}

/**
 * Parses the value of an `"all-includes"` key.
 *
 * @param config The config_parse_fn_args to use.
 */
static void all_includes_parse( config_parse_fn_args const *config ) {
  if ( !option_is_set( COPT(ALL_INCLUDES) ) ) {
    opt_all_includes = toml_bool_parse( config );
    option_mark_set( COPT(ALL_INCLUDES) );
  }
}

/**
 * Parses the value of an `"associated-header"` key.
 *
 * @param config The config_parse_fn_args to use.
 */
static void associated_header_parse( config_parse_fn_args const *config ) {
  assert( config != NULL );

  char const *const ext = path_ext( config->table->key.name );
  if ( ext != NULL && ext[0] != 'c' ) {
    print_file_error(
      config->config_path, config->key->loc.line, config->key->loc.col,
      "\"%s\": only non-header files may have an associated header\n",
      config->table->key.name
    );
    ++error_count;
  }

  char const *const string_value = toml_string_parse( config );
  if ( error_count > 0 )
    return;
  if ( strcmp( config->table->key.name, tidy_source_path ) == 0 )
    tidy_associated_header_rel_path = check_strdup( string_value );
}

/**
 * Parses the value of an `"color"` key.
 *
 * @param config The config_parse_fn_args to use.
 */
static void color_parse( config_parse_fn_args const *config ) {
  char const *const string_value = toml_string_parse( config );

  if ( error_count > 0 )
    return;
  if ( option_is_set( COPT(COLOR) ) )
    return;

  if ( !opt_color_parse( string_value ) ) {
    print_file_error(
      config->config_path, config->value->loc.line, config->value->loc.col,
      "invalid value for \"%s\"\n",
      config->key->name
    );
    ++error_count;
  }
  option_mark_set( COPT(COLOR) );
}

/**
 * Parses the value of an `"comment-style"` key.
 *
 * @param config The config_parse_fn_args to use.
 */
static void comment_style_parse( config_parse_fn_args const *config ) {
  char const *const string_value = toml_string_parse( config );

  if ( error_count > 0 )
    return;
  if ( option_is_set( COPT(COMMENT_STYLE) ) )
    return;

  if ( !opt_comment_style_parse( string_value ) ) {
    print_file_error(
      config->config_path, config->value->loc.line, config->value->loc.col,
      "invalid value for \"%s\"; must be one of \"//\", \"/*\", or \"none\"\n",
      config->key->name
    );
    ++error_count;
  }
  option_mark_set( COPT(COMMENT_STYLE) );
}

/**
 * Parses the value of an `"comment-symbols"` key.
 *
 * @param config The config_parse_fn_args to use.
 */
static void comment_symbols_parse( config_parse_fn_args const *config ) {
  char const *const string_value = toml_string_parse( config );

  if ( error_count > 0 )
    return;
  if ( option_is_set( COPT(COMMENT_SYMBOLS) ) )
    return;

  if ( !opt_comment_symbols_parse( string_value ) ) {
    print_file_error(
      config->config_path, config->value->loc.line, config->value->loc.col,
      "invalid value for \"%s\"; must be one of "
      "\"alpha\", \"length\", \"ref-count\", or \"most-used\"\n",
      config->key->name
    );
    ++error_count;
  }
  option_mark_set( COPT(COMMENT_SYMBOLS) );
}

/**
 * Parses the value of an `"elide-includes"` key.
 *
 * @param config The config_parse_fn_args to use.
 *
 * @sa elide_include_parse_string()
 */
static void elide_includes_parse( config_parse_fn_args const *config ) {
  assert( config != NULL );

  if ( strcmp( config->table->key.name, tidy_source_path ) == 0 )
    toml_string_or_string_array_parse( config, &elide_include_parse_string );
}

/**
 * Parses a single string value of an `"elide-includes"` key.
 *
 * @param config The config_parse_fn_args to use.
 *
 * @sa elide_includes_parse()
 * @sa include_handle()
 */
static void elide_include_parse_string( config_parse_fn_args const *config ) {
  assert( config != NULL );
  assert( config->value->type == TOML_STRING );

  include_handle( config->value->s, TIDY_HANDLE_ELIDE );
}

/**
 * Parses the value of a `"error"` key.
 *
 * @param config The config_parse_fn_args to use.
 */
static void error_parse( config_parse_fn_args const *config ) {
  char const *const string_value = toml_string_parse( config );

  if ( error_count > 0 )
    return;
  if ( option_is_set( COPT(ERROR) ) )
    return;

  if ( !opt_error_parse( string_value ) ) {
    print_file_error(
      config->config_path, config->value->loc.line, config->value->loc.col,
      "invalid value for \"%s\"\n",
      config->key->name
    );
    ++error_count;
  }
  option_mark_set( COPT(ERROR) );
}

/**
 * Parses the value of an `"first"` key.
 *
 * @param config The config_parse_fn_args to use.
 */
static void first_parse( config_parse_fn_args const *config ) {
  if ( !toml_bool_parse( config ) )
    return;
  if ( error_count > 0 )
    return;

  rb_iterator_t iter;
  size_t const  rel_path_len = strlen( config->table->key.name );

  rb_iterator_init( &iter, &tidy_include_set );
  for ( tidy_include *include;
        (include = rb_iterator_next( &iter )) != NULL; ) {
    if ( path_ends_with( include->abs_path, config->table->key.name,
                         rel_path_len ) ) {
      include->sort_rank = TIDY_SORT_FIRST;
    }
  } // for
}

/**
 * Parses the value of an `"ignore-as-argument"` key.
 *
 * @param config The config_parse_fn_args to use.
 */
static void ignore_as_argument_parse( config_parse_fn_args const *config ) {
  assert( config != NULL );

  if ( strcmp( config->table->key.name, tidy_source_path ) != 0 )
    return;
  if ( toml_bool_parse( config ) )
    tidy_is_source_path_ignored = true;
};

/**
 * Parses the value of an `"ignore"` key.
 *
 * @param config The config_parse_fn_args to use.
 */
static void ignore_parse( config_parse_fn_args const *config ) {
  if ( toml_bool_parse( config ) ) {
    PJL_DISCARD_RV(
      ht_insert(
        &ignore_symbol_set, CONST_CAST( char*, config->table->key.name ),
        strlen( config->table->key.name ) + 1/*\0*/
      )
    );
  }
}

/**
 * Parses the value of an `"ignore-symbols"` key.
 *
 * @param config The config_parse_fn_args to use.
 *
 * @sa ignore_symbols_parse_string()
 */
static void ignore_symbols_parse( config_parse_fn_args const *config ) {
  assert( config != NULL );

  if ( strcmp( config->table->key.name, tidy_source_path ) == 0 )
    toml_string_or_string_array_parse( config, &ignore_symbols_parse_string );
}

/**
 * Parses a single string value of an `"ignore-symbols"` key.
 *
 * @param config The config_parse_fn_args to use.
 *
 * @sa ignore_symbols_parse()
 */
static void ignore_symbols_parse_string( config_parse_fn_args const *config ) {
  assert( config != NULL );
  assert( config->value->type == TOML_STRING );

  ht_insert_rv_t const hti = ht_insert(
    &ignore_symbol_set, CONST_CAST( char*, config->value->s ),
    strlen( config->value->s ) + 1/*\0*/
  );
  if ( !hti.inserted ) {
    print_file_warning(
      config->config_path, config->value->loc.line, config->value->loc.col,
      "\"%s\" already ignored\n",
      config->value->s
    );
    ++warning_count;
  }
}

/**
 * Parses the value of an `"includes"` key.
 *
 * @param config The config_parse_fn_args to use.
 *
 * @sa includes_parse_string()
 */
static void includes_parse( config_parse_fn_args const *config ) {
  toml_string_or_string_array_parse( config, &includes_parse_string );
}

/**
 * Parses a single string value of an `"includes"` key.
 *
 * @param config The config_parse_fn_args to use.
 *
 * @sa includes_parse()
 * @sa symbols_parse_string()
 */
static void includes_parse_string( config_parse_fn_args const *config ) {
  assert( config != NULL );
  assert( config->value->type == TOML_STRING );

  tidy_include *const to_include = include_find_by_rel_path( config->value->s );
  if ( to_include != NULL )
    symbol_include_add( config->table->key.name, to_include );
}

/**
 * Parses the value of an `"keep-includes"` key.
 *
 * @param config The config_parse_fn_args to use.
 *
 * @sa keep_includes_parse_string()
 */
static void keep_includes_parse( config_parse_fn_args const *config ) {
  assert( config != NULL );

  if ( strcmp( config->table->key.name, tidy_source_path ) == 0 )
    toml_string_or_string_array_parse( config, &keep_includes_parse_string );
}

/**
 * Parses a single string value of a `"keep-includes"` key.
 *
 * @param config The config_parse_fn_args to use.
 *
 * @sa include_handle()
 * @sa keep_includes_parse()
 */
static void keep_includes_parse_string( config_parse_fn_args const *config ) {
  assert( config != NULL );
  assert( config->value->type == TOML_STRING );

  include_handle( config->value->s, TIDY_HANDLE_KEEP );
}

/**
 * Parses the value of a `"keep"` key.
 *
 * @param config The config_parse_fn_args to use.
 *
 * @sa include_handle()
 */
static void keep_parse( config_parse_fn_args const *config ) {
  if ( toml_bool_parse( config ) )
    include_handle( config->table->key.name, TIDY_HANDLE_KEEP );
}

/**
 * Parses the value of a `"line-length"` key.
 *
 * @param config The config_parse_fn_args to use.
 */
static void line_length_parse( config_parse_fn_args const *config ) {
  long const int_value = toml_int_parse( config, 0, OPT_LINE_LENGTH_MAX );
  if ( error_count == 0 && !option_is_set( COPT(LINE_LENGTH) ) ) {
    opt_line_length = STATIC_CAST( unsigned, int_value );
    option_mark_set( COPT(LINE_LENGTH) );
  }
}

/**
 * Parses the value of a `"proxy"` key.
 *
 * @param config The config_parse_fn_args to use.
 */
static void proxy_parse( config_parse_fn_args const *config ) {
  toml_string_or_string_array_parse( config, &proxy_parse_string );
}

/**
 * Parses a single string value of a `"proxy"` key.
 *
 * @param config The config_parse_fn_args to use.
 */
static void proxy_parse_string( config_parse_fn_args const *config ) {
  assert( config != NULL );

  tidy_include *const to_include =
    include_find_by_rel_path( config->table->key.name );
  if ( to_include == NULL )
    return;

  rb_iterator_t iter;
  size_t const  rel_path_len = strlen( config->value->s );

  rb_iterator_init( &iter, &tidy_include_set );
  for ( tidy_include *from_include;
        (from_include = rb_iterator_next( &iter )) != NULL; ) {
    if ( !path_ends_with( from_include->abs_path, config->value->s,
                          rel_path_len ) ) {
      continue;
    }
    if ( from_include->proxy != NULL ) {
      //
      // At some point, we may need to store all configured proxies then later
      // choose which of those to use based on the symbols referenced.  For
      // now, just pick the one that's more directly included.
      //
      if ( to_include->depth < from_include->proxy->depth )
        from_include->proxy = to_include;
    }
    else if ( include_proxy_would_cycle( from_include, to_include ) ) {
      print_file_warning(
        config->config_path, config->value->loc.line, config->value->loc.col,
        "\"%s\": proxy cycle detected\n",
        from_include->rel_path
      );
      ++warning_count;
    }
    else {
      from_include->proxy = to_include;
      from_include->is_proxy_explicit = true;
    }
  } // for
}

/**
 * Parses the value of an `"std-c-includes"` key.
 *
 * @param config The config_parse_fn_args to use.
 */
static void std_c_includes_parse( config_parse_fn_args const *config ) {
  if ( std_c_includes.len == 0 )
    std_c_includes = toml_string_array_parse( config );
}

/**
 * Parses the value of an `"std-cxx-includes"` key.
 *
 * @param config The config_parse_fn_args to use.
 */
static void std_cxx_includes_parse( config_parse_fn_args const *config ) {
  if ( std_cxx_includes.len == 0 )
    std_cxx_includes = toml_string_array_parse( config );
}

/**
 * Parses the value of a `"symbols"` key.
 *
 * @param config The config_parse_fn_args to use.
 *
 * @sa symbols_parse_string()
 */
static void symbols_parse( config_parse_fn_args const *config ) {
  toml_string_or_string_array_parse( config, &symbols_parse_string );
}

/**
 * Parses a single string value of a `"symbols"` key.
 *
 * @param config The config_parse_fn_args to use.
 *
 * @sa includes_parse_string()
 * @sa symbols_parse()
 */
static void symbols_parse_string( config_parse_fn_args const *config ) {
  assert( config != NULL );
  assert( config->value->type == TOML_STRING );

  tidy_include *const to_include =
    include_find_by_rel_path( config->table->key.name );
  if ( to_include != NULL )
    symbol_include_add( config->value->s, to_include );
}

////////// local functions ////////////////////////////////////////////////////

/**
 * Cleans-up all configuration data.
 */
static void config_cleanup( void ) {
  FREE( tidy_associated_header_rel_path );
  array_cleanup( &std_c_includes, &free_pptr );
  array_cleanup( &std_cxx_includes, &free_pptr );
  ht_cleanup( &ignore_symbol_set, /*free_fn=*/NULL );
  rb_tree_cleanup(
    &symbol_includes_map,
    POINTER_CAST( rb_free_fn_t, &symbol_includes_cleanup )
  );
}

/**
 * Finds and opens the configuration file.
 *
 * @remarks
 * @parblock
 * Configuration files are opened (if they exist) in this order:
 *
 *  1. The value of either the `--config` or `-c` command-line option.  (If
 *     specified, this _must_ exist.)
 *  2. `$PWD/include-tidy.toml`.
 *  3. `$XDG_CONFIG_HOME/include-tidy/config.toml`.  If `XDG_CONFIG_HOME` is
 *     empty or unset, then ``~/.config/` is used.
 *  4. For each _path_ in `$XDG_CONFIG_DIRS`, the first of
 *     <i>path</i><tt>/include-tidy/config.toml</tt> to exist and be readable.
 *     If `XDG_CONFIG_DIRS` is empty or unset, then `/etc/xdg` is used.
 * @endparblock
 *
 * @param config_path The full path to a configuration file.  May be NULL.
 * @param path_buf A path buffer to use.  Upon return, it contains the full
 * path of the configuration file that was found, if any.
 * @return Returns the `FILE*` for the configuration file if found or NULL if
 * not.
 */
NODISCARD
static FILE* config_file_find( char const *config_path, strbuf_t *path_buf ) {
  assert( path_buf != NULL );

  // This must be incremented in every case below and not just once initally
  // due to the fallthroughs between cases.
  static unsigned case_num = 1;

  FILE *config_file = NULL;

  switch ( case_num ) {
    case 1:
      // Try --config/-c command-line option.
      ++case_num;
      config_file = config_open( config_path, CONFIG_OPT_ERROR_IS_FATAL );
      if ( config_file != NULL ) {
        strbuf_reset( path_buf );
        strbuf_puts( path_buf, config_path );
        break;
      }
      FALLTHROUGH;

    case 2:
      // Try $PWD/include-tidy.toml.
      ++case_num;
      strbuf_reset( path_buf );
      size_t cwd_path_len;
      char const *const cwd_path = path_cwd( &cwd_path_len );
      strbuf_putsn( path_buf, cwd_path, cwd_path_len );
      strbuf_paths( path_buf, PACKAGE ".toml" );
      config_file = config_open( path_buf->str, CONFIG_OPT_IGNORE_ENOENT );
      if ( config_file != NULL )
        break;
      FALLTHROUGH;

    case 3:
      // Try $XDG_CONFIG_HOME/include-tidy/config.toml or
      // $HOME/.config/include-tidy/config.toml.
      ++case_num;
      strbuf_reset( path_buf );
      char const *const config_home =
        null_if_empty( getenv( "XDG_CONFIG_HOME" ) );
      if ( config_home != NULL ) {
        strbuf_puts( path_buf, config_home );
      }
      else {
        char const *const home = home_dir();
        if ( home != NULL ) {
          // LCOV_EXCL_START
          strbuf_puts( path_buf, home );
          strbuf_paths( path_buf, ".config" );
          // LCOV_EXCL_STOP
        }
      }
      if ( path_buf->len > 0 ) {
        strbuf_paths( path_buf, PACKAGE );
        strbuf_paths( path_buf, "config.toml" );
        config_file = config_open( path_buf->str, CONFIG_OPT_IGNORE_ENOENT );
        if ( config_file != NULL )
          break;
      }
      FALLTHROUGH;

    case 4:
      // Try $XDG_CONFIG_DIRS/include-tidy/config.toml or
      // /etc/xdg/include-tidy/config.toml.
      ++case_num;
      char const *config_dirs = null_if_empty( getenv( "XDG_CONFIG_DIRS" ) );
      if ( config_dirs == NULL )
        config_dirs = "/etc/xdg";       // LCOV_EXCL_LINE
      strbuf_reset( path_buf );
      for (;;) {
        char const *const next_sep = strchr( config_dirs, ':' );
        size_t const dir_len = next_sep != NULL ?
          STATIC_CAST( size_t, next_sep - config_dirs ) :
          strlen( config_dirs );
        if ( dir_len > 0 ) {
          strbuf_putsn( path_buf, config_dirs, dir_len );
          strbuf_paths( path_buf, PACKAGE );
          strbuf_paths( path_buf, "config.toml" );
          config_file = config_open( path_buf->str, CONFIG_OPT_IGNORE_ENOENT );
          if ( config_file != NULL )
            break;
        }
        if ( next_sep == NULL )
          break;
        config_dirs = next_sep + 1;
      } // for
  } // switch

  return config_file;
}

/**
 * Attempts to find the config_key having \a key_name.
 *
 * @param key_name The name of the configuration key to find.
 * @return Returns a pointer to the corresponding config_key or NULL if \a
 * key_name does not correspond to a config_key.
 */
NODISCARD
static config_key const* config_key_find( char const *key_name ) {
  assert( key_name != NULL );

  // For such a small set, linear search is good enough.
  FOREACH_ARRAY_ELEMENT( config_key, key, CONFIG_KEYS ) {
    if ( strcmp( key_name, key->name ) == 0 )
      return key;
  } // for

  return NULL;
}

/**
 * Tries to open a configuration file given by \a path.
 *
 * @param path The full path to try to open.  May be NULL.
 * @param opts The configuration options, if any.
 * @return Returns a `FILE*` to the open file upon success or NULL upon either
 * error or if \a path is NULL.
 */
NODISCARD
static FILE* config_open( char const *path, config_opts opts ) {
  if ( path == NULL )
    return NULL;

  FILE *const config_file = fopen( path, "r" );
  bool const  ok = config_file != NULL;
  static bool printed_configuration_header;

  if ( IS_VERBOSE( CONFIG_FILES ) ) {
    if ( verbose_section_begin( &printed_configuration_header ) )
      verbose_printf( "configuration files:\n" );
    verbose_printf( "  \"%s\": %s\n", path, ok ? "OK" : STRERROR() );
  }

  if ( !ok ) {
    switch ( errno ) {
      case ENOENT:
        if ( (opts & CONFIG_OPT_IGNORE_ENOENT) != 0 )
          break;
        FALLTHROUGH;
      default:
        if ( (opts & CONFIG_OPT_ERROR_IS_FATAL) != 0 ) {
          print_file_error( path, 0, 0, "%s\n", STRERROR() );
          exit( EX_NOINPUT );
        }
        print_file_warning( path, 0, 0, "%s\n", STRERROR() );
        ++warning_count;
        break;
    } // switch
  }

  return config_file;
}

/**
 * Parses a configuration file.
 *
 * @param config_path The full path to the configurarion file.
 * @param config_file The `FILE*` corresponding to \a config_path.
 */
static void config_parse( char const *config_path, FILE *config_file ) {
  assert( config_path != NULL );
  assert( config_file != NULL );

  toml_file toml;
  toml_file_init( &toml, config_file );

  toml_table table;
  toml_table_init( &table );

  while ( toml_table_next( &toml, &table ) ) {
    if ( table.key.name == NULL ) {
      print_file_error(
        config_path, toml.loc.line, toml.loc.col,
        "required table name missing\n"
      );
      ++error_count;
      continue;
    }
    if ( toml_table_empty( &table ) ) {
      print_file_error(
        config_path, table.key.loc.line, table.key.loc.col,
        "\"%s\": empty table\n", table.key.name
      );
      ++error_count;
      continue;
    }

    config_table_kind table_kinds =
      strcmp( table.key.name, "include-tidy" ) == 0 ?
        TABLE_INCLUDE_TIDY : TABLE_NONE;

    toml_iterator iter;
    toml_iterator_init( &iter, &table );
    for ( toml_key_value const *kv;
          (kv = toml_iterator_next( &iter )) != NULL; ) {
      config_key const *const found_key = config_key_find( kv->key.name );
      if ( found_key == NULL ) {
        print_file_error(
          config_path, kv->key.loc.line, kv->key.loc.col,
          "\"%s\": unknown key\n", table.key.name
        );
        ++error_count;
        continue;
      }

      if ( table_kinds == TABLE_NONE ||
           is_0n_bit_only_in_set( found_key->table_kinds, table_kinds ) ) {
        table_kinds = found_key->table_kinds;
      }
      else if ( (found_key->table_kinds & table_kinds) == 0 ) {
        assert( table_kinds < ARRAY_SIZE( TABLE_KINDS ) );
        print_file_error(
          config_path, kv->key.loc.line, kv->key.loc.col,
          "\"%s\": key not allowed in %s table%s; allowed only in %s table%s\n",
          found_key->name,
          TABLE_KINDS[ table_kinds ],
          is_1_bit( table_kinds ) ? "" : "s",
          TABLE_KINDS[ found_key->table_kinds ],
          is_1_bit( found_key->table_kinds ) ? "" : "s"
        );
        ++error_count;
        continue;
      }

      config_parse_fn_args const config = {
        .config_path = config_path,
        .table = &table,
        .key = &kv->key,
        .value = &kv->value
      };
      (*found_key->parse_fn)( &config );
    } // for
  } // while

  toml_table_cleanup( &table );

  if ( toml.error ) {
    print_file_error(
      config_path, toml.loc.line, toml.loc.col,
      "%s\n", toml_error_msg( &toml )
    );
    ++error_count;
  }

  toml_file_cleanup( &toml );

  if ( warning_count > 0 || error_count > 0 ) {
    EPRINTF( "%s: ", prog_name );
    if ( warning_count > 0 )
      EPRINTF( "%u warning%s", warning_count, plural_s( warning_count ) );
    if ( error_count > 0 ) {
      if ( warning_count > 0 )
        EPUTS( " and " );
      EPRINTF( "%u error%s", error_count, plural_s( error_count ) );
    }
    EPUTS( " generated\n" );
    exit( EX_CONFIG );
  }
}

// LCOV_EXCL_START
/**
 * Gets the full path of the user's home directory.
 *
 * @return Returns said directory or NULL if it is not obtainable.
 */
NODISCARD
static char const* home_dir( void ) {
  static char const *home;

  RUN_ONCE {
    home = null_if_empty( getenv( "HOME" ) );
#if HAVE_GETEUID && HAVE_GETPWUID && HAVE_STRUCT_PASSWD_PW_DIR
    if ( home == NULL ) {
      struct passwd const *const pw = getpwuid( geteuid() );
      if ( pw != NULL )
        home = null_if_empty( pw->pw_dir );
    }
#endif /* HAVE_GETEUID && && HAVE_GETPWUID && HAVE_STRUCT_PASSWD_PW_DIR */
  }

  return home;
}
// LCOV_EXCL_STOP

/**
 * Sets the \ref tidy_include::handling "handling" field of the include file(s)
 * having \a rel_path to \a handling.
 *
 * @param rel_path The relative path of the include file to use.
 * @param handling The handling to set.
 */
static void include_handle( char const *rel_path, tidy_handling handling ) {
  assert( rel_path != NULL );

  rb_iterator_t iter;
  size_t const  rel_path_len = strlen( rel_path );

  rb_iterator_init( &iter, &tidy_include_set );
  for ( tidy_include *include;
        (include = rb_iterator_next( &iter )) != NULL; ) {
    if ( path_ends_with( include->abs_path, rel_path, rel_path_len ) )
      include->handling = handling;
  } // for
}

/**
 * Gets whether \a rel_path is among \a includes.
 *
 * @param rel_path The relative path of an include file, e.g., `"stdio.h"` or
 * `"sys/wait.h"`.
 * @param includes An array of include paths.
 * @return Returns `true` only if \a rel_path is among \a includes.
 */
NODISCARD
static bool is_standard_include( char const *rel_path,
                                 array_t const *includes ) {
  assert( rel_path != NULL );
  assert( path_is_relative( rel_path ) );
  assert( includes != NULL );

  for ( size_t i = 0; i < includes->len; ++i ) {
    char const *const pattern = *(char const**)array_at_nc( includes, i );
    int const flags = strstr( pattern, "**" ) == NULL ? FNM_PATHNAME : 0;
    int const fnm_rv = fnmatch( pattern, rel_path, flags );
    switch ( fnm_rv ) {
      case 0:
        return true;
      case FNM_NOMATCH:
      default:
        // The man page for fnmatch(3) says it can return another non-zero
        // value on error -- but it's implementation specific, so just treat it
        // the same as FNM_NOMATCH.
        break;
    } // switch
  } // for

  return false;
}

/**
 * Cleans-up a symbol_includes.
 *
 * @param si The symbol_includes to clean up.  If NULL, does nothing.
 */
static void symbol_includes_cleanup( symbol_includes *si ) {
  if ( si != NULL ) {
    FREE( si->from_sym_name );
    rb_tree_cleanup( &si->to_include_set, /*free_fn=*/NULL );
  }
}

/**
 * Compares two \ref symbol_includes objects.
 *
 * @param i_si The first symbol_includes.
 * @param j_si The second symbol_includes.
 * @return Returns a number less than 0, 0, or greater than 0 if the name of \a
 * i_si is less than, equal to, or greater than the name of \a j_si,
 * respectively.
 */
NODISCARD
static int symbol_includes_cmp( symbol_includes const *i_si,
                                symbol_includes const *j_si ) {
  assert( i_si != NULL );
  assert( j_si != NULL );
  return strcmp( i_si->from_sym_name, j_si->from_sym_name );
}

/**
 * Maps \a from_sym_name to \a to_include_file so that if \a from_sym_name is
 * referenced, it'll require \a to_include_file.
 *
 * @param from_sym_name The name of the symbol.
 * @param to_include The include file that supposedly declares it.
 */
static void symbol_include_add( char const *from_sym_name,
                                tidy_include *to_include ) {
  assert( from_sym_name != NULL );
  assert( to_include != NULL );

  symbol_includes new_si = { .from_sym_name = from_sym_name };
  rb_insert_rv_t const rbi =
    rb_tree_insert( &symbol_includes_map, &new_si, sizeof new_si );
  symbol_includes *const si = RB_DINT( rbi.node );
  if ( rbi.inserted ) {
    si->from_sym_name = check_strdup( from_sym_name );
    rb_tree_init(
      &si->to_include_set, RB_DPTR,
      POINTER_CAST( rb_cmp_fn_t, &tidy_include_cmp_by_rel_path )
    );
  }
  PJL_DISCARD_RV( rb_tree_insert( &si->to_include_set, to_include, 0 ) );
}

/**
 * Dumps all symbol includes.
 */
static void symbol_includes_dump( void ) {
  if ( rb_tree_empty( &symbol_includes_map ) )
    return;
  verbose_section_begin( /*printed_header=*/NULL );
  verbose_printf( "configuration symbols:\n" );
  rb_iterator_t si_iter;
  rb_iterator_init( &si_iter, &symbol_includes_map );
  for ( symbol_includes const *si;
        (si = rb_iterator_next( &si_iter )) != NULL; ) {
    verbose_printf( "  \"%s\" -> [ ", si->from_sym_name );

    bool comma = false;
    rb_iterator_t ti_iter;
    rb_iterator_init( &ti_iter, &si->to_include_set );
    for ( tidy_include const *to_include;
          (to_include = rb_iterator_next( &ti_iter )) != NULL; ) {
      char delims[2];
      include_get_delims( to_include, delims );
      printf(
        "%s%c%s%c",
        true_or_set( &comma ) ? ", " : "",
        delims[0], to_include->abs_path, delims[1]
      );
    } // for
    puts( " ]" );
  }
}

/**
 * Compares two \ref tidy_include objects by their relative paths.
 *
 * @param i_include The first tidy_include.
 * @param j_include The second tidy_include.
 * @return Returns a number less than 0, 0, or greater than 0 if the relative
 * path of \a i_include is less than, equal to, or greater than the relative
 * path of \a j_include, respectively.
 */
NODISCARD
static int tidy_include_cmp_by_rel_path( tidy_include const *i_include,
                                         tidy_include const *j_include ) {
  assert( i_include != NULL );
  assert( j_include != NULL );
  return strcmp( i_include->rel_path, j_include->rel_path );
}

////////// extern functions ///////////////////////////////////////////////////

void config_init( void ) {
  ASSERT_RUN_ONCE();

  ht_init(
    &ignore_symbol_set, HT_DINT, 2.0, 10,
    POINTER_CAST( ht_cmp_fn_t, &strcmp ),
    POINTER_CAST( ht_hash_fn_t, &fnv1a_s )
  );
  rb_tree_init(
    &symbol_includes_map, RB_DINT,
    POINTER_CAST( rb_cmp_fn_t, &symbol_includes_cmp )
  );
  ATEXIT( &config_cleanup );

  bool found_at_least_1 = false;
  strbuf_t path_buf;
  strbuf_init( &path_buf );
  do {
    FILE *const config_file = config_file_find( opt_config_path, &path_buf );
    if ( config_file == NULL )
      break;
    config_parse( path_buf.str, config_file );
    fclose( config_file );
    found_at_least_1 = true;
    strbuf_reset( &path_buf );
  } while ( opt_config_layers || !found_at_least_1 );
  strbuf_cleanup( &path_buf );

  if ( !found_at_least_1 ) {
    print_error( "no configuration file found\n" );
    exit( EX_CONFIG );
  }

  if ( IS_VERBOSE( CONFIG_SYMBOLS ) )
    symbol_includes_dump();
}

bool config_is_standard_include( char const *rel_path ) {
  assert( rel_path != NULL );
  assert( path_is_relative( rel_path ) );

  return  (tidy_source_is_cxx &&
            is_standard_include( rel_path, &std_cxx_includes )) ||
          is_standard_include( rel_path, &std_c_includes );
}

CXFile config_symbol_get_include( char const *sym_name ) {
  assert( sym_name != NULL );

  symbol_includes find_si = { .from_sym_name = sym_name };
  rb_node_t const *const found_rb =
    rb_tree_find( &symbol_includes_map, &find_si );
  if ( found_rb == NULL )
    return NULL;
  symbol_includes const *const found_si = RB_DINT( found_rb );
  if ( rb_tree_empty( &found_si->to_include_set ) )
    return NULL;

  rb_iterator_t iter;
  rb_iterator_init( &iter, &found_si->to_include_set );

  tidy_include const *best_include = NULL;
  for ( tidy_include const *include;
        (include = rb_iterator_next( &iter )) != NULL; ) {
    include = include_get_proxy( include );
    if ( best_include == NULL || include->depth < best_include->depth )
      best_include = include;
    if ( best_include->depth == 0 )
      break;
  } // for

  return best_include != NULL ? best_include->file : NULL;
}

bool config_symbol_is_ignored( char const *sym_name ) {
  return ht_find( &ignore_symbol_set, sym_name ) != NULL;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
