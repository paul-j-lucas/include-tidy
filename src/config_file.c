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
 * Defines functions to read an **include-tidy**(1) configuration file.
 */

// local
#include "pjl_config.h"
#include "config_file.h"
#include "clang_util.h"
#include "cli_options.h"
#include "includes.h"
#include "options.h"
#include "print.h"
#include "red_black.h"
#include "toml_lite.h"
#include "trans_unit.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <errno.h>
#include <limits.h>                     /* for PATH_MAX, SIZE_MAX */
#include <fnmatch.h>
#if HAVE_PWD_H
# include <pwd.h>                       /* for getpwuid() */
#endif /* HAVE_PWD_H */
#include <stdbool.h>
#include <stdint.h>                     /* for SIZE_MAX */
#include <stdio.h>
#include <stdlib.h>                     /* for getenv(), ... */
#include <string.h>
#include <sysexits.h>
#include <unistd.h>                     /* for geteuid() */

// libclang
#include <clang-c/Index.h>

/// @endcond

/**
 * @addtogroup config-file-group
 * @{
 */

////////// enumerations ///////////////////////////////////////////////////////

/**
 * Options for the config_open() function.
 */
enum config_opts {
  CONFIG_OPT_NONE              = 0,       ///< No options.
  CONFIG_OPT_ERROR_IS_FATAL    = 1 << 0,  ///< An error is fatal.
  CONFIG_OPT_IGNORE_NOT_FOUND  = 1 << 1   ///< Ignore file not found.
};

/**
 * Table kind.
 */
enum config_table_kind {
  TABLE_NOT_INCLUDE_TIDY,               ///< Not the `include-tidy` table.
  TABLE_INCLUDE_TIDY                    ///< Only the `include-tidy` table.
};

////////// typedefs ///////////////////////////////////////////////////////////

typedef enum    config_opts       config_opts;
typedef struct  config_key        config_key;
typedef enum    config_table_kind config_table_kind;
typedef struct  symbol_includes   symbol_includes;

/**
 * Signature for function to parse the value of a configuration key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table The current toml_table.
 * @param value The toml_value to parse.
 */
typedef void (*config_parse_fn)( char const *config_path,
                                 toml_table const *table,
                                 toml_value *value );

////////// structures /////////////////////////////////////////////////////////

/**
 * Configuration key (in a table).
 */
struct config_key {
  char const       *name;               ///< Key name.
  config_table_kind table_kind;         ///< Table kind allowed in.
  config_parse_fn   parse_fn;           ///< Value parsing function.
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
  char const *from_symbol_name;         ///< Symbol name.
  rb_tree_t   to_include_files;         ///< Include file(s).
};

////////// local functions ////////////////////////////////////////////////////

static void         add_c_includes_parse( char const*, toml_table const*,
                                          toml_value* );
static void         align_column_parse( char const*, toml_table const*,
                                        toml_value* );
static void         all_includes_parse( char const*, toml_table const*,
                                        toml_value* );

NODISCARD
static bool         bool_value_parse( char const*, char const*, toml_value* );

static void         color_parse( char const*, toml_table const*, toml_value* );
static void         comment_style_parse( char const*, toml_table const*,
                                         toml_value* );

NODISCARD
static config_key const*
                    config_key_parse( char const* );

NODISCARD
static FILE*        config_open( char const*, config_opts );

NODISCARD
static char const*  home_dir( void );

static void         first_parse( char const*, toml_table const*, toml_value* );
static void         includes_parse( char const*, toml_table const*,
                                    toml_value* );

NODISCARD
static long         int_value_parse( char const*, char const*, toml_value*,
                                     long, long );

static void         keep_parse( char const*, toml_table const*, toml_value* );
static void         line_length_parse( char const*, toml_table const*,
                                       toml_value* );
static void         proxy_parse( char const*, toml_table const*, toml_value* );
static void         std_c_includes_parse( char const*, toml_table const*,
                                          toml_value* );
static void         std_cpp_includes_parse( char const*, toml_table const*,
                                            toml_value* );

NODISCARD
static char**       string_array_value_parse( char const*, char const*,
                                              toml_value* );

NODISCARD
static char const*  string_value_parse( char const*, char const*, toml_value* );

static void         symbol_include_add( char const*, CXFile );
static void         symbol_include_cleanup( symbol_includes* );
static void         symbols_parse( char const*, toml_table const*,
                                   toml_value* );

