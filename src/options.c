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
#include <errno.h>
#include <getopt.h>
#include <limits.h>                     /* for PATH_MAX */
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */
#include <stdio.h>                      /* for fdopen() */
#include <stdlib.h>                     /* for exit() */
#include <string.h>                     /* for str...() */
#include <strings.h>                    /* for strcasecmp() */
#include <sysexits.h>

// in ascending option character ASCII order; sort using: sort -k3b,3f -k3b,3r
#define OPT_ALIGN                 a
#define OPT_ALL_INCLUDES          A
#define OPT_CONFIG                c
#define OPT_CLANG                 C
#define OPT_HELP                  h
#define OPT_LINE_LENGTH           l
#define OPT_COMMENT_STYLE         s
#define OPT_VERSION               v
#define OPT_VERBOSE               V

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

/**
 * Convenience macro for iterating over all command-line options.
 *
 * @param VAR The `struct option` loop variable.
 */
#define FOREACH_CLI_OPTION(VAR, OPTIONS) \
  for ( struct option const *VAR = (OPTIONS); (VAR)->name != NULL; ++(VAR) )

#define OPT_CLANG_DEFAULT         "clang"
#define OPT_COMMENT_ALIGN_DEFAULT 41
#define OPT_COMMENT_ALIGN_MAX     256
#define OPT_LINE_LENGTH_DEFAULT   80
#define OPT_LINE_LENGTH_MAX       256
#define OPT_VERBOSE_ALL           "ais"

#define OPT_BUF_SIZE              32    /**< Maximum size for an option. */

///////////////////////////////////////////////////////////////////////////////

/**
 * Command-line options.
 *
 * @sa OPTIONS_HELP
 */
static struct option const OPTIONS[] = {
  { "align",          required_argument,  NULL, COPT(ALIGN)         },
  { "all-includes",   no_argument,        NULL, COPT(ALL_INCLUDES)  },
  { "clang",          required_argument,  NULL, COPT(CLANG)         },
  { "comment-style",  required_argument,  NULL, COPT(COMMENT_STYLE) },
  { "config",         required_argument,  NULL, COPT(CONFIG)        },
  { "help",           no_argument,        NULL, COPT(HELP)          },
  { "line-length",    required_argument,  NULL, COPT(LINE_LENGTH)   },
  { "verbose",        required_argument,  NULL, COPT(VERBOSE)       },
  { "version",        no_argument,        NULL, COPT(VERSION)       },
  { NULL,             0,                  NULL, 0                   }
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
  [ COPT(ALIGN) ] = "Align comments to this column; default=" STRINGIFY(OPT_LINE_LENGTH_DEFAULT),
  [ COPT(ALL_INCLUDES) ] = "Print all include files",
  [ COPT(CLANG) ] = "Path of clang to use; default=\"" OPT_CLANG_DEFAULT "\"",
  [ COPT(COMMENT_STYLE) ] = "Comment style: \"//\", \"/*\", or \"none\"",
  [ COPT(CONFIG) ] = "Configuration file path",
  [ COPT(HELP) ] = "Print this help and exit",
  [ COPT(LINE_LENGTH) ] = "Line length; default=" STRINGIFY(OPT_LINE_LENGTH_DEFAULT),
  [ COPT(VERBOSE) ] = "Print verbose output",
  [ COPT(VERSION) ] = "Print version and exit",
};

// extern option variables
bool                opt_all_includes;
unsigned            opt_comment_align = OPT_COMMENT_ALIGN_DEFAULT;
char const         *opt_comment_style[2] = { "// ", "" };
char const         *opt_config_path;
unsigned            opt_line_length = OPT_LINE_LENGTH_DEFAULT;
tidy_verbose        opt_verbose;

// local option variables
static char       **opt_include_paths;  ///< Null-terminated list of `-I` paths.

// local variable definitions
static bool         opts_given[ 128 ];  ///< Table of options that were given.

// local functions
static void               include_add_path( char const* );

NODISCARD
static char const*        opt_format( char, char[const], size_t ),
                 *        opt_get_long( char );

NODISCARD
static unsigned long long parse_ull( char const* );

static void               set_all_or_none( char const**, char const* );

/////////// local functions ///////////////////////////////////////////////////

