/*
**      include-tidy -- #include tidier
**      src/options.c
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
**      along with this program.  If not, see <http
*/

/**
 * @file
 * Defines types, global variables, and functions for **include-tidy** options.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "options.h"
#include "color.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>                     /* for PATH_MAX */
#include <stdbool.h>
#include <stdlib.h>                     /* for exit() */
#include <string.h>                     /* for str...() */
#include <sysexits.h>

/// @endcond

/**
 * @addtogroup options-group
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

#define OPT_VERBOSE_ALL           "acCdfipPsS"

////////// extern variables ///////////////////////////////////////////////////

/// @cond DOXYGEN_IGNORE
/// Otherwise Doxygen generates two entries.

unsigned      opt_align_column = OPT_ALIGN_COLUMN_DEFAULT;
bool          opt_all_includes;
color_when    opt_color_when = COLOR_NOT_FILE;
char const   *opt_comment_style[2] = { "// ", "" };
bool          opt_config_layers = true;
char const   *opt_config_path;
tidy_error    opt_error;
unsigned      opt_line_length = OPT_LINE_LENGTH_DEFAULT;
tidy_verbose  opt_verbose;

char const   *arg_source_path;

/// @endcond

////////// local variables ////////////////////////////////////////////////////

static char **opt_include_paths;        ///< Null-terminated list of `-I` paths.
static bool   opt_is_set_impl[ 256 ];   ///< Was an option set?

////////// local functions ////////////////////////////////////////////////////

NODISCARD
static unsigned long long parse_ull( char const* );

static void               set_all_or_none( char const**, char const* );

/////////// local functions ///////////////////////////////////////////////////

/**
 * Cleans-up options.
 *
 * @sa options_init()
 */
static void options_cleanup( void ) {
  if ( opt_include_paths != NULL ) {
    for ( char **ppath = opt_include_paths; *ppath != NULL; ++ppath )
      free( *ppath );
    free( opt_include_paths );
  }
}

/**
 * Parses a string into an <code>unsigned long long</code>.
 *
 * @remarks Unlike **strtoull(3)**, insists that \a s is entirely a non-
 * negative integer.
 *
 * @param s The NULL-terminated string to parse.
 * @return Returns the parsed integer only if \a s is entirely a non-negative
 * integer or prints an error message and exits if there was an error.
 */
NODISCARD
static unsigned long long parse_ull( char const *s ) {
  assert( s != NULL );
  SKIP_WS( s );
  if ( likely( s[0] != '\0' && s[0] != '-' ) ) {
    char *end = NULL;
    errno = 0;
    unsigned long long const n = strtoull( s, &end, 0 );
    if ( likely( errno == 0 && *end == '\0' ) )
      return n;
  }
  fatal_error( EX_USAGE, "\"%s\": invalid integer\n", s );
}

/**
 * If \a *pformat is:
 *
 *  + `"*"`: sets \a *pformat to \a all_value.
 *  + `"-"`: sets \a *pformat to `""` (the empty string).
 *
 * Otherwise does nothing.
 *
 * @param pformat A pointer to the format string to possibly set.
 * @param all_value The "all" value for when \a *pformat is `"*"`.
 */
static void set_all_or_none( char const **pformat, char const *all_value ) {
  assert( pformat != NULL );
  assert( *pformat != NULL );
  assert( all_value != NULL );
  assert( all_value[0] != '\0' );

  if ( strcmp( *pformat, "*" ) == 0 )
    *pformat = all_value;
  else if ( strcmp( *pformat, "-" ) == 0 )
    *pformat = "";
}

////////// extern functions ///////////////////////////////////////////////////

bool opt_align_column_parse( char const *s ) {
  assert( s != NULL );
  unsigned long long ull = parse_ull( s );
  if ( ull > OPT_ALIGN_COLUMN_MAX )
    return false;
  opt_align_column = STATIC_CAST( unsigned, ull );
  return true;
}

bool opt_color_parse( char const *s ) {
  struct color_when_map {
    char const *when_str;
    color_when  when;
  };
  typedef struct color_when_map color_when_map;

  static color_when_map const COLOR_WHEN_MAP[] = {
    { "always",    COLOR_ALWAYS   },
    { "auto",      COLOR_ISATTY   },    // grep compatibility
    { "isatty",    COLOR_ISATTY   },    // explicit synonym for auto
    { "never",     COLOR_NEVER    },
    { "not_file",  COLOR_NOT_FILE },    // !ISREG( stdout )
    { "not_isreg", COLOR_NOT_FILE },    // synonym for not_isfile
    { "tty",       COLOR_ISATTY   },    // synonym for isatty
  };

  assert( s != NULL );

  FOREACH_ARRAY_ELEMENT( color_when_map, m, COLOR_WHEN_MAP ) {
    if ( strcasecmp( s, m->when_str ) == 0 ) {
      opt_color_when = m->when;
      return true;
    }
  } // for

  return false;
}

