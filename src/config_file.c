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
 * @defgroup config-file-group Configuration File
 * Function to read **include-tidy**(1) a configuration file.
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
 * Mapping from an original include file to another that's a proxy for it.
 */
struct include_proxy {
  union {
    char const     *from_rel_path;      ///< From include relative path.
    CXFileUniqueID  from_include_id;    ///< Corresponding include ID.
  };
  CXFile            to_include_file;    ///< Proxy include file.
};
typedef struct include_proxy include_proxy;

/**
 * Mapping from a symbol name to the include file's relative path _or_ file
 * that declares it from a configuration file.
 */
struct symbol_include {
  char const         *symbol_name;      ///< Symbol name.
  union {
    char const       *rel_path;         ///< Include relative path.
    struct {
      CXFile          include_file;     ///< Corresponding include file.
      CXFileUniqueID  include_id;       ///< Corresponding include ID.
    };
  };
};
typedef struct symbol_include symbol_include;

// local functions
NODISCARD
static FILE*        config_open( char const*, config_opts_t );

NODISCARD
static char const*  home_dir( void );

static void         include_proxy_cleanup( include_proxy* );
static void         path_append( char*, size_t, char const* );

NODISCARD
static bool         proxy_parse( char const*, toml_table* ),
                    symbols_parse( char const*, toml_table* );

static void         symbol_include_add( char const*, char const* );
static void         symbol_include_cleanup( symbol_include* );

// local variables
static CXTranslationUnit config_tu;     ///< Current translation unit.
static rb_tree_t include_proxy_map_by_rel_path;      ///< TODO.
static rb_tree_t include_proxy_map_by_id;///< TODO.
static rb_tree_t symbol_include_map;     ///< Mapping from symbols to includes.

////////// local functions ////////////////////////////////////////////////////

/**
 * Cleans-up all configuration data.
 */
static void config_cleanup( void ) {
  rb_tree_cleanup( &include_proxy_map_by_id, /*free_fn=*/NULL );
  rb_tree_cleanup(
    &include_proxy_map_by_rel_path,
    POINTER_CAST( rb_free_fn_t, include_proxy_cleanup )
  );
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

    if ( proxy_parse( config_path, &table ) )
      continue;
    if ( symbols_parse( config_path, &table ) )
      continue;

    fatal_error( EX_CONFIG,
      "%s:%u:%u required \"proxy\" or \"symbols\" key for \"%s\" missing\n",
      config_path, table.loc.line, table.loc.col, table.name
    );
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
 * Maps an include file to another that will be a proxy for the original.
 *
 * @param from_rel_path The original include's relative path.
 * @param to_include_file The proxy include file.
 */
static void include_proxy_add( char const *from_rel_path,
                               CXFile to_include_file ) {
  assert( from_rel_path != NULL );

  include_proxy new_ip = {
    .from_rel_path = from_rel_path,
    .to_include_file = to_include_file
  };
  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &include_proxy_map_by_rel_path, &new_ip, sizeof new_ip );
  if ( rv_rbi.inserted ) {
    include_proxy *const ip = RB_DINT( rv_rbi.node );
    ip->from_rel_path = check_strdup( from_rel_path );
  }
}

/**
 * Dumps all include proxies.
 */
static void include_proxies_dump( void ) {
  if ( rb_tree_empty( &include_proxy_map_by_rel_path ) )
    return;
  verbose_printf( "configuration proxies:\n" );
  rb_iterator_t iter;
  rb_iterator_init( &include_proxy_map_by_rel_path, &iter );
  for ( include_proxy const *ip; (ip = rb_iterator_next( &iter )) != NULL; ) {
    CXString to_rel_path_cxs = clang_getFileName( ip->to_include_file );
    char const *const to_rel_path = clang_getCString( to_rel_path_cxs );
    verbose_printf( "  \"%s\" -> \"%s\"\n", ip->from_rel_path, to_rel_path );
    clang_disposeString( to_rel_path_cxs );
  } // for
  verbose_printf( "\n" );
}

