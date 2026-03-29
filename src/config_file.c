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
#include "clang_util.h"
#include "include-tidy.h"
#include "includes.h"
#include "options.h"
#include "red_black.h"
#include "toml_lite.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>                     /* for PATH_MAX, SIZE_MAX */
#if HAVE_PWD_H
# include <pwd.h>                       /* for getpwuid() */
#endif /* HAVE_PWD_H */
#include <stdint.h>                     /* for SIZE_MAX */
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

///////////////////////////////////////////////////////////////////////////////

/**
 * Options for the config_open() function.
 */
enum config_opts {
  CONFIG_OPT_NONE              = 0,       ///< No options.
  CONFIG_OPT_ERROR_IS_FATAL    = 1 << 0,  ///< An error is fatal.
  CONFIG_OPT_IGNORE_NOT_FOUND  = 1 << 1   ///< Ignore file not found.
};
typedef enum config_opts config_opts_t;

/**
 * Mapping from a symbol name to the include file's relative path _or_ file
 * that declares it from a configuration file.
 */
struct symbol_include {
  char const   *symbol_name;            ///< Symbol name.
  union {
    char const *to_rel_path;            ///< Include relative path.
    CXFile      to_include_file;        ///< Corresponding include file.
  };
};
typedef struct symbol_include symbol_include;

// local functions
NODISCARD
static FILE*        config_open( char const*, config_opts_t );

NODISCARD
static char const*  home_dir( void );

static void         path_append( char*, size_t, char const* );
static void         proxy_parse( char const*, char const*, toml_value const* );
static void         symbol_include_cleanup( symbol_include* );
static void         symbols_parse( char const*, char const*,
                                   toml_value const* );

// local variables
static rb_tree_t symbol_include_map;    ///< Mapping from symbols to includes.

////////// local functions ////////////////////////////////////////////////////

/**
 * Cleans-up all configuration data.
 */
static void config_cleanup( void ) {
  rb_tree_cleanup(
    &symbol_include_map,
    POINTER_CAST( rb_free_fn_t, &symbol_include_cleanup )
  );
}

