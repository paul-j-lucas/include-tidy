/*
**      include-tidy -- #include tidier
**      src/cli_options.c
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
 * Defines macros and functions for command-line options.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "cli_options.h"
#include "include-tidy.h"
#include "options.h"
#include "print.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <ctype.h>                      /* for isalnum(), isprint() */
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>                     /* for exit() */
#include <string.h>                     /* for str...() */
#include <sysexits.h>

/// @endcond

/**
 * @addtogroup cli-options-group
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * Convenience macro for iterating over all command-line options.
 *
 * @param VAR The `struct option` loop variable.
 */
#define FOREACH_CLI_OPTION(VAR, OPTIONS) \
  for ( struct option const *VAR = (OPTIONS); (VAR)->name != NULL; ++(VAR) )

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct ext_lang_map ext_lang_map;

////////// structures /////////////////////////////////////////////////////////

/**
 * Mapping from source file extension to programming language, either C or C++.
 */
struct ext_lang_map {
  char const *ext;                      ///< Extension (without the `'.'`).
  char const *lang;                     ///< Language: either `"c"` or `"c++"`.
};

////////// local constants ////////////////////////////////////////////////////

/**
 * Mapping of all common C and C++ filename extensions to programming language.
 */
static ext_lang_map const EXT_LANG_MAP[] = {
  { "c",   "c"   },
  { "c++", "c++" },
  { "cc",  "c++" },
  { "cp",  "c++" },
  { "cpp", "c++" },
  { "cxx", "c++" },
  { "h",   "c"   },
  { "h++", "c++" },
  { "hh",  "c++" },
  { "hp",  "c++" },
  { "hpp", "c++" },
  { "hxx", "c++" },
};

/**
 * Command-line options.
 *
 * @sa OPTIONS_HELP
 */
static struct option const OPTIONS[] = {
  { "align-column",     required_argument,  NULL, COPT(ALIGN_COLUMN)      },
  { "all-includes",     no_argument,        NULL, COPT(ALL_INCLUDES)      },
  { "clang",            required_argument,  NULL, COPT(CLANG)             },
  { "color",            required_argument,  NULL, COPT(COLOR)             },
  { "comment-style",    required_argument,  NULL, COPT(COMMENT_STYLE)     },
  { "config",           required_argument,  NULL, COPT(CONFIG)            },
  { "error",            required_argument,  NULL, COPT(ERROR)             },
  { "help",             no_argument,        NULL, COPT(HELP)              },
  { "line-length",      required_argument,  NULL, COPT(LINE_LENGTH)       },
  { "no-config-layers", no_argument,        NULL, COPT(NO_CONFIG_LAYERS)  },
  { "verbose",          required_argument,  NULL, COPT(VERBOSE)           },
  { "version",          no_argument,        NULL, COPT(VERSION)           },
  { NULL,               0,                  NULL, 0                       }
};

/**
 * Command-line options help.
 *
 * @note It is indexed by short option characters.
 *
 * @sa get_opt_help()
 * @sa OPTIONS
 */
static char const *const OPTIONS_HELP[] = {
  [ COPT(ALIGN_COLUMN) ] = "Align comments to this column; default=" STRINGIFY(OPT_LINE_LENGTH_DEFAULT),
  [ COPT(ALL_INCLUDES) ] = "Print all include files",
  [ COPT(CLANG) ] = "Path of clang to use or \"none\"; default=\"" OPT_CLANG_DEFAULT "\"",
  [ COPT(COLOR) ] = "When to colorize output; default=\"not_file\"",
  [ COPT(COMMENT_STYLE) ] = "Comment style: \"//\", \"/*\", or \"none\"",
  [ COPT(CONFIG) ] = "Configuration file path",
  [ COPT(ERROR) ] = "When to exit with a non-zero status",
  [ COPT(HELP) ] = "Print this help and exit",
  [ COPT(LINE_LENGTH) ] = "Line length; default=" STRINGIFY(OPT_LINE_LENGTH_DEFAULT),
  [ COPT(NO_CONFIG_LAYERS) ] = "Don't do configuration file layering",
  [ COPT(VERBOSE) ] = "Print verbose output",
  [ COPT(VERSION) ] = "Print version and exit",
};

