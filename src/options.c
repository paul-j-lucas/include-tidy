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
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <errno.h>
#include <limits.h>                     /* for PATH_MAX */
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */
#include <stdio.h>
#include <stdlib.h>                     /* for exit() */
#include <string.h>                     /* for str...() */
#include <sysexits.h>

/// @endcond

/**
 * @addtogroup options-group
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

#define OPT_VERBOSE_ALL           "aiPsS"

///////////////////////////////////////////////////////////////////////////////

// extern option variables
unsigned            opt_align_column = OPT_ALIGN_COLUMN_DEFAULT;
bool                opt_all_includes;
char const         *opt_comment_style[2] = { "// ", "" };
char const         *opt_config_path;
unsigned            opt_line_length = OPT_LINE_LENGTH_DEFAULT;
tidy_verbose        opt_verbose;

// extern argument variables
char const         *arg_source_path;

// local option variables
static char       **opt_include_paths;  ///< Null-terminated list of `-I` paths.

// local functions
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
 * negative number.
 *
 * @param s The NULL-terminated string to parse.
 * @return Returns the parsed number only if \a s is entirely a non-negative
 * number or prints an error message and exits if there was an error.
 */
NODISCARD
static unsigned long long parse_ull( char const *s ) {
  assert( s != NULL );
  SKIP_WS( s );
  if ( likely( s[0] != '\0' || s[0] != '-' ) ) {
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

void options_init( void ) {
  ASSERT_RUN_ONCE();
  ATEXIT( &options_cleanup );
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
      REALLOC( opt_include_paths, char*, opt_include_paths_cap + 1/*NULL*/ );
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

bool parse_comment_style( char const *delim_format ) {
  assert( delim_format != NULL );

  if ( strcmp( delim_format, "none" ) == 0 ) {
    opt_comment_style[0] = "";
    opt_comment_style[1] = "";
  }
  else if ( strcmp( delim_format, "//" ) == 0 ) {
    opt_comment_style[0] = "// ";
    opt_comment_style[1] = "";
  }
  else if ( strcmp( delim_format, "/*" ) == 0 ) {
    opt_comment_style[0] = "/* ";
    opt_comment_style[1] = " */";
  }
  else {
    return false;
  }

  return true;
}

bool parse_comment_alignment( char const *s ) {
  assert( s != NULL );
  unsigned long long ull = parse_ull( s );
  if ( ull > OPT_COMMENT_ALIGN_MAX )
    return false;
  opt_align_column = STATIC_CAST( unsigned, ull );
  return true;
}

bool parse_line_length( char const *s ) {
  assert( s != NULL );
  unsigned long long ull = parse_ull( s );
  if ( ull > OPT_LINE_LENGTH_MAX )
    return false;
  opt_line_length = STATIC_CAST( unsigned, ull );
  return true;
}

bool parse_tidy_verbose( char const *verbose_format ) {
  assert( verbose_format != NULL );

  set_all_or_none( &verbose_format, OPT_VERBOSE_ALL );
  tidy_verbose verbose = TIDY_VERBOSE_NONE;

    for ( char const *s = verbose_format; *s != '\0'; ++s ) {
    switch ( *s ) {
      case 'a':
        verbose |= TIDY_VERBOSE_ARGS;
        break;
      case 'P':
        verbose |= TIDY_VERBOSE_CONFIG_PROXIES;
        break;
      case 'i':
        verbose |= TIDY_VERBOSE_INCLUDES;
        break;
      case 'S':
        verbose |= TIDY_VERBOSE_CONFIG_SYMBOLS;
        break;
      case 's':
        verbose |= TIDY_VERBOSE_SYMBOLS;
        break;
      default:
        return false;
    } // switch
  } // for

  opt_verbose = verbose;
  return true;
}

int verbose_printf( char const *format, ... ) {
  if ( opt_verbose == TIDY_VERBOSE_NONE )
    return 0;
  fputs( "// tidy | ", stdout );
  va_list args;
  va_start( args, format );
  int const raw_len = vprintf( format, args );
  va_end( args );
  return raw_len;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