////////// local constants ////////////////////////////////////////////////////

/**
 * Configuration keys.
 */
static config_key const CONFIG_KEYS[] = {
  { "add-c-includes",   TABLE_INCLUDE_TIDY,     &add_c_includes_parse   },
  { "align-column",     TABLE_INCLUDE_TIDY,     &align_column_parse     },
  { "all-includes",     TABLE_INCLUDE_TIDY,     &all_includes_parse     },
  { "color",            TABLE_INCLUDE_TIDY,     &color_parse            },
  { "comment-style",    TABLE_INCLUDE_TIDY,     &comment_style_parse    },
  { "first",            TABLE_NOT_INCLUDE_TIDY, &first_parse            },
  { "includes",         TABLE_NOT_INCLUDE_TIDY, &includes_parse         },
  { "keep",             TABLE_NOT_INCLUDE_TIDY, &keep_parse             },
  { "line-length",      TABLE_INCLUDE_TIDY,     &line_length_parse      },
  { "proxy",            TABLE_NOT_INCLUDE_TIDY, &proxy_parse            },
  { "std-c-includes",   TABLE_INCLUDE_TIDY,     &std_c_includes_parse   },
  { "std-cpp-includes", TABLE_INCLUDE_TIDY,     &std_cpp_includes_parse },
  { "symbols",          TABLE_NOT_INCLUDE_TIDY, &symbols_parse          },
};

////////// local variables ////////////////////////////////////////////////////

static char   **std_c_includes;         ///< Standard-ish C include files.
static size_t   std_c_includes_size;    ///< Size of \ref std_c_includes.
static char   **std_cpp_includes;       ///< Standard C++ include files.
static bool     verbose_printed_any;    ///< Print any configuration files?

/**
 * Mapping from symbols to the include file(s) they're declared in.
 *
 * @sa symbol_includes
 */
static rb_tree_t symbol_include_map;

////////// local functions ////////////////////////////////////////////////////

/**
 * Parses the value of an `"add-c-includes"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table Not used.
 * @param value The toml_value to parse.
 */
static void add_c_includes_parse( char const *config_path,
                                  toml_table const *table, toml_value *value ) {
  assert( config_path != NULL );
  (void)table;
  assert( value != NULL );

  char **const add_c_includes =
    string_array_value_parse( config_path, "add-c-includes", value );
  REALLOC( std_c_includes, std_c_includes_size + value->a.size + 1 );
  memcpy( std_c_includes + std_c_includes_size, add_c_includes,
          value->a.size * sizeof std_c_includes[0] );
  free( add_c_includes );
  std_c_includes_size += value->a.size;
  std_c_includes[ std_c_includes_size ] = NULL;
}

/**
 * Parses the value of an `"align-column"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table Not used.
 * @param value The toml_value to parse.
 */
static void align_column_parse( char const *config_path,
                                toml_table const *table, toml_value *value ) {
  assert( config_path != NULL );
  (void)table;
  assert( value != NULL );

  long const int_value = int_value_parse(
    config_path, "align-column", value, 0, OPT_ALIGN_COLUMN_MAX
  );

  if ( !opt_is_set( COPT(ALIGN_COLUMN) ) ) {
    opt_align_column = STATIC_CAST( unsigned, int_value );
    opt_mark_set( COPT(ALIGN_COLUMN) );
  }
}

/**
 * Parses the value of an `"all-includes"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table Not used.
 * @param value The toml_value to parse.
 */
static void all_includes_parse( char const *config_path,
                                toml_table const *table, toml_value *value ) {
  assert( config_path != NULL );
  (void)table;
  assert( value != NULL );

  if ( !opt_is_set( COPT(ALL_INCLUDES) ) ) {
    opt_all_includes = bool_value_parse( config_path, "all-includes", value );
    opt_mark_set( COPT(ALL_INCLUDES) );
  }
}

/**
 * Cleans-up all configuration data.
 */
static void config_cleanup( void ) {
  if ( std_c_includes != NULL ) {
    for ( char **ppattern = std_c_includes; *ppattern != NULL; ++ppattern )
      free( *ppattern );
    free( std_c_includes );
  }
  if ( std_cpp_includes != NULL ) {
    for ( char **ppattern = std_cpp_includes; *ppattern != NULL; ++ppattern )
      free( *ppattern );
    free( std_cpp_includes );
  }
  rb_tree_cleanup(
    &symbol_include_map,
    POINTER_CAST( rb_free_fn_t, &symbol_include_cleanup )
  );
}