////////// local functions ////////////////////////////////////////////////////

NODISCARD
static char const*  get_opt_format( int ),
                 *  get_opt_long( char ),
                 *  is_long_opt( int, char const *const[], char const*, int* ),
                 *  is_short_opt( int, char const *const[], char, int* );

NODISCARD
static bool         is_Xtidy_opt( int, char const *const[], int* );

/////////// local functions ///////////////////////////////////////////////////

/**
 * Calls **clang**(1) and parses its verbose output to get the list of include
 * search paths.
 *
 * @param pargc A pointer to the argument count from \c main().
 * @param pargv A pointer to the argument values from \c main().
 * @param clang_path The path of the **clang** to use.
 * @param lang The language to use, either `"c"` or `"c++"`.
 */
static void add_clang_include_paths( int *pargc, char const **pargv[],
                                     char const *clang_path,
                                     char const *lang ) {
  assert(  pargc != NULL );
  assert( *pargc > 0 );
  assert(  pargv != NULL );
  assert( *pargv != NULL );
  assert( clang_path != NULL );
  assert( lang != NULL );
  ASSERT_RUN_ONCE();

  static char const CLANG_TEMPLATE[] =
    "%s"          // clang path
    " -E"         // run only the preprocessor stage
    " -v"         // show verbose output
    " -x%s"       // set langauge: c or c++
    " -"          // read from stdin
    " </dev/null" // set stdin to /dev/null
    " 2>&1";      // redirect stderr to stdout

  char *clang_command = NULL;
  check_asprintf( &clang_command, CLANG_TEMPLATE, clang_path, lang );

  FILE *const fclang = popen( clang_command, "r" );
  free( clang_command );
  if ( fclang == NULL )
    goto error;

  bool    found_include_search_paths = false;
  bool    is_argv_on_heap = false;
  char   *line_buf = NULL;
  size_t  line_cap = 0;

  while ( getline( &line_buf, &line_cap, fclang ) != -1 ) {
    if ( strcmp( line_buf, "#include <...> search starts here:\n" ) == 0 ) {
      found_include_search_paths = true;
      break;
    }
  } // while

  if ( found_include_search_paths ) {
    int argi = *pargc;                  // where to insert new -isystem option
    for ( int i = *pargc - 1; i > 0; --i ) {
      if ( (*pargv)[i][0] != '-' ) {
        argi = i;
        break;
      }
    } // for

    while ( getline( &line_buf, &line_cap, fclang ) != -1 ) {
      if ( strcmp( line_buf, "End of search list.\n" ) == 0 )
        break;

#ifdef __APPLE__
      // On macOS, clang's include search paths include frameworks directories
      // denoted by having paths followed by " (framework directory)".  These
      // don't contain .h file directly, so there's no point in including them.
      if ( strstr( line_buf, "(framework directory)" ) != NULL )
        continue;
#endif /* __APPLE__ */

      char const *const include_path = str_trim( line_buf );
      if ( include_path[0] != '/' )
        continue;

      size_t const old_argc = STATIC_CAST( size_t, *pargc );
      size_t const new_argc = old_argc + 1/*new arg*/ + 1/*NULL*/;

      if ( false_set( &is_argv_on_heap ) ) {
        // We can't realloc the argv passed to main(), so in order to insert an
        // argument, we first have to duplicate it into the heap.
        char const **const heap_argv = MALLOC( char*, new_argc );
        memcpy( heap_argv, *pargv, old_argc * sizeof(char*) );
        *pargv = heap_argv;
      }
      else {
        REALLOC( *pargv, char*, new_argc );
      }

      // Insert new -isystem option before last argv (the filename).
      memmove( &(*pargv)[ argi + 1 ], &(*pargv)[ argi ],
               STATIC_CAST( size_t, *pargc - argi + 1 ) * sizeof(char*) );

      char *new_arg = NULL;
      check_asprintf( &new_arg, "-isystem%s", include_path );
      (*pargv)[ argi++ ] = new_arg;
      ++*pargc;
    } // while

    (*pargv)[ *pargc ] = NULL;
  }

  free( line_buf );
  if ( ferror( fclang ) )
    goto error;
  pclose( fclang );
  return;

error:
  fatal_error( EX_UNAVAILABLE,
    "\"%s\": invocation failed: %s\n", clang_path, STRERROR()
  );
}