/**
 * Finds and opens the configuration file.
 *
 * @remarks
 * @parblock
 * The path of the configuration file is determined as follows (in priority
 * order):
 *
 *  1. The value of either the `--config` or `-c` command-line option; or:
 *  2. `$XDG_CONFIG_HOME/include-tidy` or `~/.config/include-tidy`; or:
 *  3. `$XDG_CONFIG_DIRS/include-tidy` for each path or `/etc/xdg/include-tidy`.
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
  char const *home = NULL;

  // 1. Try --config/-c command-line option.
  FILE *config_file = config_open( config_path, CONFIG_OPT_ERROR_IS_FATAL );
  if ( config_file != NULL )
    strcpy( path_buf, config_path );

  // 2. Try $XDG_CONFIG_HOME/include-tidy.toml and
  //    $HOME/.config/include-tidy.toml.
  if ( config_file == NULL && (home = home_dir()) != NULL ) {
    char const *const config_dir = null_if_empty( getenv( "XDG_CONFIG_HOME" ) );
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
      path_append( path_buf, SIZE_MAX, PACKAGE ".toml" );
      config_file = config_open( path_buf, CONFIG_OPT_IGNORE_NOT_FOUND );
      path_buf[0] = '\0';
    }
  }

  // 3. Try $XDG_CONFIG_DIRS/include-tidy and /etc/xdg/include-tidy.
  if ( config_file == NULL ) {
    char const *config_dirs = null_if_empty( getenv( "XDG_CONFIG_DIRS" ) );
    if ( config_dirs == NULL )
      config_dirs = "/etc/xdg";         // LCOV_EXCL_LINE
    for (;;) {
      char const *const next_sep = strchr( config_dirs, ':' );
      size_t const dir_len = next_sep != NULL ?
        STATIC_CAST( size_t, next_sep - config_dirs ) : strlen( config_dirs );
      if ( dir_len > 0 ) {
        strncpy_0( path_buf, config_dirs, dir_len );
        path_append( path_buf, dir_len, PACKAGE );
        config_file = config_open( path_buf, CONFIG_OPT_IGNORE_NOT_FOUND );
        path_buf[0] = '\0';
        if ( config_file != NULL )
          break;
      }
      if ( next_sep == NULL )
        break;
      config_dirs = next_sep + 1;
    } // for
  }

  return config_file;
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
static FILE* config_open( char const *path, config_opts_t opts ) {
  if ( path == NULL )
    return NULL;
  FILE *const config_file = fopen( path, "r" );
  if ( config_file == NULL ) {
    switch ( errno ) {
      case ENOENT:
        if ( (opts & CONFIG_OPT_IGNORE_NOT_FOUND) != 0 )
          break;
        FALLTHROUGH;
      default:
        if ( (opts & CONFIG_OPT_ERROR_IS_FATAL) != 0 ) {
          fatal_error( EX_NOINPUT,
            "configuration file \"%s\": %s\n", path, STRERROR()
          );
        }
        EPRINTF(
          "%s: warning: configuration file \"%s\": %s\n",
          prog_name, path, STRERROR()
        );
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
      fatal_error( EX_CONFIG,
        "%s:%u:%u required table (header) name missing\n",
        config_path, toml.loc.line, toml.loc.col
      );
    }

    enum known_keys {
      KEY_NONE    = 0,
      KEY_PROXY   = 1 << 0,
      KEY_SYMBOLS = 1 << 1,
    };
    typedef enum known_keys known_keys;

    known_keys keys = KEY_NONE;

    toml_iterator iter;
    toml_iterator_init( &table, &iter );
    for ( toml_key_value const *kv;
          (kv = toml_iterator_next( &iter )) != NULL; ) {
      if ( strcmp( kv->key, "proxy" ) == 0 ) {
        if ( (keys & KEY_SYMBOLS) != 0 )
          goto mutually_exclusive;
        proxy_parse( config_path, table.name, &kv->value );
        keys |= KEY_PROXY;
        continue;
      }
      if ( strcmp( kv->key, "symbols" ) == 0 ) {
        if ( (keys & KEY_PROXY) != 0 )
          goto mutually_exclusive;
        symbols_parse( config_path, table.name, &kv->value );
        keys |= KEY_SYMBOLS;
        continue;
      }

      fatal_error( EX_CONFIG,
        "%s:%u:%u: \"%s\": unknown key\n",
        config_path, kv->key_loc.line, kv->key_loc.col, table.name
      );

mutually_exclusive:
      fatal_error( EX_CONFIG,
        "%s:%u:%u: \"proxy\" and \"symbols\" keys are mutuall exclusive\n",
        config_path, kv->key_loc.line, kv->key_loc.col
      );
    } // for

    if ( keys == KEY_NONE ) {
      fatal_error( EX_CONFIG,
        "%s:%u:%u: required \"proxy\" or \"symbols\" key for \"%s\" missing\n",
        config_path, table.loc.line, table.loc.col, table.name
      );
    }
  } // while

  toml_table_cleanup( &table );

  if ( toml.error ) {
    fatal_error( EX_CONFIG,
      "%s:%u:%u: %s\n",
      config_path, toml.loc.line, toml.loc.col, toml_error_msg( &toml )
    );
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
 * Appends a component to a path ensuring that exactly one `/ `separates them.
 *
 * @param path The path to append to.  The buffer pointed to must be big enough
 * to hold the new path.
 * @param path_len The length of \a path.  If `SIZE_MAX`,
 * <code>strlen(</code>\a path<code>)</code> is used instead.
 * @param component The component to append.
 */
static void path_append( char *path, size_t path_len, char const *component ) {
  assert( path != NULL );
  assert( component != NULL );

  if ( path_len == SIZE_MAX )
    path_len = strlen( path );

  if ( path_len > 0 ) {
    path += path_len - 1;
    if ( *path != '/' )
      *++path = '/';
    strcpy( ++path, component );
  }
}

/**
 * If present, parses the value of a `"proxy"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table The toml_table to parse.
 */
static void proxy_parse( char const *config_path, char const *table_name,
                         toml_value const *value ) {
  assert( config_path != NULL );
  assert( table_name != NULL );
  assert( value != NULL );

  CXFile to_include_file = include_getFile( table_name );
  if ( to_include_file == NULL )
    return;
  CXFile from_include_file;

  switch ( value->type ) {
    case TOML_STRING:
      from_include_file = include_getFile( value->s );
      if ( from_include_file != NULL )
        include_add_proxy( from_include_file, to_include_file );
      break;
    case TOML_ARRAY:
      for ( unsigned i = 0; i < value->a.size; ++i ) {
        toml_value const *const a_value = &value->a.values[i];
        if ( a_value->type != TOML_STRING ) {
          fatal_error( EX_CONFIG,
            "%s:%u:%u: "
            "invalid value for \"proxy\" key array; expected string\n",
            config_path, a_value->loc.line, a_value->loc.col
          );
        }
        from_include_file = include_getFile( a_value->s );
        if ( from_include_file != NULL )
          include_add_proxy( from_include_file, to_include_file );
      } // for
      break;
    default:
      fatal_error( EX_CONFIG,
        "%s:%u:%u "
        "invalid value for \"proxy\" key; expected string or array\n",
        config_path, value->loc.line, value->loc.col
      );
  } // switch
}

/**
 * Cleans-up a symbol_include.
 *
 * @param si The symbol_include to clean up.  If NULL, does nothing.
 */
static void symbol_include_cleanup( symbol_include *si ) {
  if ( si == NULL )
    return;
  FREE( si->symbol_name );
}

