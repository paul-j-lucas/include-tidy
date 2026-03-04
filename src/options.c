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
 * Defines global variables and functions for **include-tidy** options.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "options.h"
#include "include-tidy.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <ctype.h>                      /* for islower(), toupper() */
#include <getopt.h>
#include <stddef.h>                     /* for size_t */
#include <stdio.h>                      /* for fdopen() */
#include <stdlib.h>                     /* for exit() */
#include <string.h>                     /* for str...() */
#include <sysexits.h>
#include <string.h>

// in ascending option character ASCII order; sort using: sort -bdfk3
#define OPT_HELP                h
#define OPT_INCLUDE_TIDY        X
#define OPT_VERSION             v

/// Command-line option character as a character literal.
#define COPT(X)                   CHARIFY(OPT_##X)

/// Command-line option character as a single-character string literal.
#define SOPT(X)                   STRINGIFY(OPT_##X)

/// Command-line short option as a parenthesized, dashed string literal for the
/// usage message.
#define UOPT(X)                   " (-" SOPT(X) ") "

/// @endcond

/**
 * @addtogroup options-group
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

#define OPT_BUF_SIZE        32          /**< Maximum size for an option. */

///////////////////////////////////////////////////////////////////////////////

/**
 * Command-line options.
 */
static struct option const OPTIONS[] = {
  { "help",     no_argument,  NULL, COPT(HELP)    },
  { "version",  no_argument,  NULL, COPT(VERSION) },
  { NULL,       0,            NULL, 0             }
};

// local variable definitions
static bool         opts_given[ 128 ];  ///< Table of options that were given.

// local functions
NODISCARD
static char const*  opt_format( char, char[const], size_t ),
                 *  opt_get_long( char );

/////////// local functions ///////////////////////////////////////////////////

/**
 * If \a opt was given, checks that _only_ it was given and, if not, prints an
 * error message and exits; if \a opt was not given, does nothing.
 *
 * @param opt The option to check for.
 */
static void check_opt_exclusive( char opt ) {
  if ( !opts_given[ STATIC_CAST( unsigned, opt ) ] )
    return;
  for ( size_t i = '0'; i < ARRAY_SIZE( opts_given ); ++i ) {
    char const curr_opt = STATIC_CAST( char, i );
    if ( curr_opt == opt )
      continue;
    if ( opts_given[ STATIC_CAST( unsigned, curr_opt ) ] ) {
      char opt_buf[ OPT_BUF_SIZE ];
      fatal_error( EX_USAGE,
        "%s can be given only by itself\n",
        opt_format( opt, opt_buf, sizeof opt_buf )
      );
    }
  } // for
}

/**
 * Checks option combinations for semantic errors.
 */
static void check_options( void ) {
  check_opt_exclusive( COPT(HELP) );
  check_opt_exclusive( COPT(VERSION) );
}

/**
 * Makes the `optstring` (short option) equivalent of \a opts for the third
 * argument of `getopt_long()`.
 *
 * @param opts An array of options to make the short option string from.  Its
 * last element must be all zeros.
 * @return Returns the `optstring` for the third argument of `getopt_long()`.
 * The caller is responsible for freeing it.
 */
NODISCARD
static char const* make_short_opts( struct option const opts[static const 2] ) {
  // pre-flight to calculate string length
  size_t len = 1 /* for leading ':' */ + STRLITLEN( "X:" );
  for ( struct option const *opt = opts; opt->name != NULL; ++opt ) {
    assert( opt->has_arg >= 0 && opt->has_arg <= 2 );
    len += 1 + STATIC_CAST( unsigned, opt->has_arg );
  } // for

  char *const short_opts = MALLOC( char, len + 1/*\0*/ );
  char *s = short_opts;

  *s++ = ':';                           // return missing argument as ':'
  *s++ = 'X';
  *s++ = ':';

  for ( struct option const *opt = opts; opt->name != NULL; ++opt ) {
    assert( opt->val > 0 && opt->val < 128 );
    assert( opt->val != 'X' );
    *s++ = STATIC_CAST( char, opt->val );
    switch ( opt->has_arg ) {
      case optional_argument:
        *s++ = ':';
        FALLTHROUGH;
      case required_argument:
        *s++ = ':';
    } // switch
  } // for
  *s = '\0';

  return short_opts;
}

/**
 * Formats an option as <code>[--%s/]-%c</code> where \c %s is the long option
 * (if any) and %c is the short option.
 *
 * @param short_opt The short option (along with its corresponding long option,
 * if any) to format.
 * @param buf The buffer to use.
 * @param size The size of \a buf.
 * @return Returns \a buf.
 */
