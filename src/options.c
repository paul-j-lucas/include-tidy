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
#include "red_black.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <ctype.h>                      /* for islower(), toupper() */
#include <getopt.h>
#include <limits.h>                     /* for PATH_MAX */
#include <stddef.h>                     /* for size_t */
#include <stdio.h>                      /* for fdopen() */
#include <stdlib.h>                     /* for exit() */
#include <string.h>                     /* for str...() */
#include <sysexits.h>
#include <string.h>

// in ascending option character ASCII order; sort using: sort -bdfk3
#define OPT_CLANG                 c
#define OPT_HELP                  h
#define OPT_INCLUDE               I
#define OPT_VERBOSE               V
#define OPT_VERSION               v
#define OPT_LANGUAGE              x

/// Command-line option character as a character literal.
#define COPT(X)                   CHARIFY(OPT_##X)

/// Command-line option as a string literal.
#define SOPT(X)                   STRINGIFY(OPT_##X)

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
  { "clang",    required_argument,  NULL, COPT(CLANG)   },
  { "help",     no_argument,        NULL, COPT(HELP)    },
  { "verbose",  no_argument,        NULL, COPT(VERBOSE) },
  { "version",  no_argument,        NULL, COPT(VERSION) },
  { NULL,       0,                  NULL, 0             }
};

// option variables
static char const  *opt_clang_path = "clang";
static char const  *opt_language;
static unsigned     opt_verbose;

// local variable definitions
static char       **include_path_list;  ///< List of `-I` paths.
static size_t       include_path_cap;
static bool         opts_given[ 128 ];  ///< Table of options that were given.

// local functions
static void         include_add_path( char const* );

NODISCARD
static char const*  opt_format( char, char[const], size_t ),
                 *  opt_get_long( char );

/////////// local functions ///////////////////////////////////////////////////

/**
 * Call **clang**(1) and parse its verbose output to get the list of include
 * search paths.
 *
 * @param pargc A pointer to the argument count from \c main().
 * @param pargv A pointer to the argument values from \c main().
 */
static void add_clang_include_paths( int *pargc, char const **pargv[] ) {
  ASSERT_RUN_ONCE();

  static char const CLANG_TEMPLATE[] =
    "%s"                                // clang path
    " -E"                               // run only the preprocessor stage
    " -x%s"                             // specify langauge: c or c++
    " -v"                               // show verbose output
    " -"                                // read from stdin
    " </dev/null"                       // read from /dev/null
    " 2>&1";                            // redirect stderr to stdout

  char clang_buf[ PATH_MAX + 32 ];
  snprintf( clang_buf, sizeof clang_buf, CLANG_TEMPLATE, opt_clang_path, "c" );

  FILE *const clang = popen( clang_buf, "r" );
  if ( clang == NULL )
    goto error;

  bool    found_include_search = false;
  char   *line_buf = NULL;
  size_t  line_cap = 0;

  while ( getline( &line_buf, &line_cap, clang ) != -1 ) {
    if ( STRNCMPLIT( line_buf, "#include <...> search starts here:" ) == 0 ) {
      found_include_search = true;
      break;
    }
  } // while

  if ( found_include_search ) {
    int argi = *pargc;                  // index where to insert new -I option
    for ( int i = *pargc - 1; i > 0; --i ) {
      if ( (*pargv)[i][0] != '-' ) {
        argi = i;
        break;
      }
    } // for

    while ( getline( &line_buf, &line_cap, clang ) != -1 ) {
      if ( STRNCMPLIT( line_buf, "End of search list." ) == 0 )
        break;

#ifdef __APPLE__
      // On macOS, clang's include search paths include frameworks directories
      // denoted by having paths followed by " (framework directory)".  These
      // don't contain .h file directly, so there's no point in including them.
      if ( strstr( line_buf, "(framework directory)" ) != NULL )
        continue;
#endif /* __APPLE__ */

      char const *const abs_include_path = str_trim( line_buf );
      if ( abs_include_path[0] != '/' )
        continue;

      size_t const old_argc = STATIC_CAST( size_t, *pargc );
      size_t const new_size = (old_argc + 2) * sizeof(char*);

      static bool is_argv_on_heap = false;
      if ( !is_argv_on_heap ) {
        char const **const heap_argv = malloc( new_size );
        if ( unlikely( heap_argv == NULL ) )
          goto error;
        memcpy( heap_argv, *pargv, old_argc * sizeof(char*) );
        *pargv = heap_argv;
        is_argv_on_heap = true;
      }
      else {
        *pargv = realloc( *pargv, new_size );
        if ( *pargv == NULL )
          goto error;
      }

      // make space to insert new -I option before last argv (the filename)
      memmove( &(*pargv)[ argi + 1 ], &(*pargv)[ argi ], 
               STATIC_CAST( size_t, *pargc - argi + 1 ) * sizeof(char*) );

      char *new_arg = NULL;
      if ( unlikely( asprintf( &new_arg, "-I%s", abs_include_path ) == -1 ) )
        goto error;
      (*pargv)[ argi++ ] = new_arg;
      ++*pargc;
    } // while

    (*pargv)[ *pargc ] = NULL;
  }

  free( line_buf );
  if ( ferror( clang ) )
    goto error;
  pclose( clang );
  return;

error:
  fatal_error( EX_UNAVAILABLE, "invoking clang failed%s\n", STRERROR() );
}

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
 * Adds \a include_path to the global list of include (`-I`) paths.
 *
 * @param include_path The include path to add.
 */