/**
 * Parses a bool value.
 *
 * @param config_path The full path to the configurarion file.
 * @param key_name The key name.
 * @param value The toml_value to parse.
 */
static bool bool_value_parse( char const *config_path, char const *key_name,
                              toml_value *value ) {
  assert( config_path != NULL );
  assert( key_name != NULL );
  assert( value != NULL );

  if ( value->type != TOML_BOOL ) {
    print_error(
      config_path, value->loc.line, value->loc.col,
      "invalid value for \"%s\"; expected boolean\n", key_name
    );
    exit( EX_CONFIG );
  }

  return value->b;
}

/**
 * Parses the value of an `"color"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table Not used.
 * @param value The toml_value to parse.
 */
static void color_parse( char const *config_path, toml_table const *table,
                         toml_value *value ) {
  assert( config_path != NULL );
  (void)table;
  assert( value != NULL );

  char const *const string_value =
    string_value_parse( config_path, "color", value );

  if ( opt_is_set( COPT(COLOR) ) )
    return;

  if ( !opt_color_parse( string_value ) ) {
    print_error(
      config_path, value->loc.line, value->loc.col,
      "invalid value for \"color\"\n"
    );
    exit( EX_CONFIG );
  }
  opt_mark_set( COPT(COLOR) );
}

/**
 * Parses the value of an `"comment-style"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table Not used.
 * @param value The toml_value to parse.
 */
static void comment_style_parse( char const *config_path,
                                 toml_table const *table, toml_value *value ) {
  assert( config_path != NULL );
  (void)table;
  assert( value != NULL );

  char const *const string_value =
    string_value_parse( config_path, "comment-style", value );

  if ( opt_is_set( COPT(COMMENT_STYLE) ) )
    return;

  if ( !opt_comment_style_parse( string_value ) ) {
    print_error(
      config_path, value->loc.line, value->loc.col,
      "invalid value for \"comment-style\";"
      " must be one of \"//\", \"/*\", or \"none\"\n"
    );
    exit( EX_CONFIG );
  }
  opt_mark_set( COPT(COMMENT_STYLE) );
}

/**
 * Finds and opens the configuration file.
 *
 * @remarks
 * @parblock
 * The path of the configuration file is determined as follows (in priority
 * order):
 *
 *  1. The value of either the `--config` or `-c` command-line option.
 *  2. `$PWD/include-tidy.toml`.
 *  3. `$XDG_CONFIG_HOME/include-tidy/config.toml`.  If `XDG_CONFIG_HOME` is
 *     empty or unset, then ``~/.config/` is used.
 *  4. `$XDG_CONFIG_DIRS/include-tidy/config.toml` for each path.  If
 *     `XDG_CONFIG_DIRS` is empty or unset, then `/etc/xdg` is used.
 * @endparblock
 *
 * @param config_path The full path to a configuration file.  May be NULL.
 * @param path_buf A path buffer to use.  It _must_ be initialized to the empty
 * string.  Upon return, it contains the full path of the configuration file
 * that was found, if any.
 * @return Returns the `FILE*` for the configuration file if found or NULL if
 * not.
 */