/**
 * Call **clang**(1) and parse its verbose output to get the list of include
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
  ASSERT_RUN_ONCE();
  assert( pargc != NULL );
  assert( pargv != NULL );
  assert( clang_path != NULL );
  assert( lang != NULL );

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

  bool    found_include_search = false;
  char   *line_buf = NULL;
  size_t  line_cap = 0;

  while ( getline( &line_buf, &line_cap, fclang ) != -1 ) {
    if ( strcmp( line_buf, "#include <...> search starts here:\n" ) == 0 ) {
      found_include_search = true;
      break;
    }
  } // while

  if ( found_include_search ) {
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
        if ( unlikely( *pargv == NULL ) )
          goto error;
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
  fatal_error(
    EX_UNAVAILABLE, "invoking %s failed: %s\n",
    clang_path, STRERROR()
  );
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
 * Gets the path to **clang**, if given.
 *
 * @param argc The command-line argument count.
 * @param argv The command-line argument values.
 * @return Returns the path to **clang**.
 */
static char const* get_clang_path( int argc, char const *const argv[] ) {
  for ( int i = 1; i < argc; ++i ) {
    if ( strcmp( argv[i], "-Xtidy" ) != 0 )
      continue;
    if ( ++i >= argc )
      fatal_error( EX_USAGE, "-Xtidy requires subsequent option\n" );
    if ( STRNCMPLIT( argv[i], "-" SOPT(CLANG) ) == 0 ) {
      if ( argv[i][2] == '\0' ) {       // -c <path>, not -c<path>
        if ( ++i >= argc )
          goto missing_arg;
        return argv[i];
      }
      return argv[i] + STRLITLEN( "-" SOPT(CLANG) );
    }
    if ( STRNCMPLIT( argv[i], "--clang" ) == 0 ) {
      char const *const equal = strchr( argv[i], '=' );
      if ( equal == NULL )
        goto missing_arg;
      char const *const path = equal + 1;
      if ( path[0] == '\0' )
        goto missing_arg;
      return path;
    }
  } // for

  return OPT_CLANG_DEFAULT;

missing_arg:
  fatal_error( EX_USAGE, "--clang/-%c requires an argument\n", COPT(CLANG) );
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
  assert( (unsigned)opt < ARRAY_SIZE( OPTIONS_HELP ) );
  char const *const help = OPTIONS_HELP[ opt ];
  assert( help != NULL );
  return help;
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
 * Gets the language of clang's `-x` option, if given.
 *
 * @param argc The command-line argument count.
 * @param argv The command-line argument values.
 * @return Returns the language of clang's `-x` option, either `"c"` or
 * `"c++"`, or NULL if not given.
 */
static char const* get_x_language( int argc, char const *const argv[] ) {
  for ( int i = 1; i < argc; ++i ) {
    if ( STRNCMPLIT( argv[i], "-x" ) != 0 )
      continue;
    char const *lang;
    if ( argv[i][2] == '\0' ) {         // -x <lang>, not -x<lang>
      if ( ++i >= argc )
        fatal_error( EX_USAGE, "-%c requires argument\n", argv[i][1] );
      lang = argv[i];
    }
    else {
      lang = argv[i] + STRLITLEN( "-x" );
    }

    if ( strcmp( lang, "c" ) == 0 )
      return "c";
    if ( strcmp( lang, "c++" ) == 0 )
      return "c++";

    fatal_error( EX_USAGE,
      "\"%s\": invalid value for -x; must be either \"c\" or \"c++\"\n",
      lang
    );
  } // for

  return NULL;
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
  assert( pargc != NULL );
  assert(  argv != NULL );
  assert( ptidy_argc != NULL );
  assert( ptidy_argv != NULL );

  int const argc = *pargc;
  int new_argc = 1, tidy_argc = 1;

  char const **const tidy_argv =
    MALLOC( char*, STATIC_CAST( size_t, argc ) + 1 );
  tidy_argv[0] = argv[0];

  for ( int i = 1; i < argc; ++i ) {
    if ( strcmp( argv[i], "-Xtidy" ) == 0 ) {
      if ( ++i >= argc )
        fatal_error( EX_USAGE, "-Xtidy requires subsequent option\n" );
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
          fatal_error( EX_USAGE, "-%c requires an argument\n", short_opt );
        tidy_argv[ tidy_argc++ ] = argv[i];
      }
    }
    else if ( STRNCMPLIT( argv[i], "-I" ) == 0 ) {
      argv[ new_argc++ ] = argv[i];
      tidy_argv[ tidy_argc++ ] = argv[i];
      if ( argv[i][2] == '\0' ) {  // -I <dir>, not -I<dir>
        if ( ++i >= argc )
          fatal_error( EX_USAGE, "-%c requires an argument\n", argv[i][1] );
        argv[ new_argc++ ] = argv[i];
        tidy_argv[ tidy_argc++ ] = argv[i];
      }
    }
    else if ( STRNCMPLIT( argv[i], "-isystem" ) == 0 ) {
      argv[ new_argc++ ] = argv[i];
      char *new_arg = NULL;
      check_asprintf( &new_arg, "-I%s", argv[i] + STRLITLEN( "-isystem" ) );
      tidy_argv[ tidy_argc++ ] = new_arg;
      if ( argv[i][STRLITLEN( "-isystem" )] == '\0' ) {
        // -isystem <dir>, not -isystem<dir>
        if ( ++i >= argc )
          fatal_error( EX_USAGE, "-isystem requires an argument\n" );
        argv[ new_argc++ ] = argv[i];
        tidy_argv[ tidy_argc++ ] = argv[i];
      }
    }
    else if ( strcmp( argv[i], "--help" ) == 0 ||
              strcmp( argv[i], "--version" ) == 0 ) {
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
  FOREACH_CLI_OPTION( opt, OPTIONS ) {
    if ( opt->val == short_opt )
      return opt->name;
  } // for
  return "";
}

/**
 * Cleans-up options.
 */
static void options_cleanup( void ) {
  if ( opt_include_paths != NULL ) {
    for ( char **ppath = opt_include_paths; *ppath != NULL; ++ppath )
      free( *ppath );
    free( opt_include_paths );
  }
}

/**
 * Parses a comment delimiter.
 *
 * @param comment_delim The comment delimiter to parse.
 */
static void parse_comment_style( char const *comment_delim ) {
  assert( comment_delim != NULL );

  if ( strcmp( comment_delim, "none" ) == 0 ) {
    opt_comment_style[0] = "";
    opt_comment_style[1] = "";
  }
  else if ( strcmp( comment_delim, "//" ) == 0 ) {
    opt_comment_style[0] = "// ";
    opt_comment_style[1] = "";
  }
  else if ( strcmp( comment_delim, "/*" ) == 0 ) {
    opt_comment_style[0] = "/* ";
    opt_comment_style[1] = " */";
  }
  else {
    fatal_error( EX_USAGE,
      "\"%s\": invalid value for --comment-style;"
      " must be one of \"//\", \"/*\", or \"none\"\n",
      comment_delim
    );
  }
}

/**
 * Parses the file extension of \a path.
 *
 * @param path The pathname.
 * @return Returns either `"c"` (for C) or `"c++"` (for C++).
 */
NODISCARD
static char const* parse_file_ext( char const *path ) {
  struct ext_lang_map {
    char const *ext;
    char const *lang;
  };
  typedef struct ext_lang_map ext_lang_map;

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

  assert( path != NULL );

  char const *const dot = strrchr( path, '.' );
  if ( dot == NULL || dot[1] == '\0' )
    return NULL;
  char const *const ext = dot + 1;

  FOREACH_ARRAY_ELEMENT( ext_lang_map, m, EXT_LANG_MAP ) {
    if ( strcasecmp( ext, m->ext ) == 0 )
      return m->lang;
  } // for

  EPRINTF(
    "%s: \"%s\": unknown file extension; must be one of ",
    prog_name, ext
  );
  bool comma = false;
  FOREACH_ARRAY_ELEMENT( ext_lang_map, m, EXT_LANG_MAP )
    EPRINTF( true_or_set( &comma ) ? ", %s" : "%s", m->ext );
  EPUTS( "; or use -xc[++]\n" );

  exit( EX_USAGE );
}

/**
 * Parses the alignment column number for comments.
 *
 * @param s The string to parse.
 * @return Returns the comment alignment column.
 */
NODISCARD
unsigned parse_comment_alignment( char const *s ) {
  assert( s != NULL );
  unsigned long long ull = parse_ull( s );
  if ( ull > OPT_COMMENT_ALIGN_MAX ) {
    fatal_error( EX_USAGE,
      "\"%s\": invalid value for --align/-a; must be 0-%d\n",
      s, OPT_COMMENT_ALIGN_MAX
    );
  }
  return STATIC_CAST( unsigned, ull );
}

/**
 * Parses the line length.
 *
 * @param s The string to parse.
 * @return Returns the line length.
 */
NODISCARD
unsigned parse_line_length( char const *s ) {
  assert( s != NULL );
  unsigned long long ull = parse_ull( s );
  if ( ull > OPT_LINE_LENGTH_MAX ) {
    fatal_error( EX_USAGE,
      "\"%s\": invalid value for --line-length/-l; must be 1-%d\n",
      s, OPT_LINE_LENGTH_MAX
    );
  }
  return STATIC_CAST( unsigned, ull );
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
 * Parses the value of the **include-tidy** verbose option.
 *
 * @param verbose_format
 * @parblock
 * The null-terminated **include-tidy** debug format string to parse.
 * Value format are:
 *
 * Format | Meaning
 * -------|-----------------------------------------------------------------
 * `a`    | Be verbose about command-line arguments.
 * `i`    | Be verbose about include files.
 * `s`    | Be verbose about symbols referenced.
 *
 * Multiple formats may be given, one immediately after the other, e.g., `ai`.
 * Alternatively, `*` may be given to mean "all" or either the empty string or
 * `-` may be given to mean "none."
 * @endparblock
 * @return Returns the parsed value.
 */
NODISCARD
static tidy_verbose parse_tidy_verbose( char const *verbose_format ) {
  assert( verbose_format != NULL );

  set_all_or_none( &verbose_format, OPT_VERBOSE_ALL );
  tidy_verbose verbose = TIDY_VERBOSE_NONE;

    for ( char const *s = verbose_format; *s != '\0'; ++s ) {
    switch ( *s ) {
      case 'a':
        verbose |= TIDY_VERBOSE_ARGS;
        break;
      case 'i':
        verbose |= TIDY_VERBOSE_INCLUDES;
        break;
      case 's':
        verbose |= TIDY_VERBOSE_SYMBOLS;
        break;
      default:
        ;
    } // switch
  } // for

  return verbose;
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
 * Preprocess the command-line.
 *
 * @remarks
 * @parblock
 * The order that we have to parse command-line arguments is necessitated to
 * be unconventional.
 *
 * Ordinarily, a program would parse all options first, increment argv past
 * them, then look at (the new) argv[1] for the file; but the source file may
 * be needed before parsing options because its language (based on its filename
 * extension) affects the list of system include files and the corresponding
 * `-isystem` options needed by clang.
 *
 * We also have to pre-scan all options looking for clang's -x<language> option
 * because that has priority over whatever language is indicated by the source
 * file's extension.
 *
 * Finally, we have to call **clang** and insert `-isystem` options for the
 * include paths it would use to compile the source file.
 * @endparblock
 *
 * @param pargc A pointer to the argument count from \c main().
 * @param pargv A pointer to the argument values from \c main().
 */
static void preprocess_argv( int *pargc, char const **pargv[] ) {
  assert( pargc != NULL );
  assert( pargv != NULL );

  if ( *pargc < 2 || (*pargv)[ *pargc - 1 ][0] == '-' )
    return;
  tidy_source_path = (*pargv)[ *pargc - 1 ];

  char const *const clang_path = get_clang_path( *pargc, *pargv );
  char const *lang = get_x_language( *pargc, *pargv );
  if ( lang == NULL )
    lang = parse_file_ext( tidy_source_path );

  add_clang_include_paths( pargc, pargv, clang_path, lang );
  include_add_path( "." );
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

char const* include_resolve( char const *included_path ) {
  assert( included_path != NULL );
  assert( opt_include_paths != NULL );

  size_t      longest_include_path_len = 0;
  char const *shortest_include_path = included_path;

  for ( char **ppath = opt_include_paths; *ppath != NULL; ++ppath ) {
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
  ATEXIT( &options_cleanup );

  int               opt;
  bool              opt_help = false;
  bool              opt_version = false;
  char const *const short_opts = make_short_opts( OPTIONS, "I:" );
  int               tidy_argc;
  char const      **tidy_argv;

  preprocess_argv( pargc, pargv );
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
      case COPT(ALIGN):;
        opt_comment_align = parse_comment_alignment( optarg );
        break;
      case COPT(ALL_INCLUDES):
        opt_all_includes = true;
        break;
      case COPT(CLANG):                 // already handled
        break;
      case COPT(COMMENT_STYLE):
        if ( *SKIP_WS( optarg ) == '\0' )
          goto missing_arg;
        parse_comment_style( optarg );
        break;
      case COPT(CONFIG):
        if ( *SKIP_WS( optarg ) == '\0' )
          goto missing_arg;
        opt_config_path = optarg;
        break;
      case COPT(HELP):
        opt_help = true;
        break;
      case 'I':                         // special case
        if ( *SKIP_WS( optarg ) == '\0' )
          goto missing_arg;
        include_add_path( optarg );
        break;
      case COPT(LINE_LENGTH):
        opt_line_length = parse_line_length( optarg );
        break;
      case COPT(VERBOSE):
        if ( *SKIP_WS( optarg ) == '\0' )
          goto missing_arg;
        opt_verbose = parse_tidy_verbose( optarg );
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

  // argv[argc-1] is the source file, but we've already copied it into
  // tidy_source_path, so just NULL it out.
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
