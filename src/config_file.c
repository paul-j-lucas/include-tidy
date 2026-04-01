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

////////// enumerations ///////////////////////////////////////////////////////

/**
 * Configuration file key kinds.
 */
enum config_key_kind {
  CONFIG_KEY_NONE,                      ///< No key.
  CONFIG_KEY_CONFIG_NEXT  = 1 << 0,     ///< `config_next`.
  CONFIG_KEY_INCLUDES     = 1 << 0,     ///< `includes`.
  CONFIG_KEY_PROXY        = 1 << 1,     ///< `proxy`.
  CONFIG_KEY_SYMBOLS      = 1 << 2,     ///< `symbols`.
};

/**
 * Connfiguration file locations.
 */
enum config_locs {
  CONFIG_LOC_CLI,                       ///< Command line.
  CONFIG_LOC_CWD,                       ///< Current working directory.
  CONFIG_LOC_XDG_CONFIG_HOME,           ///< `$XDG_CONFIG_HOME`.
  CONFIG_LOC_XDG_CONFIG_DIRS            ///< `$XDG_CONFIG_DIRS`.
};

/**
 * Last value of config_locs.
 */
#define CONFIG_LOC_LAST   CONFIG_LOC_XDG_CONFIG_DIRS

/**
 * Options for the config_open() function.
 */
enum config_opts {
  CONFIG_OPT_NONE              = 0,       ///< No options.
  CONFIG_OPT_ERROR_IS_FATAL    = 1 << 0,  ///< An error is fatal.
  CONFIG_OPT_IGNORE_NOT_FOUND  = 1 << 1   ///< Ignore file not found.
};

////////// typedefs ///////////////////////////////////////////////////////////

typedef enum    config_opts     config_opts;
typedef enum    config_key_kind config_key_kind;
typedef struct  config_key      config_key;
typedef enum    config_locs     config_locs;
typedef struct  symbol_include  symbol_include;

/**
 * Signature for function to parse the value of a configuration key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table_name The TOML table name.
 * @param value The toml_value to parse.
 */
typedef void (*config_parse_fn)( char const *config_path,
                                 char const *table_name,
                                 toml_value const *value );

////////// structures /////////////////////////////////////////////////////////

/**
 * Configuration key (in a table).
 */
struct config_key {
  char const       *name;               ///< Key name.
  config_key_kind   kind;               ///< Key kind.
  config_parse_fn   parse_fn;           ///< Value parsing function.
};

/**
 * Mapping from a symbol name to a set of include CXFiles that declare it from
 * a configuration file.
 */
struct symbol_include {
  char const *from_symbol_name;         ///< Symbol name.
  rb_tree_t   to_include_files;         ///< Include file(s).
};

// local functions
NODISCARD
static config_key const*
                    config_key_parse( char const* );

static void         config_next_parse( char const*, char const*,
                                       toml_value const* );
NODISCARD
static FILE*        config_open( char const*, config_opts );

NODISCARD
static char const*  home_dir( void );

static void         includes_parse( char const*, char const*,
                                    toml_value const* );
static void         path_append( char*, size_t, char const* );
static void         proxy_parse( char const*, char const*, toml_value const* );
static void         symbol_include_add( char const*, CXFile );
static void         symbol_include_cleanup( symbol_include* );
static void         symbols_parse( char const*, char const*,
                                   toml_value const* );

// local variables
static config_locs  config_loc;         ///< Configuration file location.
static bool         config_next = true; ///< Read next configuration file?
static rb_tree_t    symbol_include_map; ///< Mapping from symbols to includes.

/**
 * Configuration keys.
 */