NODISCARD
static FILE* config_find( char const *config_path,
                          char path_buf[static PATH_MAX] ) {
  static unsigned case_num = 1;

  FILE *config_file = NULL;
  char const *home = NULL;

  switch ( case_num ) {
    case 1:
      // Try --config/-c command-line option.
      ++case_num;
      config_file = config_open( config_path, CONFIG_OPT_ERROR_IS_FATAL );
      if ( config_file != NULL ) {
        strcpy( path_buf, config_path );
        break;
      }
      FALLTHROUGH;

    case 2:
      // Try $PWD/include-tidy.toml
      ++case_num;
      size_t cwd_len;
      strcpy( path_buf, get_cwd( &cwd_len ) );
      path_append( path_buf, cwd_len, PACKAGE ".toml" );
      config_file = config_open( path_buf, CONFIG_OPT_IGNORE_NOT_FOUND );
      if ( config_file != NULL )
        break;
      FALLTHROUGH;

    case 3:
      // Try $XDG_CONFIG_HOME/include-tidy/config.toml or
      // $HOME/.config/include-tidy/config.toml.
      ++case_num;
      if ( (home = home_dir()) != NULL ) {
        char const *const config_dir =
          null_if_empty( getenv( "XDG_CONFIG_HOME" ) );
        if ( config_dir != NULL ) {
          strcpy( path_buf, config_dir );
        }
        else if ( home != NULL ) {
          // LCOV_EXCL_START
          strcpy( path_buf, home );
          path_append( path_buf, SIZE_MAX, ".config" );
          // LCOV_EXCL_STOP
        }
        if ( path_buf[0] != '\0' ) {
          path_append( path_buf, SIZE_MAX, PACKAGE );
          path_append( path_buf, SIZE_MAX, "config.toml" );
          config_file = config_open( path_buf, CONFIG_OPT_IGNORE_NOT_FOUND );
          if ( config_file != NULL )
            break;
        }
      }
      FALLTHROUGH;

    case 4:
      // Try $XDG_CONFIG_DIRS/include-tidy/config.toml or
      // /etc/xdg/include-tidy/config.toml.
      ++case_num;
      char const *config_dirs = null_if_empty( getenv( "XDG_CONFIG_DIRS" ) );
      if ( config_dirs == NULL )
        config_dirs = "/etc/xdg";       // LCOV_EXCL_LINE
      for (;;) {
        char const *const next_sep = strchr( config_dirs, ':' );
        size_t dir_len = next_sep != NULL ?
          STATIC_CAST( size_t, next_sep - config_dirs ) :
          strlen( config_dirs );
        if ( dir_len > 0 ) {
          strncpy_0( path_buf, config_dirs, dir_len );
          path_append( path_buf, dir_len, PACKAGE );
          dir_len += STRLITLEN( PACKAGE );
          path_append( path_buf, dir_len, "config.toml" );
          config_file = config_open( path_buf, CONFIG_OPT_IGNORE_NOT_FOUND );
          path_buf[0] = '\0';
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
 * Parses a configuration key string.
 *
 * @param s The string to parse.
 * @return Returns a pointer to the corresponding config_key or NULL if \a s
 * does not correspond to a key.
 */
static config_key const* config_key_parse( char const *s ) {
  assert( s != NULL );

  FOREACH_ARRAY_ELEMENT( config_key, key, CONFIG_KEYS ) {
    if ( strcmp( s, key->name ) == 0 )
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

  if ( (opt_verbose & TIDY_VERBOSE_CONFIG_FILES) != 0 ) {
    if ( false_set( &verbose_printed_any ) )
      verbose_printf( "configuration files:\n" );
    verbose_printf( "  \"%s\": %s\n", path, ok ? "OK" : STRERROR() );
  }

  if ( !ok ) {
    switch ( errno ) {
      case ENOENT:
        if ( (opts & CONFIG_OPT_IGNORE_NOT_FOUND) != 0 )
          break;
        FALLTHROUGH;
      default:
        if ( (opts & CONFIG_OPT_ERROR_IS_FATAL) != 0 ) {
          print_error( path, 0, 0, "%s\n", STRERROR() );
          exit( EX_NOINPUT );
        }
        print_warning( path, 0, 0, "%s\n", STRERROR() );
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
  toml_init( &toml, config_file );

  toml_table table;
  toml_table_init( &table );

  while ( toml_table_next( &toml, &table ) ) {
    if ( table.name == NULL ) {
      print_error(
        config_path, toml.loc.line, toml.loc.col,
        "required table name missing\n"
      );
      exit( EX_CONFIG );
    }
    if ( toml_table_empty( &table ) ) {
      print_error(
        config_path, table.loc.line, table.loc.col,
        "\"%s\": empty table\n", table.name
      );
      exit( EX_CONFIG );
    }

    bool const is_include_tidy_table =
      strcmp( table.name, "include-tidy" ) == 0;

    toml_iterator   iter;

    toml_iterator_init( &table, &iter );
    for ( toml_key_value *kv; (kv = toml_iterator_next( &iter )) != NULL; ) {
      config_key const *const key = config_key_parse( kv->key );
      if ( key == NULL ) {
        print_error(
          config_path, kv->key_loc.line, kv->key_loc.col,
          "\"%s\": unknown key\n", table.name
        );
        exit( EX_CONFIG );
      }

      if ( key->table_kind != is_include_tidy_table ) {
        print_error(
          config_path, kv->key_loc.line, kv->key_loc.col,
          "\"%s\": key %s in \"include-tidy\" table\n",
          key->name, key->table_kind ? "only allowed" : "not allowed"
        );
        exit( EX_CONFIG );
      }

      (*key->parse_fn)( config_path, &table, &kv->value );
    } // for
  } // while

  toml_table_cleanup( &table );

  if ( toml.error ) {
    print_error(
      config_path, toml.loc.line, toml.loc.col,
      "%s\n", toml_error_msg( &toml )
    );
    exit( EX_CONFIG );
  }

  toml_cleanup( &toml );
}

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

/**
 * Parses the value of an `"first"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table The current toml_table.
 * @param value The toml_value to parse.
 */
static void first_parse( char const *config_path, toml_table const *table,
                         toml_value*value ) {
  assert( config_path != NULL );
  assert( table != NULL );
  assert( value != NULL );

  bool const first = bool_value_parse( config_path, "first", value );
  if ( !first )
    return;

  CXFile include_file = include_get_File( table->name );
  if ( include_file == NULL )
    return;
  tidy_include *const include = include_find( include_file );
  assert( include != NULL );
  include->sort_rank = -2;
}

/**
 * Adds a proxy from \a from_include_file to \a to_include_file.
 *
 * @param from_include_file The file to add the proxy from.
 * @param to_include_file The file to add the proxy to.
 */
static void include_add_explicit_proxy( char const *config_path,
                                        CXFile from_include_file,
                                        toml_loc const *from_loc,
                                        CXFile to_include_file ) {
  assert( config_path != NULL );
  assert( from_include_file != NULL );
  assert( from_loc != NULL );
  assert( to_include_file != NULL );

  tidy_include *const from_include = include_find( from_include_file );
  if ( from_include == NULL )
    return;
  tidy_include *const to_include = include_find( to_include_file );
  if ( to_include == NULL )
    return;

  if ( from_include->proxy != NULL ) {
    print_warning(
      config_path, from_loc->line, from_loc->col,
      "\"%s\" already has proxy \"%s\"\n",
      from_include->rel_path, from_include->proxy->rel_path
    );
  }
  else if ( include_proxy_would_cycle( from_include, to_include ) ) {
    print_warning(
      config_path, from_loc->line, from_loc->col,
      "\"%s\": proxy cycle detected\n", from_include->rel_path
    );
  }
  else {
    from_include->proxy = to_include;
    from_include->is_proxy_explicit = true;
  }
}

/**
 * Parses the value of an `"includes"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table The current toml_table.
 * @param value The toml_value to parse.
 */
static void includes_parse( char const *config_path, toml_table const *table,
                            toml_value *value ) {
  assert( config_path != NULL );
  assert( table != NULL );
  assert( value != NULL );

  CXFile to_include_file;

  switch ( value->type ) {
    case TOML_STRING:
      to_include_file = include_get_File( value->s );
      if ( to_include_file != NULL )
        symbol_include_add( table->name, to_include_file );
      break;
    case TOML_ARRAY:
      for ( unsigned i = 0; i < value->a.size; ++i ) {
        toml_value const *const a_value = &value->a.values[i];
        if ( a_value->type != TOML_STRING ) {
          print_error(
            config_path, a_value->loc.line, a_value->loc.col,
            "invalid value for \"includes\"; expected string\n",
          );
          exit( EX_CONFIG );
        }
        to_include_file = include_get_File( a_value->s );
        if ( to_include_file != NULL )
          symbol_include_add( table->name, to_include_file );
      } // for
      break;
    default:
      print_error(
        config_path, value->loc.line, value->loc.col,
        "invalid value for \"includes\"; expected string or array\n"
      );
      exit( EX_CONFIG );
  } // switch
}

/**
 * Parses an integer value.
 *
 * @param config_path The full path to the configurarion file.
 * @param key_name The key name.
 * @param value The toml_value to parse.
 * @param value_min The minimum allowed value.
 * @param value_max The maximum allowed value.
 */
static long int_value_parse( char const *config_path, char const *key_name,
                             toml_value *value,
                             long value_min, long value_max ) {
  assert( config_path != NULL );
  assert( key_name != NULL );
  assert( value != NULL );

  if ( value->type != TOML_INT ) {
    print_error(
      config_path, value->loc.line, value->loc.col,
      "invalid value for \"%s\"; expected integer\n", key_name
    );
    exit( EX_CONFIG );
  }

  if ( value->i < value_min || value->i > value_max ) {
    print_error(
      config_path, value->loc.line, value->loc.col,
      "\"%ld\": invalid value for \"%s\"; must be %ld-%ld\n",
      value->i, key_name, value_min, value_max
    );
    exit( EX_CONFIG );
  }

  return value->i;
}

/**
 * Gets whether \a rel_path is among \a includes.
 *
 * @param rel_path The relative path of an include file, e.g., `"stdio.h"` or
 * `"sys/wait.h"`.
 * @param includes A NULL-terminated array of include paths.
 * @return Returns `true` only if \a rel_path is among \a includes.
 */
NODISCARD
static bool is_standard_include( char const *rel_path, char *includes[] ) {
  assert( rel_path != NULL );
  assert( path_is_relative( rel_path ) );

  if ( includes != NULL ) {
    for ( char **ppattern = includes; *ppattern != NULL; ++ppattern ) {
      int const flags = strstr( *ppattern, "**" ) == NULL ? FNM_PATHNAME : 0;
      switch ( fnmatch( *ppattern, rel_path, flags ) ) {
        case 0:
          return true;
        case FNM_NOMATCH:
          continue;
        default:
          assert( false && "fnmatch() error" );
      } // switch
    } // for
  }

  return false;
}

/**
 * Parses the value of a `"keep"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table The current toml_table.
 * @param value The toml_value to parse.
 */
static void keep_parse( char const *config_path, toml_table const *table,
                        toml_value *value ) {
  assert( config_path != NULL );
  assert( table != NULL );
  assert( value != NULL );

  bool const keep = bool_value_parse( config_path, "keep", value );
  if ( !keep )
    return;

  CXFile include_file = include_get_File( table->name );
  if ( include_file == NULL )
    return;
  tidy_include *const include = include_find( include_file );
  assert( include != NULL );
  include->is_needed = true;
}

/**
 * Parses the value of a `"line-length"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table Not used.
 * @param value The toml_value to parse.
 */
static void line_length_parse( char const *config_path,
                               toml_table const *table, toml_value *value ) {
  assert( config_path != NULL );
  (void)table;
  assert( value != NULL );

  long const int_value = int_value_parse(
    config_path, "line-length", value, 0, OPT_LINE_LENGTH_MAX
  );

  if ( !opt_is_set( COPT(LINE_LENGTH) ) ) {
    opt_line_length = STATIC_CAST( unsigned, int_value );
    opt_mark_set( COPT(LINE_LENGTH) );
  }
}

/**
 * Parses the value of a `"proxy"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table The current toml_table.
 * @param value The toml_value to parse.
 */
static void proxy_parse( char const *config_path, toml_table const *table,
                         toml_value *value ) {
  assert( config_path != NULL );
  assert( table != NULL );
  assert( value != NULL );

  CXFile to_include_file = include_get_File( table->name );
  if ( to_include_file == NULL )
    return;
  CXFile from_include_file;

  switch ( value->type ) {
    case TOML_STRING:
      from_include_file = include_get_File( value->s );
      if ( from_include_file != NULL ) {
        include_add_explicit_proxy(
          config_path, from_include_file, &value->loc, to_include_file
        );
      }
      break;
    case TOML_ARRAY:
      for ( unsigned i = 0; i < value->a.size; ++i ) {
        toml_value const *const a_value = &value->a.values[i];
        if ( a_value->type != TOML_STRING ) {
          print_error(
            config_path, a_value->loc.line, a_value->loc.col,
            "invalid value for \"proxy\" key array; expected string\n"
          );
          exit( EX_CONFIG );
        }
        from_include_file = include_get_File( a_value->s );
        if ( from_include_file != NULL ) {
          include_add_explicit_proxy(
            config_path, from_include_file, &a_value->loc, to_include_file
          );
        }
      } // for
      break;
    default:
      print_error(
        config_path, value->loc.line, value->loc.col,
        "invalid value for \"proxy\" key; expected string or array\n"
      );
      exit( EX_CONFIG );
  } // switch
}

/**
 * Parses the value of an `"std-c-includes"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table Not used.
 * @param value The toml_value to parse.
 */
static void std_c_includes_parse( char const *config_path,
                              toml_table const *table, toml_value *value ) {
  assert( config_path != NULL );
  (void)table;
  assert( value != NULL );

  if ( std_c_includes == NULL ) {
    std_c_includes =
      string_array_value_parse( config_path, "std-c-includes", value );
    std_c_includes_size = value->a.size;
  }
}

/**
 * Parses the value of an `"std-cpp-includes"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table Not used.
 * @param value The toml_value to parse.
 */
static void std_cpp_includes_parse( char const *config_path,
                              toml_table const *table, toml_value *value ) {
  assert( config_path != NULL );
  (void)table;
  assert( value != NULL );

  if ( std_cpp_includes == NULL ) {
    std_cpp_includes =
      string_array_value_parse( config_path, "std-cpp-includes", value );
  }
}

/**
 * Parses an array of strings values.
 *
 * @param config_path The full path to the configurarion file.
 * @param key_name The key name.
 * @param value The toml_value to parse.
 * @return Returns a pointer to the first element of a null-terminated string
 * array or NULL if the array is empty.
 */
static char** string_array_value_parse( char const *config_path,
                                        char const *key_name,
                                        toml_value *value ) {
  assert( config_path != NULL );
  assert( key_name != NULL );
  assert( value != NULL );

  if ( value->type != TOML_ARRAY ) {
    print_error(
      config_path, value->loc.line, value->loc.col,
      "invalid value for \"%s\"; expected array\n", key_name
    );
    exit( EX_CONFIG );
  }

  if ( value->a.size == 0 )
    return NULL;
  char **const array = MALLOC( char*, value->a.size + 1/*NULL*/ );

  for ( unsigned i = 0; i < value->a.size; ++i ) {
    toml_value *const a_value = &value->a.values[i];
    if ( a_value->type != TOML_STRING ) {
      print_error(
        config_path, a_value->loc.line, a_value->loc.col,
        "invalid value for \"%s\"; expected string\n", key_name
      );
      exit( EX_CONFIG );
    }
    array[i] = a_value->s;
    a_value->s = NULL;                  // steal value's string
  } // for

  array[ value->a.size ] = NULL;
  return array;
}

/**
 * Parses a string value.
 *
 * @param config_path The full path to the configurarion file.
 * @param key_name The key name.
 * @param value The toml_value to parse.
 */
static char const* string_value_parse( char const *config_path,
                                       char const *key_name,
                                       toml_value *value ) {
  assert( config_path != NULL );
  assert( key_name != NULL );
  assert( value != NULL );

  if ( value->type != TOML_STRING ) {
    print_error(
      config_path, value->loc.line, value->loc.col,
      "invalid value for \"%s\"; expected string\n", key_name
    );
    exit( EX_CONFIG );
  }

  return value->s;
}

/**
 * Cleans-up a symbol_includes.
 *
 * @param si The symbol_includes to clean up.  If NULL, does nothing.
 */
static void symbol_include_cleanup( symbol_includes *si ) {
  if ( si == NULL )
    return;
  FREE( si->from_symbol_name );
  rb_tree_cleanup( &si->to_include_files, /*free_fn=*/NULL );
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
static int symbol_include_cmp( symbol_includes const *i_si,
                               symbol_includes const *j_si ) {
  assert( i_si != NULL );
  assert( j_si != NULL );
  return strcmp( i_si->from_symbol_name, j_si->from_symbol_name );
}

/**
 * Maps \a from_symbol_name to \a to_include_file so that if \a
 * from_symbol_name is referenced, it'll require \a to_include_file.
 *
 * @param from_symbol_name The name of the symbol.
 * @param to_include_file The include file that supposedly declares it.
 */
static void symbol_include_add( char const *from_symbol_name,
                                CXFile to_include_file ) {
  assert( from_symbol_name != NULL );
  assert( to_include_file != NULL );

  symbol_includes new_si = { .from_symbol_name = from_symbol_name };
  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &symbol_include_map, &new_si, sizeof new_si );
  symbol_includes *const si = RB_DINT( rv_rbi.node );
  if ( rv_rbi.inserted ) {
    si->from_symbol_name = check_strdup( from_symbol_name );
    rb_tree_init(
      &si->to_include_files, RB_DINT,
      POINTER_CAST( rb_cmp_fn_t, &tidy_CXFile_cmp )
    );
  }
  PJL_DISCARD_RV(
    rb_tree_insert(
      &si->to_include_files, &to_include_file, sizeof to_include_file
    )
  );
}

/**
 * Dumps all symbol includes.
 */
static void symbol_includes_dump( void ) {
  if ( rb_tree_empty( &symbol_include_map ) )
    return;
  verbose_printf( "configuration symbols:\n" );
  rb_iterator_t si_iter;
  rb_iterator_init( &symbol_include_map, &si_iter );
  for ( symbol_includes const *si;
        (si = rb_iterator_next( &si_iter )) != NULL; ) {
    verbose_printf( "  \"%s\" -> [ ", si->from_symbol_name );

    bool comma = false;
    rb_iterator_t tif_iter;
    rb_iterator_init( &si->to_include_files, &tif_iter );
    for ( CXFile *pto_include_file;
          (pto_include_file = rb_iterator_next( &tif_iter )) != NULL; ) {
      CXString const abs_path_cxs =
        tidy_File_getRealPathName( *pto_include_file );
      char const *const abs_path = clang_getCString( abs_path_cxs );
      printf( "%s\"%s\"", true_or_set( &comma ) ? ", " : "", abs_path );
      clang_disposeString( abs_path_cxs );
    } // for
    puts( " ]" );
  }
  verbose_printf( "\n" );
}

/**
 * Parses the value of a `"symbols"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table The current toml_table.
 * @param value The toml_value to parse.
 */
static void symbols_parse( char const *config_path, toml_table const *table,
                           toml_value *value ) {
  assert( config_path != NULL );
  assert( table != NULL );
  assert( value != NULL );

  CXFile to_include_file = include_get_File( table->name );
  if ( to_include_file == NULL )
    return;

  switch ( value->type ) {
    case TOML_STRING:
      symbol_include_add( value->s, to_include_file );
      break;
    case TOML_ARRAY:
      for ( unsigned i = 0; i < value->a.size; ++i ) {
        toml_value const *const a_value = &value->a.values[i];
        if ( a_value->type != TOML_STRING ) {
          print_error(
            config_path, a_value->loc.line, a_value->loc.col,
            "invalid value for \"symbols\" key array; expected string\n"
          );
          exit( EX_CONFIG );
        }
        symbol_include_add( a_value->s, to_include_file );
      } // for
      break;
    default:
      print_error(
        config_path, value->loc.line, value->loc.col,
        "invalid value for \"symbols\" key; expected string or array\n"
      );
      exit( EX_CONFIG );
  } // switch
}

////////// extern functions ///////////////////////////////////////////////////

CXFile config_get_symbol_include( char const *symbol_name ) {
  assert( symbol_name != NULL );

  symbol_includes find_si = { .from_symbol_name = symbol_name };
  rb_node_t const *const found_rb =
    rb_tree_find( &symbol_include_map, &find_si );
  if ( found_rb == NULL )
    return NULL;
  symbol_includes const *const found_si = RB_DINT( found_rb );

  rb_iterator_t iter;
  rb_iterator_init( &found_si->to_include_files, &iter );
  for ( CXFile *pto_include_file;
        (pto_include_file = rb_iterator_next( &iter )) != NULL; ) {
    tidy_include const *const include = include_find( *pto_include_file );
    if ( include != NULL && include->depth == 0 )
      return *pto_include_file;
  } // for

  return NULL;
}

void config_init( void ) {
  ASSERT_RUN_ONCE();

  rb_tree_init(
    &symbol_include_map, RB_DINT,
    POINTER_CAST( rb_cmp_fn_t, &symbol_include_cmp )
  );
  ATEXIT( &config_cleanup );

  bool found_at_least_1 = false;
  do {
    char path_buf[ PATH_MAX ];
    FILE *const config_file = config_find( opt_config_path, path_buf );
    if ( config_file == NULL )
      break;
    config_parse( path_buf, config_file );
    fclose( config_file );
    found_at_least_1 = true;
  } while ( opt_config_layers || !found_at_least_1 );

  if ( !found_at_least_1 ) {
    print_error( "include-tidy.toml", 0, 0, "no configuration file found\n" );
    exit( EX_CONFIG );
  }

  if ( verbose_printed_any )
    verbose_printf( "\n" );

  if ( (opt_verbose & TIDY_VERBOSE_CONFIG_SYMBOLS) != 0 )
    symbol_includes_dump();
}

bool config_is_standard_include( char const *rel_path ) {
  assert( rel_path != NULL );
  assert( path_is_relative( rel_path ) );

  return  (tidy_lang == CXLanguage_CPlusPlus &&
           is_standard_include( rel_path, std_cpp_includes )) ||
          is_standard_include( rel_path, std_c_includes );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