/**
 * Compares two \ref symbol_include objects.
 *
 * @param i_si The first symbol_include.
 * @param j_si The second symbol_include.
 * @return Returns a number less than 0, 0, or greater than 0 if the name of \a
 * i_si is less than, equal to, or greater than the name of \a j_si,
 * respectively.
 */
NODISCARD
static int symbol_include_cmp( symbol_include const *i_si,
                               symbol_include const *j_si ) {
  assert( i_si != NULL );
  assert( j_si != NULL );
  return strcmp( i_si->symbol_name, j_si->symbol_name );
}

/**
 * Maps \a symbol_name to \a to_rel_path so that if \a symbol_name is
 * referenced, it'll require \a to_rel_path.
 *
 * @param symbol_name The name of the symbol.
 * @param to_rel_path The include file's relative path.
 */
static void symbol_include_add( char const *symbol_name,
                                char const *to_rel_path ) {
  assert( symbol_name != NULL );
  assert( to_rel_path != NULL );

  symbol_include new_si = { .symbol_name = symbol_name };
  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &symbol_include_map, &new_si, sizeof new_si );
  if ( rv_rbi.inserted ) {
    symbol_include *const si = RB_DINT( rv_rbi.node );
    *si = (symbol_include){
      .symbol_name = check_strdup( symbol_name ),
      .to_rel_path = check_strdup( to_rel_path )
    };
  }
}

/**
 * Dumps all symbol includes.
 */
static void symbol_includes_dump( void ) {
  if ( rb_tree_empty( &symbol_include_map ) )
    return;
  verbose_printf( "configuration symbols:\n" );
  rb_iterator_t iter;
  rb_iterator_init( &symbol_include_map, &iter );
  for ( symbol_include const *si; (si = rb_iterator_next( &iter )) != NULL; )
    verbose_printf( "  \"%s\" -> \"%s\"\n", si->symbol_name, si->to_rel_path );
  verbose_printf( "\n" );
}

/**
 * Resolves each symbol_include's \ref symbol_include::to_rel_path
 * "to_rel_path" to its corresponding \ref symbol_include::to_include_file
 * "to_include_file".
 */
static void symbol_includes_resolve( void ) {
  rb_iterator_t iter;
  rb_iterator_init( &symbol_include_map, &iter );
  for ( symbol_include *si; (si = rb_iterator_next( &iter )) != NULL; ) {
    CXFile to_include_file = include_getFile( si->to_rel_path );
    FREE( si->to_rel_path );
    si->to_include_file = to_include_file;
  } // for
}

/**
 * If present, parses the value of a `"symbols"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table The toml_table to parse.
 */
static void symbols_parse( char const *config_path, char const *table_name,
                           toml_value const *value ) {
  assert( config_path != NULL );
  assert( table_name != NULL );
  assert( value != NULL );

  switch ( value->type ) {
    case TOML_STRING:
      symbol_include_add( value->s, table_name );
      break;
    case TOML_ARRAY:
      for ( unsigned i = 0; i < value->a.size; ++i ) {
        toml_value const *const a_value = &value->a.values[i];
        if ( a_value->type != TOML_STRING ) {
          fatal_error( EX_CONFIG,
            "%s:%u:%u: "
            "invalid value for \"symbols\" key array; expected string\n",
            config_path, a_value->loc.line, a_value->loc.col
          );
        }
        symbol_include_add( a_value->s, table_name );
      } // for
      break;
    default:
      fatal_error( EX_CONFIG,
        "%s:%u:%u "
        "invalid value for \"symbols\" key; expected string or array\n",
        config_path, value->loc.line, value->loc.col
      );
  } // switch
}

////////// extern functions ///////////////////////////////////////////////////

CXFile config_get_symbol_include( char const *symbol_name ) {
  assert( symbol_name != NULL );

  symbol_include find_si = { .symbol_name = symbol_name };
  rb_node_t const *const found_rb =
    rb_tree_find( &symbol_include_map, &find_si );
  if ( found_rb == NULL )
    return NULL;
  symbol_include const *const found_si = RB_DINT( found_rb );
  return found_si->to_include_file;
}

void config_init( void ) {
  ASSERT_RUN_ONCE();

  rb_tree_init(
    &symbol_include_map, RB_DINT,
    POINTER_CAST( rb_cmp_fn_t, &symbol_include_cmp )
  );
  ATEXIT( &config_cleanup );

  char path_buf[ PATH_MAX ];
  FILE *const config_file = config_find( opt_config_path, path_buf );
  if ( config_file == NULL )
    return;
  config_parse( path_buf, config_file );
  fclose( config_file );
  if ( (opt_verbose & TIDY_VERBOSE_CONFIG_SYMBOLS) != 0 )
    symbol_includes_dump();

  symbol_includes_resolve();
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