static config_key const CONFIG_KEYS[] = {
  { "config-next",  CONFIG_KEY_CONFIG_NEXT, &config_next_parse  },
  { "includes",     CONFIG_KEY_INCLUDES,    &includes_parse     },
  { "proxy",        CONFIG_KEY_PROXY,       &proxy_parse        },
  { "symbols",      CONFIG_KEY_SYMBOLS,     &symbols_parse      },
};

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
 *  2. `$PWD/include-tidy.toml`; or:
 *  3. `$XDG_CONFIG_HOME/include-tidy.toml` or `~/.config/include-tidy.toml`;
 *     or:
 *  4. `$XDG_CONFIG_DIRS/include-tidy.toml` for each path or
 *     `/etc/xdg/include-tidy.toml`.
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
  FILE *config_file = NULL;
  char const *home = NULL;

  switch ( config_loc ) {
    case CONFIG_LOC_CLI:
      // 1. Try --config/-c command-line option.
      config_file = config_open( config_path, CONFIG_OPT_ERROR_IS_FATAL );
      if ( config_file != NULL )
        strcpy( path_buf, config_path );
      FALLTHROUGH;

    case CONFIG_LOC_CWD:
      // 2. Try $PWD/include-tidy.toml
      if ( config_file == NULL ) {
        size_t cwd_len;
        strcpy( path_buf, get_cwd( &cwd_len ) );
        path_append( path_buf, cwd_len, PACKAGE ".toml" );
        config_file = config_open( path_buf, CONFIG_OPT_IGNORE_NOT_FOUND );
        path_buf[0] = '\0';
      }
      FALLTHROUGH;

    case CONFIG_LOC_XDG_CONFIG_HOME:
      // 3. Try $XDG_CONFIG_HOME/include-tidy.toml and
      //    $HOME/.config/include-tidy.toml.
      if ( config_file == NULL && (home = home_dir()) != NULL ) {
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
          path_append( path_buf, SIZE_MAX, PACKAGE ".toml" );
          config_file = config_open( path_buf, CONFIG_OPT_IGNORE_NOT_FOUND );
          path_buf[0] = '\0';
        }
      }
      FALLTHROUGH;

    case CONFIG_LOC_XDG_CONFIG_DIRS:
      // 4. Try $XDG_CONFIG_DIRS/include-tidy and /etc/xdg/include-tidy.
      if ( config_file == NULL ) {
        char const *config_dirs = null_if_empty( getenv( "XDG_CONFIG_DIRS" ) );
        if ( config_dirs == NULL )
          config_dirs = "/etc/xdg";         // LCOV_EXCL_LINE
        for (;;) {
          char const *const next_sep = strchr( config_dirs, ':' );
          size_t const dir_len = next_sep != NULL ?
            STATIC_CAST( size_t, next_sep - config_dirs ) :
            strlen( config_dirs );
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
    } // switch
    config_next = false;
  }

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
 * Parses the value of the `"config_next"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param value The toml_value to parse.
 */
static void config_next_parse( char const *config_path, char const *table_name,
                               toml_value const *value ) {
  assert( config_path != NULL );
  (void)table_name;
  assert( value != NULL );

  if ( value->type != TOML_BOOL ) {
    fatal_error( EX_CONFIG,
      "%s:%u:%u "
      "invalid value for \"config_next\" key; expected boolean\n",
      config_path, value->loc.line, value->loc.col
    );
  }

  config_next = value->b;
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
        "%s:%u:%u required table name missing\n",
        config_path, toml.loc.line, toml.loc.col
      );
    }

    config_key_kind key_kinds_seen = CONFIG_KEY_NONE;

    toml_iterator iter;
    toml_iterator_init( &table, &iter );
    for ( toml_key_value const *kv;
          (kv = toml_iterator_next( &iter )) != NULL; ) {

      config_key const *const key = config_key_parse( kv->key );
      if ( key == NULL ) {
        fatal_error( EX_CONFIG,
          "%s:%u:%u: \"%s\": unknown key\n",
          config_path, kv->key_loc.line, kv->key_loc.col, table.name
        );
      }
      if ( key_kinds_seen != CONFIG_KEY_NONE ) {
        fatal_error( EX_CONFIG,
          "%s:%u:%u: \"%s\": key mutually exclusive with previous keys\n",
          config_path, kv->key_loc.line, kv->key_loc.col, table.name
        );
      }

      (*key->parse_fn)( config_path, table.name, &kv->value );
      key_kinds_seen |= key->kind;
    } // for

    if ( key_kinds_seen == CONFIG_KEY_NONE ) {
      fatal_error( EX_CONFIG,
        "%s:%u:%u: \"%s\": empty table\n",
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
 * Parses the value of an `"includes"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table_name The toml_table name.
 * @param value The toml_value to parse.
 */
