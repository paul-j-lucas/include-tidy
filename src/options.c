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
 * When to dump in UTF-8.
 */
enum utf8_when {
  UTF8_NEVER,                           ///< Never dump in UTF-8.
  UTF8_ENCODING,                        ///< Dump in UTF-8 only if encoding is.
  UTF8_ALWAYS                           ///< Always dump in UTF-8.
};
typedef enum utf8_when utf8_when_t;

/// @cond DOXYGEN_IGNORE
/// Otherwise Doxygen generates two entries.

// TODO

/// @endcond

/**
 * Command-line options.
 *
 * @sa OPTIONS_HELP
 */
static struct option const OPTIONS[] = {
  { "help",               no_argument,        NULL, COPT(HELP)                },
  { "version",            no_argument,        NULL, COPT(VERSION)             },
  { NULL,                 0,                  NULL, 0                         }
};

/**
 * Command-line options help.
 *
 * @note It is indexed by short option characters.
 *
 * @sa OPTIONS
 * @sa opt_help()
 */
static char const *const OPTIONS_HELP[] = {
  [ COPT(HELP) ] = "Print this help and exit",
  [ COPT(VERSION) ] = "Print version and exit",
};

// local variable definitions
static bool         opts_given[ 128 ];  ///< Table of options that were given.

// local functions
static void         set_all_or_none( char const**, char const* );
NODISCARD
static char const*  opt_format( char, char[const], size_t ),
                 *  opt_get_long( char );

/////////// local functions ///////////////////////////////////////////////////

/**
 * If \a opt was given, checks that _only_ it was given and, if not, prints an
 * error message and exits; if \a opt was not given, does nothing.
 *
 * @param opt The option to check for.
 *
 * @sa check_opt_mutually_exclusive()
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
 * Checks that no options were given that are among the two given mutually
 * exclusive sets of short options.
 * Prints an error message and exits if any such options are found.
 *
 * @param opts1 The first set of short options.
 * @param opts2 The second set of short options.
 *
 * @sa check_opt_exclusive()
 */
static void check_opt_mutually_exclusive( char const *opts1,
                                          char const *opts2 ) {
  assert( opts1 != NULL );
  assert( opts2 != NULL );

  unsigned gave_count = 0;
  char const *opt = opts1;
  char gave_opt1 = '\0';

  for ( unsigned i = 0; i < 2; ++i ) {
    for ( ; *opt != '\0'; ++opt ) {
      if ( opts_given[ STATIC_CAST( uint8_t, *opt ) ] ) {
        if ( ++gave_count > 1 ) {
          char const gave_opt2 = *opt;
          char opt1_buf[ OPT_BUF_SIZE ];
          char opt2_buf[ OPT_BUF_SIZE ];
          fatal_error( EX_USAGE,
            "%s and %s are mutually exclusive\n",
            opt_format( gave_opt1, opt1_buf, sizeof opt1_buf ),
            opt_format( gave_opt2, opt2_buf, sizeof opt2_buf  )
          );
        }
        gave_opt1 = *opt;
        break;
      }
    } // for
    if ( gave_count == 0 )
      break;
    opt = opts2;
  } // for
}

/**
 * For each option in \a opts that was given, checks that at least one of
 * \a req_opts was also given.
 * If not, prints an error message and exits.
 *
 * @param opts The set of short options.
 * @param req_opts The set of required options for \a opts.
 */
static void check_opt_required( char const *opts, char const *req_opts ) {
  assert( opts != NULL );
  assert( opts[0] != '\0' );
  assert( req_opts != NULL );
  assert( req_opts[0] != '\0' );

  for ( char const *opt = opts; *opt; ++opt ) {
    if ( opts_given[ STATIC_CAST( uint8_t, *opt ) ] ) {
      for ( char const *req_opt = req_opts; req_opt[0] != '\0'; ++req_opt )
        if ( opts_given[ STATIC_CAST( uint8_t, *req_opt ) ] )
          return;
      char opt_buf[ OPT_BUF_SIZE ];
      bool const reqs_multiple = req_opts[1] != '\0';
      fatal_error( EX_USAGE,
        "%s requires %sthe -%s option%s to be given also\n",
        opt_format( *opt, opt_buf, sizeof opt_buf ),
        (reqs_multiple ? "one of " : ""),
        req_opts, (reqs_multiple ? "s" : "")
      );
    }
  } // for
}