static void include_add_path( char const *include_path ) {
  assert( include_path != NULL );

  char real_path[ PATH_MAX ];
  if ( realpath( include_path, real_path ) != NULL )
    include_path = real_path;

  size_t i = 0;

  if ( include_path_list == NULL ) {
    include_path_cap = 1;
    include_path_list = MALLOC( char*, include_path_cap + 1 );
    goto add_path;
  }

  for ( ; include_path_list[i] != NULL; ++i ) {
    if ( strcmp( include_path, include_path_list[i] ) == 0 )
      return;
  } // for

  if ( i >= include_path_cap ) {
    include_path_cap *= 2;
    REALLOC( include_path_list, char*, include_path_cap + 1 );
  }

add_path:
  include_path_list[  i] = check_strdup( include_path );
  include_path_list[++i] = NULL;
}

/**
 * Moves include-tidy specific command-line options to a separate array.
 *
 * @remarks
 * @parblock
 * Command-line options of the form have:
 *
 *  + <tt>-Xtidy</tt> _option_ have <tt>-Xtidy</tt> removed and _option_ moved
 *    to \a *ptidy_argv.
 *  + <tt>-I</tt><i>path</i> or <tt>-I</tt> <i>path</i> copied to \a
 *    *ptidy_argv.
 *  + <tt>-x</tt><i>language</i> or <tt>-x</tt> <i>language</i> copied to \a
 *    *ptidy_argv.
 *  + <tt>--help</tt> or <tt>--version</tt> are moved to \a *ptidy_argv.
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
 *      include-tidy -Xtidy --foo -DNDEBUG -I/opt/local/include bar.c
 *
 *          orig_argc = 4
 *          orig_argv[0] = "include-tidy"
 *          orig_argv[1] = "-DNDEBUG"
 *          orig_argv[2] = "-I/opt/local/include"
 *          orig_argv[3] = "bar.c"
 *          orig_argv[4] = NULL
 *
 *          tidy_argc = 3
 *          tidy_argv[0] = "include-tidy"
 *          tidy_argv[1] = "--foo"
 *          orig_argv[2] = "-I/opt/local/include"
 *          tidy_argv[3] = NULL
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
    else if ( STRNCMPLIT( orig_argv[i], "-" SOPT(INCLUDE)  ) == 0 ||
              STRNCMPLIT( orig_argv[i], "-" SOPT(LANGUAGE) ) == 0 ) {
      orig_argv[ new_argc++  ] = orig_argv[ i ];
      tidy_argv[ tidy_argc++ ] = orig_argv[ i ];
      if ( orig_argv[i][2] == '\0' ) {  // -I dir, not -Idir
        if ( ++i >= orig_argc )
          fatal_error( EX_USAGE, "-%c requires argument\n", orig_argv[i][1] );
        orig_argv[ new_argc++  ] = orig_argv[ i ];
        tidy_argv[ tidy_argc++ ] = orig_argv[ i ];
      }
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
 * Cleans-up options.
 */