NODISCARD
static char const* opt_format( char short_opt, char buf[const], size_t size ) {
  char const *const long_opt = opt_get_long( short_opt );
  snprintf(
    buf, size, "%s%s%s-%c",
    *long_opt ? "--" : "", long_opt, *long_opt ? "/" : "", short_opt
  );
  return buf;
}

/**
 * Gets the corresponding name of the long option for the given short option.
 *
 * @param short_opt The short option to get the corresponding long option for.
 * @return Returns the said name or the empty string if none.
 */
NODISCARD
static char const* opt_get_long( char short_opt ) {
  for ( struct option const *opt = OPTIONS; opt->name != NULL; ++opt ) {
    if ( opt->val == short_opt )
      return opt->name;
  } // for
  return "";
}

/**
 * Prints the usage message to standard error and exits.
 *
 * @param status The status to exit with.  If it is `EX_OK`, prints to standard
 * output; otherwise prints to standard error.
 */
_Noreturn
static void print_usage( int status ) {
  FILE *const fout = status == EX_OK ? stdout : stderr;

  FPRINTF( fout,
    "usage: %s [-Xtidy option] source-file\n"
    "       %s --help\n"
    "       %s --version\n"
    "\n"
    PACKAGE_NAME " home page: " PACKAGE_URL "\n"
    "Report bugs to: " PACKAGE_BUGREPORT "\n",
    prog_name, prog_name, prog_name
  );

  exit( status );
}

/**
 * Prints the **include-tidy** version.
 */
static void print_version( void ) {
  PUTS(
    PACKAGE_STRING "\n"
    "Copyright (C) " INCLUDE_TIDY_COPYRIGHT_YEAR " " INCLUDE_TIDY_AUTHOR "\n"
    "License " INCLUDE_TIDY_LICENSE " <" INCLUDE_TIDY_LICENSE_URL ">.\n"
    "This is free software: you are free to change and redistribute it.\n"
    "There is NO WARRANTY to the extent permitted by law.\n"
  );
}

////////// extern functions ///////////////////////////////////////////////////

void options_init( int argc, char const *const argv[] ) {
  ASSERT_RUN_ONCE();

  int               opt;
  bool              opt_help = false;
  bool              opt_version = false;
  char const *const short_opts = make_short_opts( OPTIONS );

  opterr = 1;

  for (;;) {
    opt = getopt_long(
      argc, CONST_CAST( char**, argv ), short_opts, OPTIONS,
      /*longindex=*/NULL
    );
    if ( opt == -1 )
      break;
    switch ( opt ) {
      case COPT(HELP):
        opt_help = true;
        break;
      case COPT(INCLUDE_TIDY):
        // TODO
        break;
      case COPT(VERSION):
        opt_version = true;
        break;

      case ':':
        goto missing_arg;
      case '?':
        goto invalid_opt;

      default:
        if ( isprint( opt ) )
          INTERNAL_ERROR(
            "'%c': unaccounted-for getopt_long() return value\n", opt
          );
        INTERNAL_ERROR(
          "%d: unaccounted-for getopt_long() return value\n", opt
        );
    } // switch
    opts_given[ opt ] = true;
  } // for

  FREE( short_opts );

  argc -= optind;
  argv += optind - 1;

  check_options();

  if ( opt_help )
    print_usage( argc > 0 ? EX_USAGE : EX_OK );

  if ( opt_version ) {
    if ( argc > 0 )                     // include-tidy -v foo
      print_usage( EX_USAGE );
    print_version();
    exit( EX_OK );
  }

  switch ( argc ) {
    case 1:                             // infile only
      tidy_source_path = argv[1];
      break;
    default:
      print_usage( EX_USAGE );
  } // switch

  return;

invalid_opt:;
  // Determine whether the invalid option was short or long.
  char const *const invalid_opt = argv[ optind - 1 ];
  EPRINTF( "%s: ", prog_name );
  if ( invalid_opt != NULL && strncmp( invalid_opt, "--", 2 ) == 0 )
    EPRINTF( "\"%s\"", invalid_opt + 2/*skip over "--"*/ );
  else
    EPRINTF( "'%c'", STATIC_CAST( char, optopt ) );
  EPRINTF( ": invalid option; use --help or -h for help\n" );
  exit( EX_USAGE );

missing_arg:;
  char opt_buf[ OPT_BUF_SIZE ];
  fatal_error( EX_USAGE,
    "\"%s\" requires an argument\n",
    opt_format(
      STATIC_CAST( char, opt == ':' ? optopt : opt ),
      opt_buf, sizeof opt_buf
    )
  );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