/**
 * Cleans-up a include_proxy.
 *
 * @param ip The include_proxy to clean up.  If NULL, does nothing.
 */
static void include_proxy_cleanup( include_proxy *ip ) {
  if ( ip == NULL )
    return;
  FREE( ip->from_rel_path );
}

/**
 * Compares two \ref include_proxy objects by ID.
 *
 * @param i_ip The first include_proxy.
 * @param j_ip The second include_proxy.
 * @return Returns a number less than 0, 0, or greater than 0 if the name of \a
 * i_ip is less than, equal to, or greater than the name of \a j_ip,
 * respectively.
 */
NODISCARD
static int include_proxy_cmp_by_id( include_proxy const *i_ip,
                                    include_proxy const *j_ip ) {
  assert( i_ip != NULL );
  assert( j_ip != NULL );
  return tidy_CXFileUniqueID_cmp(
    &i_ip->from_include_id,
    &j_ip->from_include_id
  );
}

/**
 * Compares two \ref include_proxy objects by relative path.
 *
 * @param i_ip The first include_proxy.
 * @param j_ip The second include_proxy.
 * @return Returns a number less than 0, 0, or greater than 0 if the name of \a
 * i_ip is less than, equal to, or greater than the name of \a j_ip,
 * respectively.
 */
NODISCARD
static int include_proxy_cmp_by_rel_path( include_proxy const *i_ip,
                                          include_proxy const *j_ip ) {
  assert( i_ip != NULL );
  assert( j_ip != NULL );
  return strcmp( i_ip->from_rel_path, j_ip->from_rel_path );
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
 * @return Returns `true` only if a `"proxy"` key exists and was parsed
 * successfully.
 */
static bool proxy_parse( char const *config_path, toml_table *table ) {
  assert( config_path != NULL );
  assert( table != NULL );

  toml_value const *const value = toml_table_find( table, "proxy" );
  if ( value == NULL )
    return false;

  CXFile to_include_file = clang_getFile( config_tu, table->name );
  if ( to_include_file == NULL )
    return true;

  switch ( value->type ) {
    case TOML_STRING:
      include_proxy_add( value->s, to_include_file );
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
        include_proxy_add( a_value->s, to_include_file );
      } // for
      break;
    default:
      fatal_error( EX_CONFIG,
        "%s:%u:%u "
        "invalid value for \"proxy\" key; expected string or array\n",
        config_path, value->loc.line, value->loc.col
      );
  } // switch

  return true;
}

/**
 * Resolves each include_proxy's \ref include_proxy::from_rel_path
 * "from_rel_path" to its corresponding \ref include_proxy::from_include_id
 * "from_include_id".
 */
static void resolve_include_proxys( void ) {
  rb_iterator_t iter;
  rb_iterator_init( &include_proxy_map_by_rel_path, &iter );
  for ( include_proxy *ip; (ip = rb_iterator_next( &iter )) != NULL; ) {
    CXFile include_file = include_getFile( ip->from_rel_path );
    if ( include_file != NULL ) {
      include_proxy new_ip = {
        .from_include_id = tidy_getFileUniqueID( include_file ),
        .to_include_file = ip->to_include_file
      };
      PJL_DISCARD_RV(
        rb_tree_insert( &include_proxy_map_by_id, &new_ip, sizeof new_ip )
      );
    }
  } // for
}

/**
 * Resolves each symbol_include's \ref symbol_include::rel_path "rel_path" to
 * its corresponding \ref symbol_include::include_file "include_file".
 */
