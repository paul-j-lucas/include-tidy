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
 * Defines types, global variables, and functions for **include-tidy**(1)
 * options.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "options.h"
#include "color.h"
#include "type_traits.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>                     /* for exit() */
#include <string.h>                     /* for str...() */
#include <strings.h>                    /* for strcasecmp() */
#include <sysexits.h>

/// @endcond

/**
 * @addtogroup options-group
 * @{
 */

////////// extern variables ///////////////////////////////////////////////////

/// @cond DOXYGEN_IGNORE
/// Otherwise Doxygen generates two entries.

unsigned      opt_align_column = OPT_ALIGN_COLUMN_DEFAULT;
bool          opt_all_includes;
color_when    opt_color_when = COLOR_NOT_FILE;
char const   *opt_comment_style[2] = { "// ", "" };
tidy_com_sym  opt_comment_symbols;
bool          opt_config_layers = true;
char const   *opt_config_path;
bool          opt_debug;
tidy_error    opt_error;
unsigned      opt_line_length = OPT_LINE_LENGTH_DEFAULT;
tidy_verbose  opt_verbose;

char const   *tidy_source_path;

/// @endcond

/////////// local functions ///////////////////////////////////////////////////

/**
 * Parses a string into an `unsigned long long`.
 *
 * @remarks Unlike **strtoull**(3), insists that \a s is entirely a non-
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

bool opt_comment_symbols_parse( char const *s ) {
  struct com_sym_map {
    char const   *com_sym_str;
    tidy_com_sym  com_sym;
  };
  typedef struct com_sym_map com_sym_map;

  static com_sym_map const COM_SYM_MAP[] = {
    { "alpha",      TIDY_COM_SYM_ALPHA     },
    { "length",     TIDY_COM_SYM_LENGTH    },
    { "ref-count",  TIDY_COM_SYM_REF_COUNT },
    { "most-used",  TIDY_COM_SYM_MOST_USED },
  };

  assert( s != NULL );

  FOREACH_ARRAY_ELEMENT( com_sym_map, m, COM_SYM_MAP ) {
    if ( strcasecmp( s, m->com_sym_str ) == 0 ) {
      opt_comment_symbols = m->com_sym;
      return true;
    }
  } // for

  return false;
}

bool opt_error_parse( char const *s ) {
  struct error_map {
    char const *error_str;
    tidy_error  error;
  };
  typedef struct error_map error_map;

  static error_map const ERROR_MAP[] = {
    { "always",     TIDY_ERROR_ALWAYS         },
    { "never",      TIDY_ERROR_NEVER          },
    { "violations", TIDY_ERROR_IF_VIOLATIONS  },
  };

  assert( s != NULL );

  FOREACH_ARRAY_ELEMENT( error_map, m, ERROR_MAP ) {
    if ( strcasecmp( s, m->error_str ) == 0 ) {
      opt_error = m->error;
      return true;
    }
  } // for

  return false;
}

bool opt_line_length_parse( char const *s ) {
  assert( s != NULL );
  unsigned long long ull = parse_ull( s );
  if ( ull > OPT_LINE_LENGTH_MAX )
    return false;
  opt_line_length = STATIC_CAST( unsigned, ull );
  return true;
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
        verbose |= TIDY_VERBOSE_SRC_FILE_VIOLATIONS;
        break;
      case 'F':
        verbose |= TIDY_VERBOSE_SRC_FILE_ALWAYS;
        break;
      case 'i':
        verbose |= TIDY_VERBOSE_INCLUDES;
        break;
      case 'p':
        verbose |= TIDY_VERBOSE_PROXIES_IMPLICIT;
        break;
      case 'P':
        verbose |= TIDY_VERBOSE_PROXIES_EXPLICIT;
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

  if ( (verbose & TIDY_VERBOSE_SRC_FILE_ALWAYS) != 0 )
    verbose &= ~TO_UNSIGNED_EXPR( TIDY_VERBOSE_SRC_FILE_VIOLATIONS );

  opt_verbose = verbose;
  return true;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