/**
 * If \a opt was given, checks that _only_ it was given and, if not, prints an
 * error message and exits; if \a opt was not given, does nothing.
 *
 * @param opt The option to check for.
 */
static void check_opt_exclusive( int opt ) {
  if ( !opt_is_set( opt ) )
    return;
  for ( int curr_opt = '0'; curr_opt < 256; ++curr_opt ) {
    if ( curr_opt == opt )
      continue;
    if ( opt_is_set( curr_opt ) ) {
      fatal_error( EX_USAGE,
        "%s can be given only by itself\n",
        get_opt_format( opt )
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
 * Gets the path to **clang**, if given.
 *
 * @param argc The command-line argument count.
 * @param argv The command-line argument values.
 * @return Returns the path to **clang** or NULL if it's `"none"`.
 */
static char const* get_clang_path( int argc, char const *const argv[] ) {
  assert( argc > 0 );
  assert( argv != NULL );

  for ( int i = 1; i < argc; ++i ) {
    if ( !is_Xtidy_opt( argc, argv, &i ) )
      continue;
    char const *clang_path = is_short_opt( argc, argv, COPT(CLANG), &i );
    if ( clang_path == NULL )
      clang_path = is_long_opt( argc, argv, "clang", &i );
    if ( clang_path != NULL )
      return strcmp( clang_path, "none" ) == 0 ? NULL : clang_path;
  } // for

  return OPT_CLANG_DEFAULT;
}

/**
 * Gets the language from \a ext.
 *
 * @param ext A filename extension (without the dot).
 * @return Returns either `"c"` (for C) or `"c++"` (for C++).
 */
NODISCARD
static char const* get_ext_language( char const *ext ) {
  assert( ext != NULL );

  FOREACH_ARRAY_ELEMENT( ext_lang_map, m, EXT_LANG_MAP ) {
    if ( strcasecmp( ext, m->ext ) == 0 )
      return m->lang;
  } // for

  return NULL;
}

/**
 * Formats an option as `--%%s/-%%c` where `%%s` is the long option and `%%c`
 * is the short option.
 *
 * @param short_opt The short option (along with its corresponding long option)
 * to format.
 * @return Returns the formatted string.
 *
 * @warning The pointer returned is to a static buffer, so you can't do
 * something like call this twice in the same `printf()` statement.
 */
NODISCARD
static char const* get_opt_format( int short_opt ) {
  static char opt_buf[32];              // big enough

  char const *const long_opt = get_opt_long( STATIC_CAST( char, short_opt ) );
  check_snprintf( opt_buf, sizeof opt_buf, "--%s/-%c", long_opt, short_opt );
  return opt_buf;
}

/**
 * Gets the help message for \a opt.
 *
 * @param opt The option to get the help for.
 * @return Returns said help message.
 */
NODISCARD
static char const* get_opt_help( int opt ) {
  assert( opt > 0 );
  assert( STATIC_CAST( unsigned, opt ) < ARRAY_SIZE( OPTIONS_HELP ) );
  char const *const help = OPTIONS_HELP[ opt ];
  assert( help != NULL );
  return help;
}

/**
 * Gets the corresponding name of the long option for the given short option.
 *
 * @param short_opt The short option to get the corresponding long option for.
 * @return Returns the said name.
 */
NODISCARD
static char const* get_opt_long( char short_opt ) {
  FOREACH_CLI_OPTION( opt, OPTIONS ) {
    if ( opt->val == short_opt )
      return opt->name;
  } // for
  assert( false && "option not found" );
}

/**
 * Gets the `option` corresponding to \a short_opt.
 *
 * @param short_opt The short option to get the option for.
 * @return Returns the corresponding `option` or NULL if not found.
 */
NODISCARD
static struct option const* get_option( char short_opt ) {
  FOREACH_CLI_OPTION( opt, OPTIONS ) {
    if ( opt->val == short_opt )
      return opt;
  } // for
  return NULL;                          // LCOV_EXCL_LINE
}

/**
 * Gets the source path from the last argument, if any.
 *
 * @param argc The argument count from \c main().
 * @param argv The argument values from \c main().
 * @return Returns the source path or NULL if none.
 */
static char const* get_source_path( int argc, char const *argv[] ) {
  assert( argc > 0 );
  assert( argv != NULL );

  if ( argc < 2 )                       // can't be a source path
    return NULL;
  char const *const last_argv = (argv)[ argc - 1 ];
  if ( last_argv[0] == '-' )            // last doesn't look like a filename
    return NULL;
  return last_argv;
}

/**
 * Gets the language of clang's `-x` option, if given.
 *
 * @param argc The command-line argument count.
 * @param argv The command-line argument values.
 * @return Returns the language of clang's `-x` option, either `"c"` or
 * `"c++"`, or NULL if not given.
 */
static char const* get_x_language( int argc, char const *const argv[] ) {
  assert( argc > 0 );
  assert( argv != NULL );

  for ( int i = 1; i < argc; ++i ) {
    char const *const lang = is_short_opt( argc, argv, 'x', &i );
    if ( lang == NULL )
      continue;
    if ( strcmp( lang, "c" ) == 0 )
      return "c";
    if ( strcmp( lang, "c++" ) == 0 )
      return "c++";
    fatal_error( EX_USAGE,
      "\"%s\": invalid value for -x; must be either \"c\" or \"c++\"\n", lang
    );
  } // for

  return NULL;
}

/**
 * Checks whether the \a argv[\a *pargi] is a long option \a opt having an
 * argument.
 *
 * @param argc The command-line argument count.
 * @param argv The command-line argument values.
 * @param opt The long option (without the `--`).
 * @param pargi A pointer to the current argument index variable.
 * @return Returns the value of the argument or NULL if \a argv[\a *pargi] is
 * not \a opt.
 *
 * @sa is_short_opt()
 */
static char const* is_long_opt( int argc, char const *const argv[],
                                char const *opt, int *pargi ) {
  assert( argc > 0 );
  assert( argv != NULL );
  assert( opt != NULL );
  assert( pargi != NULL );

  char const *const arg = argv[ *pargi ];

  if ( STRNCMPLIT( arg, "--" ) != 0 )
    return NULL;
  size_t const opt_len = strlen( opt );
  if ( strncmp( arg + STRLITLEN( "--" ), opt, opt_len ) != 0 )
    return NULL;
  if ( arg[ STRLITLEN( "--" ) + opt_len ] == '=' ) {
    char const *const value = arg + STRLITLEN( "--" ) + opt_len + 1;
    if ( value[0] != '\0' )
      return value;
  }
  else if ( ++*pargi < argc ) {
    char const *const value = argv[ *pargi ];
    if ( value != NULL && value[0] != '\0' )
      return value;
  }

  fatal_error( EX_USAGE, "\"--%s\" requires an argument\n", opt );
}

/**
 * Checks whether the \a argv[\a *pargi] is a short option \a opt having an
 * argument.
 *
 * @param argc The command-line argument count.
 * @param argv The command-line argument values.
 * @param opt The short option character.
 * @param pargi A pointer to the current argument index variable.
 * @return Returns the value of the argument or NULL if \a argv[\a *pargi] is
 * not \a opt.
 *
 * @sa is_long_opt()
 */
static char const* is_short_opt( int argc, char const *const argv[],
                                 char opt, int *pargi ) {
  assert( argc > 0 );
  assert( argv != NULL );
  assert( pargi != NULL );

  if ( argv[ *pargi ][0] != '-' || argv[ *pargi ][1] != opt )
    return NULL;
  if ( argv[ *pargi ][2] != '\0' )    // -x<arg>, not -x <arg>
    return argv[ *pargi ] + 2;
  if ( ++*pargi < argc ) {
    char const *const value = argv[ *pargi ];
    if ( value != NULL && value[0] != '\0' )
      return value;
  }

  fatal_error( EX_USAGE, "\"-%c\" requires an argument\n", opt );
}

/**
 * Checks whether the \a argv[\a *pargi] is `-Xtidy` and followed by a
 * subsequent option.
 *
 * @param argc The command-line argument count.
 * @param argv The command-line argument values.
 * @param pargi A pointer to the current argument index variable.
 * @return Returns `true` only if `argv[*pargi]` is `-Xtidy` and is followed by
 * a subsequent option.
 */
static bool is_Xtidy_opt( int argc, char const *const argv[], int *pargi ) {
  assert( argc > 0 );
  assert( argv != NULL );
  assert( pargi != NULL );
  assert( *pargi < argc );

  if ( strcmp( argv[ *pargi ], "-Xtidy" ) != 0 )
    return false;
  if ( ++*pargi >= argc )
    fatal_error( EX_USAGE, "-Xtidy requires subsequent option\n" );
  return true;
}

/**
 * Makes the `optstring` (short option) equivalent of \a opts for the third
 * argument of `getopt_long()`.
 *
 * @param opts An array of options to make the short option string from.  Its
 * last element must be all zeros.
 * @param extra_opts Extra options to add.  May be NULL.
 * @return Returns the `optstring` for the third argument of `getopt_long()`.
 * The caller is responsible for freeing it.
 */
NODISCARD
static char const* make_short_opts( struct option const opts[static const 2],
                                    char const *extra_opts ) {
  extra_opts = empty_if_null( extra_opts );
  size_t const extra_opts_len = strlen( extra_opts );

  // pre-flight to calculate string length
  size_t len = 1 /* for leading ':' */ + extra_opts_len;
  FOREACH_CLI_OPTION( opt, opts ) {
    assert( opt->has_arg >= 0 && opt->has_arg <= 2 );
    len += 1 + STATIC_CAST( unsigned, opt->has_arg );
  } // for

  char *const short_opts = MALLOC( char, len + 1/*\0*/ );
  char *s = short_opts;

  *s++ = ':';                           // return missing argument as ':'
  FOREACH_CLI_OPTION( opt, opts ) {
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

  strcpy( s, extra_opts );
  s += extra_opts_len;
  *s = '\0';

  return short_opts;
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
 *  + <tt>-isystem</tt><i>path</i> or <tt>-isystem</tt> <i>path</i> copied to
 *    \a *ptidy_argv, but as <tt>-I</tt><i>path</i>.
 *  + <tt>--help</tt> or <tt>--version</tt> are moved to \a *ptidy_argv.
 *
 * All other options and arguments are left as-is in \a argv.  Examples:
 *
 *      include-tidy --help
 *
 *          argc = 1
 *          argv[0] = "include-tidy"
 *          argv[1] = NULL
 *
 *          tidy_argc = 2
 *          tidy_argv[0] = "include-tidy"
 *          tidy_argv[1] = "--help"
 *          tidy_argv[2] = NULL
 *
 *      include-tidy -Xtidy --foo -DNDEBUG -I/opt/local/include bar.c
 *
 *          argc = 4
 *          argv[0] = "include-tidy"
 *          argv[1] = "-DNDEBUG"
 *          argv[2] = "-I/opt/local/include"
 *          argv[3] = "bar.c"
 *          argv[4] = NULL
 *
 *          tidy_argc = 3
 *          tidy_argv[0] = "include-tidy"
 *          tidy_argv[1] = "--foo"
 *          tidy_argv[2] = "-I/opt/local/include"
 *          tidy_argv[3] = NULL
 * @endparblock
 *
 * @param pargc A pointer to `argc`.
 * @param argv A copy of `argv`.
 * @param ptidy_argc A pointer to an `argc` for include-tidy specific options.
 * @param ptidy_argv A pointer to an `argv` for include-tidy specific options.
 * The caller is responsible for free'ing the array of pointers to strings, but
 * _not_ the strings themselves.
 */
static void move_tidy_args( int *pargc, char const *argv[],
                            int *ptidy_argc, char const **ptidy_argv[] ) {
  assert(  pargc != NULL );
  assert( *pargc > 0 );
  assert(   argv != NULL );
  assert( ptidy_argc != NULL );
  assert( ptidy_argv != NULL );

  int const argc = *pargc;
  int new_argc = 1, tidy_argc = 1;

  char const **const tidy_argv =
    MALLOC( char*, STATIC_CAST( size_t, argc ) + 1/*NULL*/ );
  tidy_argv[0] = argv[0];

  for ( int i = 1; i < argc; ++i ) {
    if ( is_Xtidy_opt( argc, argv, &i ) ) {
      tidy_argv[ tidy_argc++ ] = argv[i];
      if ( argv[i][0] == '-' && isalnum( argv[i][1] ) ) {
        if ( argv[i][2] != '\0' )
          continue;
        char const short_opt = argv[i][1];
        struct option const *const opt = get_option( short_opt );
        if ( opt == NULL ) {
          fatal_error( EX_USAGE,
            "'%c': invalid option; use --help for help\n", short_opt
          );
        }
        if ( opt->has_arg == no_argument )
          continue;
        if ( ++i >= argc )
          fatal_error( EX_USAGE, "\"-%c\" requires an argument\n", short_opt );
        tidy_argv[ tidy_argc++ ] = argv[i];
      }
    }
    else if ( STRNCMPLIT( argv[i], "-I" ) == 0 ) {
      argv[ new_argc++ ] = argv[i];
      tidy_argv[ tidy_argc++ ] = argv[i];
      if ( argv[i][2] == '\0' ) {  // -I <dir>, not -I<dir>
        if ( ++i >= argc )
          fatal_error( EX_USAGE, "\"-%c\" requires an argument\n", argv[i][1] );
        argv[ new_argc++ ] = argv[i];
        tidy_argv[ tidy_argc++ ] = argv[i];
      }
    }
    else if ( STRNCMPLIT( argv[i], "-isystem" ) == 0 ) {
      argv[ new_argc++ ] = argv[i];
      char *new_arg = NULL;
      // Convert -isystem to -I for include-tidy.
      check_asprintf( &new_arg, "-I%s", argv[i] + STRLITLEN( "-isystem" ) );
      tidy_argv[ tidy_argc++ ] = new_arg;
      if ( argv[i][STRLITLEN( "-isystem" )] == '\0' ) {
        // -isystem <dir>, not -isystem<dir>
        if ( ++i >= argc )
          fatal_error( EX_USAGE, "\"-isystem\" requires an argument\n" );
        argv[ new_argc++ ] = argv[i];
        tidy_argv[ tidy_argc++ ] = argv[i];
      }
    }
    else if ( STRNCMPLIT( argv[i], "--help" ) == 0 ||
              STRNCMPLIT( argv[i], "--version" ) == 0 ) {
      tidy_argv[ tidy_argc++ ] = argv[i];
    }
    else {
      argv[ new_argc++ ] = argv[i];
    }
  } // for

  argv[ new_argc ] = tidy_argv[ tidy_argc ] = NULL;

  *pargc = new_argc;
  *ptidy_argc = tidy_argc;
  *ptidy_argv = tidy_argv;
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
  FOREACH_CLI_OPTION( opt, OPTIONS ) {
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

  fprintf( fout,
    "usage: %s [-Xtidy tidy-option]... [clang-option]... source-file\n"
    "       %s other-option\n"
    "\n"
    "tidy options:\n"
    , prog_name, prog_name
  );

  FOREACH_CLI_OPTION( opt, OPTIONS ) {
    switch ( opt->val ) {
      case COPT(HELP):
      case COPT(VERSION):
        // These are special in that they are allowed directly rather than
        // following -Xtidy.  Consequently, they don't have short options and
        // also must be printed seperately (below).
        continue;
    } // switch
    fprintf( fout, "  --%s", opt->name );
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
    fprintf( fout, " (-%c) %s.\n", opt->val, get_opt_help( opt->val ) );
  } // for

  fputs( "\nother options:\n", fout );

  FOREACH_CLI_OPTION( opt, OPTIONS ) {
    switch ( opt->val ) {
      case COPT(HELP):
      case COPT(VERSION):
        assert( opt->has_arg == no_argument );
        fprintf( fout, "  --%s", opt->name );
        size_t const opt_len = strlen( opt->name );
        assert( opt_len <= longest_opt_len );
        FPUTNSP( longest_opt_len - opt_len + STRLITLEN( " (-?) " ), fout );
        fprintf( fout, "%s.\n", get_opt_help( opt->val ) );
    } // switch
  } // for

  fputs(
    "\n"
    PACKAGE_NAME " home page: " PACKAGE_URL "\n"
    "Report bugs to: " PACKAGE_BUGREPORT "\n",
    fout
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

void cli_options_init( int *pargc, char const **pargv[] ) {
  assert(  pargc != NULL );
  assert( *pargc > 0 );
  assert(  pargv != NULL );
  assert( *pargv != NULL );
  ASSERT_RUN_ONCE();

  // The order that we have to parse command-line arguments is necessitated to
  // be unconventional.
  //
  // Ordinarily, a program would parse all options first, increment argv past
  // them, then look at (the new) argv[1] for the file; but the source file may
  // be needed before parsing options because its language (based on its
  // filename extension) affects the list of system include files and the
  // corresponding `-isystem` options needed by clang.
  //
  // We also have to pre-scan all options looking for clang's -x<language>
  // option because that has priority over whatever language is indicated by
  // the source file's extension.
  //
  // Finally, we have to call **clang** and insert `-isystem` options for the
  // include paths it would use to compile the source file.

  arg_source_path = get_source_path( *pargc, *pargv );
  char const *const clang_path = get_clang_path( *pargc, *pargv );
  char const *const ext =
    arg_source_path != NULL ? path_ext( arg_source_path ) : NULL;
  char const *lang = get_x_language( *pargc, *pargv );
  if ( lang == NULL && ext != NULL )
    lang = get_ext_language( ext );
  if ( clang_path != NULL && lang != NULL )
    add_clang_include_paths( pargc, pargv, clang_path, lang );

  // So #include "/path/to/current/directory/foo.h" will get relativized to
  // "foo.h".
  opt_include_paths_add( "." );

  int               opt;
  bool              opt_help = false;
  bool              opt_version = false;
  char const *const short_opts = make_short_opts( OPTIONS, SOPT(INCLUDE) ":" );
  int               tidy_argc;
  char const      **tidy_argv;

  move_tidy_args( pargc, *pargv, &tidy_argc, &tidy_argv );

  opterr = 1;
  for (;;) {
    opt = getopt_long(
      tidy_argc, CONST_CAST( char**, tidy_argv ), short_opts, OPTIONS,
      /*longindex=*/NULL
    );
    if ( opt == -1 )
      break;
    switch ( opt ) {
      case COPT(ALIGN_COLUMN):;
        if ( !opt_align_column_parse( optarg ) ) {
          fatal_error( EX_USAGE,
            "\"%s\": invalid value for %s; must be 0-%d\n",
            optarg, get_opt_format( opt ), OPT_ALIGN_COLUMN_MAX
          );
        }
        break;
      case COPT(ALL_INCLUDES):
        opt_all_includes = true;
        break;
      case COPT(CLANG):                 // already handled
        break;
      case COPT(COLOR):
        if ( *SKIP_WS( optarg ) == '\0' )
          goto missing_arg;
        if ( !opt_color_parse( optarg ) ) {
          fatal_error( EX_USAGE,
            "\"%s\": invalid value for %s\n",
            optarg, get_opt_format( opt )
          );
        }
        break;
      case COPT(COMMENT_STYLE):
        if ( *SKIP_WS( optarg ) == '\0' )
          goto missing_arg;
        if ( !opt_comment_style_parse( optarg ) ) {
          fatal_error( EX_USAGE,
            "\"%s\": invalid value for %s;"
            " must be one of \"//\", \"/*\", or \"none\"\n",
            optarg, get_opt_format( opt )
          );
        }
        opt_mark_set( COPT(COMMENT_STYLE) );
        break;
      case COPT(CONFIG):
        if ( *SKIP_WS( optarg ) == '\0' )
          goto missing_arg;
        opt_config_path = optarg;
        break;
      case COPT(ERROR):
        if ( *SKIP_WS( optarg ) == '\0' )
          goto missing_arg;
        if ( !opt_error_parse( optarg ) ) {
          fatal_error( EX_USAGE,
            "\"%s\": invalid value for %s;"
            " must be one of \"violations\", \"always\", or \"never\"\n",
            optarg, get_opt_format( opt )
          );
        }
        break;
      case COPT(HELP):
        opt_help = true;
        break;
      case COPT(INCLUDE):
        if ( *SKIP_WS( optarg ) == '\0' )
          goto missing_arg;
        opt_include_paths_add( optarg );
        break;
      case COPT(LINE_LENGTH):
        if ( !opt_line_length_parse( optarg ) ) {
          fatal_error( EX_USAGE,
            "\"%s\": invalid value for %s; must be 1-%d\n",
            optarg, get_opt_format( opt ), OPT_LINE_LENGTH_MAX
          );
        }
        break;
      case COPT(NO_CONFIG_LAYERS):
        opt_config_layers = false;
        break;
      case COPT(VERBOSE):
        if ( *SKIP_WS( optarg ) == '\0' )
          goto missing_arg;
        if ( !opt_verbose_parse( optarg ) ) {
          fatal_error( EX_USAGE,
            "\"%s\": invalid value for %s; must be [ais]\n",
            optarg, get_opt_format( opt )
          );
        }
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
    opt_mark_set( opt );
  } // for
  FREE( short_opts );

  if ( (opt_verbose & TIDY_VERBOSE_ARGS) != 0 ) {
    verbose_printf( "clang argv:\n" );
    for ( int i = 0; i < *pargc; ++i )
      verbose_printf( "  %2d %s\n", i, (*pargv)[i] );
    verbose_printf( "\n" );
    verbose_printf( "tidy argv:\n" );
    for ( int i = 0; i < tidy_argc; ++i )
      verbose_printf( "  %2d %s\n", i, tidy_argv[i] );
    verbose_printf( "\n" );
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

  if ( lang == NULL ) {
    EPRINTF( "%s: ", prog_name );
    if ( ext == NULL )
      EPUTS( "missing" );
    else
      EPRINTF( "\"%s\": unknown", ext );
    EPUTS( " extension; must be one of " );
    bool comma = false;
    FOREACH_ARRAY_ELEMENT( ext_lang_map, m, EXT_LANG_MAP )
      EPRINTF( true_or_set( &comma ) ? ", %s" : "%s", m->ext );
    EPUTS( "; or use -xc[++]\n" );
    exit( EX_USAGE );
  }

  // argv[argc-1] is the source file, but we've already copied it into
  // arg_source_path, so just NULL it out.
  (*pargv)[ --*pargc ] = NULL;

  return;

invalid_opt:;
  // Determine whether the invalid option was short or long.
  char const *const invalid_opt = tidy_argv[ optind - 1 ];
  EPRINTF( "%s: ", prog_name );
  if ( invalid_opt != NULL && strncmp( invalid_opt, "--", 2 ) == 0 )
    EPRINTF( "\"%s\"", invalid_opt + 2/*skip over "--"*/ );
  else
    EPRINTF( "'%c'", STATIC_CAST( char, optopt ) );
  EPRINTF( ": invalid option; use --help for help\n" );
  exit( EX_USAGE );

missing_arg:
  fatal_error( EX_USAGE,
    "\"%s\" requires an argument\n",
    get_opt_format( STATIC_CAST( char, opt == ':' ? optopt : opt ) )
  );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