/**
 * Checks option combinations for semantic errors.
 */
static void check_options( void ) {
  // check for exclusive options
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
  size_t len = 1;                       // for leading ':'
  for ( struct option const *opt = opts; opt->name != NULL; ++opt ) {
    assert( opt->has_arg >= 0 && opt->has_arg <= 2 );
    len += 1 + STATIC_CAST( unsigned, opt->has_arg );
  } // for

  char *const short_opts = MALLOC( char, len + 1/*\0*/ );
  char *s = short_opts;

  *s++ = ':';                           // return missing argument as ':'
  for ( struct option const *opt = opts; opt->name != NULL; ++opt ) {
    assert( opt->val > 0 && opt->val < 128 );
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
  FOREACH_CLI_OPTION( opt ) {
    if ( opt->val == short_opt )
      return opt->name;
  } // for
  return "";
}

/**
 * Gets the help message for \a opt.
 *
 * @param opt The option to get the help for.
 * @return Returns said help message.
 */
NODISCARD
static char const* opt_help( int opt ) {
  unsigned const uopt = STATIC_CAST( unsigned, opt );
  assert( uopt < ARRAY_SIZE( OPTIONS_HELP ) );
  char const *const help = OPTIONS_HELP[ uopt ];
  assert( help != NULL );
  return help;
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

/**
 * Prints the usage message to standard error and exits.
 *
 * @param status The status to exit with.  If it is `EX_OK`, prints to standard
 * output; otherwise prints to standard error.
 */
_Noreturn
static void print_usage( int status ) {
  // pre-flight to calculate longest long option length
  size_t longest_opt_len = 0;
  for ( struct option const *opt = OPTIONS; opt->name != NULL; ++opt ) {
    size_t opt_len = strlen( opt->name );
    switch ( opt->has_arg ) {
      case no_argument:
        break;
      case optional_argument:
        opt_len += STRLITLEN( "[=ARG]" );
        break;
      case required_argument:
        opt_len += STRLITLEN( "=ARG" );
        break;
    } // switch
    if ( opt_len > longest_opt_len )
      longest_opt_len = opt_len;
  } // for

  FILE *const fout = status == EX_OK ? stdout : stderr;

  FPRINTF( fout,
"usage: %s [options] [infile [outfile]]\n"
"       %s --help\n"
"       %s --version\n"
"options:\n",
    prog_name, prog_name, prog_name
  );

  for ( struct option const *opt = OPTIONS; opt->name != NULL; ++opt ) {
    FPRINTF( fout, "  --%s", opt->name );
    size_t opt_len = strlen( opt->name );
    switch ( opt->has_arg ) {
      case no_argument:
        break;
      case optional_argument:
        opt_len += STATIC_CAST( size_t, fprintf( fout, "[=ARG]" ) );
        break;
      case required_argument:
        opt_len += STATIC_CAST( size_t, fprintf( fout, "=ARG" ) );
        break;
    } // switch
    assert( opt_len <= longest_opt_len );
    FPUTNSP( longest_opt_len - opt_len, fout );
    FPRINTF( fout, " (-%c) %s.\n", opt->val, opt_help( opt->val ) );
  } // for

  FPUTS(
    "\n"
    PACKAGE_NAME " home page: " PACKAGE_URL "\n"
    "Report bugs to: " PACKAGE_BUGREPORT "\n",
    fout
  );

  exit( status );
}

/**
 * Prints the **ad** version.
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

struct option const* cli_option_next( struct option const *opt ) {
  return opt == NULL ? OPTIONS : (++opt)->name == NULL ? NULL : opt;
}

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
    if ( argc > 0 )                     // ad -v foo
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
