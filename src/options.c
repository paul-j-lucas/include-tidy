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
 * Move include-tidy specific command-line options to a separate array.
 *
 * @remarks
 * @parblock
 * Command-line options of the form have:
 *
 *  + `-Xtidy` _option_ have `-Xtidy` removed and _option_ moved to \a
 *    *ptidy_argv.
 *  + `--help` or `--version` are moved to \a *ptidy_argv.
 *
 * All other options and arguments are left as-is in \a orig_argv.  Examples:
 *
 *      include-tidy --help
 *
 *          orig_argc = 1
 *          orig_argv[0] = "include-tidy"
 *          orig_argv[1] = NULL
 *
 *          tidy_argc = 2
 *          tidy_argv[0] = "include-tidy"
 *          tidy_argv[1] = "--help"
 *          tidy_argv[2] = NULL
 *
 *      include-tidy -Xtidy --foo -I/opt/local/include bar.c
 *
 *          orig_argc = 3
 *          orig_argv[0] = "include-tidy"
 *          orig_argv[1] = "-I/opt/local/include
 *          orig_argv[2] = bar.c
 *          orig_argv[3] = NULL
 *
 *          tidy_argc = 2
 *          tidy_argv[0] = "include-tidy"
 *          tidy_argv[1] = "--foo"
 *          tidy_argv[2] = NULL
 * @endparblock
 *
 * @param porig_argc A pointer to `argc`.
 * @param orig_argv A copy of `argv`.
 * @param ptidy_argc A pointer to an `argc` for include-tidy specific options.
 * @param ptidy_argv A pointer to an `argv` for include-tidy specific options.
 * The caller is responsible for free'ing the array of pointers to strings, but
 * _not_ the strings themselves.
 */
static void move_tidy_args( int *porig_argc, char const *orig_argv[],
                            int *ptidy_argc, char const **ptidy_argv[] ) {
  assert( porig_argc != NULL );
  assert(  orig_argv != NULL );
  assert( ptidy_argc != NULL );
  assert( ptidy_argv != NULL );

  int const orig_argc = *porig_argc;
  int new_argc = 1, tidy_argc = 1;

  char const **const tidy_argv =
    MALLOC( char*, STATIC_CAST( size_t, orig_argc ) + 1 );
  tidy_argv[0] = orig_argv[0];

  for ( int i = 1; i < orig_argc; ++i ) {
    if ( strcmp( orig_argv[i], "-Xtidy" ) == 0 ) {
      if ( ++i >= orig_argc )
        fatal_error( EX_USAGE, "-Xtidy requires subsequent option\n" );
      tidy_argv[ tidy_argc++ ] = orig_argv[ i ];
    }
    else if ( strcmp( orig_argv[i], "--help" ) == 0 ||
              strcmp( orig_argv[i], "--version" ) == 0 ) {
      tidy_argv[ tidy_argc++ ] = orig_argv[ i ];
    }
    else {
      orig_argv[ new_argc++ ] = orig_argv[ i ];
    }
  } // for

  orig_argv[ new_argc ] = tidy_argv[ tidy_argc ] = NULL;

  *porig_argc = new_argc;
  *ptidy_argc = tidy_argc;
  *ptidy_argv = tidy_argv;
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
    "usage: %s [-Xtidy option]... [clang-options] source-file\n"
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

void options_init( int *pargc, char const *argv[] ) {
  ASSERT_RUN_ONCE();

  int           opt;
  bool          opt_help = false;
  bool          opt_version = false;
  int           tidy_argc;
  char const  **tidy_argv;

  move_tidy_args( pargc, argv, &tidy_argc, &tidy_argv );

  opterr = 1;

  for (;;) {
    opt = getopt_long(
      tidy_argc, CONST_CAST( char**, tidy_argv ),
      ":",                              // return missing argument as ':'
      OPTIONS, /*longindex=*/NULL
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

  tidy_argc -= optind - 1;

#if 0
  printf( "argc = %d\n", *pargc );
  for ( int i = 0; i < *pargc; ++i )
    printf( "%d %s\n", i, argv[i] );
  printf( "tidy_argc = %d\n", tidy_argc  );
  for ( int i = 0; i < tidy_argc; ++i )
    printf( "%d %s\n", i, tidy_argv[i] );
  puts( "-------------------------" );
#endif

  free( tidy_argv );
  check_options();

  if ( tidy_argc > 1 )
    print_usage( EX_USAGE );
  if ( opt_help )
    print_usage( *pargc > 1 ? EX_USAGE : EX_OK );
  if ( opt_version ) {
    if ( *pargc > 1 )                   // include-tidy --version foo
      print_usage( EX_USAGE );
    print_version();
    exit( EX_OK );
  }

  if ( *pargc < 2 )
    print_usage( EX_USAGE );
  tidy_source_path = argv[ --*pargc ];
  argv[ *pargc ] = NULL;

  return;

invalid_opt:;
  // Determine whether the invalid option was short or long.
  char const *const invalid_opt = tidy_argv[ optind - 1 ];
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