static void resolve_symbol_includes( void ) {
  rb_iterator_t iter;
  rb_iterator_init( &symbol_include_map, &iter );
  for ( symbol_include *si; (si = rb_iterator_next( &iter )) != NULL; ) {
    CXFile include_file = include_getFile( si->rel_path );
    FREE( si->rel_path );
    si->include_file = include_file;

    if ( include_file == NULL )
      si->include_id = (CXFileUniqueID){ 0 };
    else
      si->include_id = tidy_getFileUniqueID( include_file );
  } // for
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
 * Maps \a symbol_name to \a rel_path so that if \a symbol_name is referenced,
 * it'll require \a rel_path.
 *
 * @param symbol_name The name of the symbol.
 * @param rel_path The include file's relative path.
 */
static void symbol_include_add( char const *symbol_name,
                                char const *rel_path ) {
  assert( symbol_name != NULL );
  assert( rel_path != NULL );

  symbol_include new_si = { .symbol_name = symbol_name };
  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &symbol_include_map, &new_si, sizeof new_si );
  if ( rv_rbi.inserted ) {
    symbol_include *const si = RB_DINT( rv_rbi.node );
    *si = (symbol_include){
      .symbol_name = check_strdup( symbol_name ),
      .rel_path = check_strdup( rel_path )
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
    verbose_printf( "  \"%s\" -> \"%s\"\n", si->symbol_name, si->rel_path );
  verbose_printf( "\n" );
}

/**
 * If present, parses the value of a `"symbols"` key.
 *
 * @param config_path The full path to the configurarion file.
 * @param table The toml_table to parse.
 * @return Returns `true` only if a `"symbols"` key exists and was parsed
 * successfully.
 */
static bool symbols_parse( char const *config_path, toml_table *table ) {
  assert( config_path != NULL );
  assert( table != NULL );

  toml_value const *const value = toml_table_find( table, "symbols" );
  if ( value == NULL )
    return false;

  switch ( value->type ) {
    case TOML_STRING:
      symbol_include_add( value->s, table->name );
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
        symbol_include_add( a_value->s, table->name );
      } // for
      break;
    default:
      fatal_error( EX_CONFIG,
        "%s:%u:%u "
        "invalid value for \"symbols\" key; expected string or array\n",
        config_path, value->loc.line, value->loc.col
      );
  } // switch

  return true;
}

////////// extern functions ///////////////////////////////////////////////////

CXFile config_get_include_proxy( CXFile include_file ) {
  include_proxy find_ip = {
    .from_include_id = tidy_getFileUniqueID( include_file )
  };
  rb_node_t const *const found_rb =
    rb_tree_find( &include_proxy_map_by_id, &find_ip );
  if ( found_rb == NULL )
    return include_file;
  include_proxy const *const found_ip = RB_DINT( found_rb );
  return found_ip->to_include_file;
}

CXFile config_get_symbol_include( char const *symbol_name ) {
  assert( symbol_name != NULL );

  symbol_include find_si = { .symbol_name = symbol_name };
  rb_node_t const *const found_rb =
    rb_tree_find( &symbol_include_map, &find_si );
  if ( found_rb == NULL )
    return NULL;
  symbol_include const *const found_si = RB_DINT( found_rb );
  return found_si->include_file;
}

void config_init( CXTranslationUnit tu ) {
  ASSERT_RUN_ONCE();
  config_tu = tu;

  rb_tree_init(
    &include_proxy_map_by_id, RB_DINT,
    POINTER_CAST( rb_cmp_fn_t, &include_proxy_cmp_by_id )
  );
  rb_tree_init(
    &include_proxy_map_by_rel_path, RB_DINT,
    POINTER_CAST( rb_cmp_fn_t, &include_proxy_cmp_by_rel_path )
  );
  rb_tree_init(
    &symbol_include_map, RB_DINT, POINTER_CAST( rb_cmp_fn_t, &symbol_include_cmp )
  );
  ATEXIT( &config_cleanup );

  char path_buf[ PATH_MAX ];
  FILE *const config_file = config_find( opt_config_path, path_buf );
  if ( config_file == NULL )
    return;
  config_parse( path_buf, config_file );
  fclose( config_file );
  if ( (opt_verbose & TIDY_VERBOSE_CONFIG_PROXIES) != 0 )
    include_proxies_dump();
  if ( (opt_verbose & TIDY_VERBOSE_CONFIG_SYMBOLS) != 0 )
    symbol_includes_dump();
}

void config_resolve_includes( void ) {
  resolve_include_proxys();
  resolve_symbol_includes();
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