static void includes_parse( char const *config_path, char const *table_name,
                            toml_value const *value ) {
  assert( config_path != NULL );
  assert( table_name != NULL );
  assert( value != NULL );

  CXFile to_include_file;

  switch ( value->type ) {
    case TOML_STRING:
      to_include_file = include_get_File( value->s );
      if ( to_include_file != NULL )
        symbol_include_add( table_name, to_include_file );
      break;
    case TOML_ARRAY:
      for ( unsigned i = 0; i < value->a.size; ++i ) {
        toml_value const *const a_value = &value->a.values[i];
        if ( a_value->type != TOML_STRING ) {
          fatal_error( EX_CONFIG,
            "%s:%u:%u: "
            "invalid value for \"includes\" key array; expected string\n",
            config_path, a_value->loc.line, a_value->loc.col
          );
        }
        to_include_file = include_get_File( a_value->s );
        if ( to_include_file != NULL )
          symbol_include_add( table_name, to_include_file );
      } // for
      break;
    default:
      fatal_error( EX_CONFIG,
        "%s:%u:%u "
        "invalid value for \"includes\" key; expected string or array\n",
        config_path, value->loc.line, value->loc.col
      );
  } // switch
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
 * Parses the value of a `"proxy"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table_name The TOML table name.
 * @param value The toml_value to parse.
 */
static void proxy_parse( char const *config_path, char const *table_name,
                         toml_value const *value ) {
  assert( config_path != NULL );
  assert( table_name != NULL );
  assert( value != NULL );

  CXFile to_include_file = include_get_File( table_name );
  if ( to_include_file == NULL )
    return;
  CXFile from_include_file;

  switch ( value->type ) {
    case TOML_STRING:
      from_include_file = include_get_File( value->s );
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
        from_include_file = include_get_File( a_value->s );
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
  FREE( si->from_symbol_name );
  rb_tree_cleanup( &si->to_include_files, /*free_fn=*/NULL );
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

  symbol_include new_si = { .from_symbol_name = from_symbol_name };
  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &symbol_include_map, &new_si, sizeof new_si );
  symbol_include *const si = RB_DINT( rv_rbi.node );
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
  for ( symbol_include const *si;
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
 * @param table_name The TOML table name.
 * @param value The toml_value to parse.
 */
static void symbols_parse( char const *config_path, char const *table_name,
                           toml_value const *value ) {
  assert( config_path != NULL );
  assert( table_name != NULL );
  assert( value != NULL );

  CXFile to_include_file = include_get_File( table_name );
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
          fatal_error( EX_CONFIG,
            "%s:%u:%u: "
            "invalid value for \"symbols\" key array; expected string\n",
            config_path, a_value->loc.line, a_value->loc.col
          );
        }
        symbol_include_add( a_value->s, to_include_file );
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

  symbol_include find_si = { .from_symbol_name = symbol_name };
  rb_node_t const *const found_rb =
    rb_tree_find( &symbol_include_map, &find_si );
  if ( found_rb == NULL )
    return NULL;
  symbol_include const *const found_si = RB_DINT( found_rb );

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

  char path_buf[ PATH_MAX ];

  for ( ; config_next && config_loc <= CONFIG_LOC_LAST; ++config_loc ) {
    FILE *const config_file = config_find( opt_config_path, path_buf );
    if ( config_file == NULL )
      break;

    RUN_ONCE {
      rb_tree_init(
        &symbol_include_map, RB_DINT,
        POINTER_CAST( rb_cmp_fn_t, &symbol_include_cmp )
      );
      ATEXIT( &config_cleanup );
    }

    config_parse( path_buf, config_file );
    fclose( config_file );
  } // for

  if ( (opt_verbose & TIDY_VERBOSE_CONFIG_SYMBOLS) != 0 )
    symbol_includes_dump();
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