static void options_cleanup( void ) {
  if ( include_path_list != NULL ) {
    for ( char **ppath = include_path_list; *ppath != NULL; ++ppath )
      free( *ppath );
    free( include_path_list );
  }
}

/**
 * Parses the language to use.
 *
 * @param language Either `"c"` or `"c++"`.
 * @return Returns \a language.
 */
NODISCARD
static char const* parse_language( char const *language ) {
  assert( language != NULL );

  if ( strcmp( language, "c" ) == 0 || strcmp( language, "c++" ) == 0 )
    return language;

  fatal_error( EX_USAGE,
    "\"%s\": invalid value for -" SOPT(LANGUAGE) "; must be either c or c++\n",
    language
  );
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
    "usage: %s [-Xtidy tidy-option]... [clang-option]... source-file\n"
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

char const* include_resolve( char const *included_path ) {
  assert( included_path != NULL );

  size_t      longest_include_path_len = 0;
  char const *shortest_include_path = included_path;

  for ( char **ppath = include_path_list; *ppath != NULL; ++ppath ) {
    char const *const include_path_i      = *ppath;
    size_t const      include_path_i_len  = strlen( include_path_i );

    if ( include_path_i_len > longest_include_path_len &&
        strncmp( included_path, include_path_i, include_path_i_len ) == 0 ) {
      longest_include_path_len = include_path_i_len;
      shortest_include_path = included_path + include_path_i_len;

      if ( shortest_include_path[0] == '/' )
        ++shortest_include_path;
    }
  } // for

  if ( STRNCMPLIT( shortest_include_path, "./" ) == 0 )
    shortest_include_path += STRLITLEN( "./" );

  return shortest_include_path;
}

void options_init( int *pargc, char const **pargv[] ) {
  ASSERT_RUN_ONCE();

  int           opt;
  bool          opt_help = false;
  bool          opt_version = false;
  int           tidy_argc;
  char const  **tidy_argv;

  add_clang_include_paths( pargc, pargv );
  move_tidy_args( pargc, *pargv, &tidy_argc, &tidy_argv );

  include_add_path( "." );
  ATEXIT( &options_cleanup );

  opterr = 1;

  for (;;) {
    opt = getopt_long(
      tidy_argc, CONST_CAST( char**, tidy_argv ),
      ":"                               // return missing argument as ':'
      SOPT(INCLUDE)   ":"
      SOPT(LANGUAGE)  ":"
      SOPT(VERBOSE)
      , OPTIONS, /*longindex=*/NULL
    );
    if ( opt == -1 )
      break;
    switch ( opt ) {
      case COPT(CLANG):
        if ( *SKIP_WS( optarg ) == '\0' )
          goto missing_arg;
        opt_clang_path = optarg;
        break;
      case COPT(HELP):
        opt_help = true;
        break;
      case COPT(INCLUDE):
        if ( *SKIP_WS( optarg ) == '\0' )
          goto missing_arg;
        include_add_path( optarg );
        break;
      case COPT(LANGUAGE):
        if ( *SKIP_WS( optarg ) == '\0' )
          goto missing_arg;
        opt_language = parse_language( optarg );
        break;
      case COPT(VERBOSE):
        ++opt_verbose;
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

  if ( opt_verbose > 0 ) {
    int i;
    PUTS( "/*\n" );
    PUTS( "  clang argv\n" );
    for ( i = 0; i < *pargc; ++i )
      PRINTF( "    %2d %s\n", i, (*pargv)[i] );
    PUTS( "\n  tidy argv\n" );
    for ( i = 0; i < tidy_argc; ++i )
      PRINTF( "    %2d %s\n", i, tidy_argv[i] );
    PUTS( "*/\n" );
  }

  tidy_argc -= optind - 1;
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

  tidy_source_path = (*pargv)[ --*pargc ];
  (*pargv)[ *pargc ] = NULL;

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