bool opt_comment_style_parse( char const *s ) {
  assert( s != NULL );

  if ( strcmp( s, "none" ) == 0 ) {
    opt_comment_style[0] = "";
    opt_comment_style[1] = "";
  }
  else if ( strcmp( s, "//" ) == 0 ) {
    opt_comment_style[0] = "// ";
    opt_comment_style[1] = "";
  }
  else if ( strcmp( s, "/*" ) == 0 ) {
    opt_comment_style[0] = "/* ";
    opt_comment_style[1] = " */";
  }
  else {
    return false;
  }

  return true;
}

bool opt_error_parse( char const *s ) {
  assert( s != NULL );

  if ( strcmp( s, "always" ) == 0 )
    opt_error = TIDY_ERROR_ALWAYS;
  else if ( strcmp( s, "never" ) == 0 )
    opt_error = TIDY_ERROR_NEVER;
  else if ( strcmp( s, "violations" ) == 0 )
    opt_error = TIDY_ERROR_IF_VIOLATIONS;
  else
    return false;

  return true;
}

void opt_include_paths_add( char const *include_path ) {
  assert( include_path != NULL );

  char real_path[ PATH_MAX ];
  if ( realpath( include_path, real_path ) != NULL )
    include_path = real_path;

  static size_t opt_include_paths_cap = 1;
  size_t i = 0;

  if ( opt_include_paths == NULL ) {
    opt_include_paths = MALLOC( char*, opt_include_paths_cap + 1/*NULL*/ );
  }
  else {
    for ( ; opt_include_paths[i] != NULL; ++i ) {
      if ( strcmp( include_path, opt_include_paths[i] ) == 0 )
        return;
    } // for
    if ( i >= opt_include_paths_cap ) {
      opt_include_paths_cap *= 2;
      REALLOC( opt_include_paths, opt_include_paths_cap + 1/*NULL*/ );
    }
  }

  opt_include_paths[  i] = check_strdup( include_path );
  opt_include_paths[++i] = NULL;
}

char const* opt_include_paths_relativize( char const *abs_path ) {
  assert( abs_path != NULL );
  assert( opt_include_paths != NULL );

  size_t      longest_include_path_len = 0;
  char const *shortest_include_path = abs_path;

  for ( char **ppath = opt_include_paths; *ppath != NULL; ++ppath ) {
    char const *const include_path_i      = *ppath;
    size_t const      include_path_i_len  = strlen( include_path_i );

    if ( include_path_i_len > longest_include_path_len &&
         strncmp( abs_path, include_path_i, include_path_i_len ) == 0 ) {
      longest_include_path_len = include_path_i_len;
      shortest_include_path = abs_path + include_path_i_len;

      if ( shortest_include_path[0] == '/' )
        ++shortest_include_path;
    }
  } // for

  return path_no_dot_slash( shortest_include_path );
}

bool opt_is_set( int short_opt ) {
  assert( short_opt >= 0 && short_opt <= 255 );
  return opt_is_set_impl[ STATIC_CAST( unsigned char, short_opt ) ];
}

bool opt_line_length_parse( char const *s ) {
  assert( s != NULL );
  unsigned long long ull = parse_ull( s );
  if ( ull > OPT_LINE_LENGTH_MAX )
    return false;
  opt_line_length = STATIC_CAST( unsigned, ull );
  return true;
}

void opt_mark_set( int short_opt ) {
  assert( short_opt >= 0 && short_opt <= 255 );
  assert( isalnum( short_opt ) );
  opt_is_set_impl[ STATIC_CAST( unsigned char, short_opt ) ] = true;
}

bool opt_verbose_parse( char const *verbose_format ) {
  assert( verbose_format != NULL );

  set_all_or_none( &verbose_format, OPT_VERBOSE_ALL );
  tidy_verbose verbose = TIDY_VERBOSE_NONE;

    for ( char const *s = verbose_format; *s != '\0'; ++s ) {
    switch ( *s ) {
      case 'a':
        verbose |= TIDY_VERBOSE_ARGS;
        break;
      case 'c':
        verbose |= TIDY_VERBOSE_CONFIG_FILES;
        break;
      case 'C':
        verbose |= TIDY_VERBOSE_CURSORS;
        break;
      case 'd':
        verbose |= TIDY_VERBOSE_DIRECTORY;
        break;
      case 'f':
        verbose |= TIDY_VERBOSE_SOURCE_FILE;
        break;
      case 'i':
        verbose |= TIDY_VERBOSE_INCLUDES;
        break;
      case 'p':
        verbose |= TIDY_VERBOSE_PROXIES_EXPLICIT;
        break;
      case 'P':
        verbose |= TIDY_VERBOSE_PROXIES_IMPLICIT;
        break;
      case 's':
        verbose |= TIDY_VERBOSE_SYMBOLS;
        break;
      case 'S':
        verbose |= TIDY_VERBOSE_CONFIG_SYMBOLS;
        break;
      default:
        return false;
    } // switch
  } // for

  opt_verbose = verbose;
  return true;
}

void options_init( void ) {
  ASSERT_RUN_ONCE();
  ATEXIT( &options_cleanup );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
