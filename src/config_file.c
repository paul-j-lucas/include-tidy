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
#include "include-tidy.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

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

NODISCARD
static FILE*            config_open( char const*, config_opts_t );

NODISCARD
static char const*      home_dir( void );

static void             path_append( char*, size_t, char const* );

NODISCARD
static char*            strip_comment( char* );

////////// local functions ////////////////////////////////////////////////////

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

  // 2. Try $XDG_CONFIG_HOME/include-tidy and $HOME/.config/include-tidy.
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
      path_append( path_buf, SIZE_MAX, PACKAGE );
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

  // parse configuration file
  char line_buf[ 8192 ];
  unsigned line_no = 0;
  while ( fgets( line_buf, sizeof line_buf, config_file ) != NULL ) {
    ++line_no;
    char *line = strip_comment( line_buf );
    if ( line == NULL ) {
      fatal_error( EX_CONFIG,
        "%s:%u: \"%s\": unclosed quote\n",
        config_path, line_no, str_trim( line_buf )
      );
    }
    line = str_trim( line );

#if 0
    switch ( line[0] ) {
      case '\0':                        // line was entirely whitespace
        continue;
      case '[':                         // parse section line
        curr_section = section_parse( line );
        if ( curr_section == CONFIG_SECTION_NONE ) {
          fatal_error( EX_CONFIG,
            "%s:%u: \"%s\": invalid section\n",
            config_path, line_no, line
          );
        }
        continue;
    } // switch
#endif
  } // while

  if ( unlikely( ferror( config_file ) ) )
    fatal_error( EX_IOERR, "%s: %s\n", config_path, STRERROR() );
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
 * Strips a `#` comment from a line minding quotes and backslashes.
 *
 * @param s The null-terminated line to strip the comment from.
 * @return Returns \a s on success or NULL for an unclosed quote.
 */
NODISCARD
static char* strip_comment( char *s ) {
  assert( s != NULL );
  char *const s0 = s;
  char quote = '\0';

  for ( ; *s != '\0'; ++s ) {
    switch ( *s ) {
      case '#':
        if ( quote != '\0' )
          break;
        *s = '\0';
        return s0;
      case '"':
      case '\'':
        if ( quote == '\0' )
          quote = *s;
        else if ( *s == quote )
          quote = '\0';
        break;
      case '\\':
        ++s;
        break;
    } // switch
  } // for

  return quote != '\0' ? NULL : s0;
}

////////// extern functions ///////////////////////////////////////////////////

/**
 * Reads an **include-tidy**(1) configuration file, if any.
 *
 * @param config_path The full path of the configuration file to read. May be
 * NULL.
 *
 * @note This function must be called at most once.
 */
void config_init( char const *config_path ) {
  ASSERT_RUN_ONCE();

  static char path_buf[ PATH_MAX ];
  FILE *const config_file = config_find( config_path, path_buf );
  if ( config_file != NULL ) {
    config_parse( path_buf, config_file );
    fclose( config_file );
  }
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
