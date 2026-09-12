/*
**      include-tidy -- #include tidier
**      src/include-tidy.c
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
 * Defines `main()`, a function for exit status, and global variables.
 */

// local
#include "pjl_config.h"
#include "include-tidy.h"
#include "cli_options.h"
#include "color.h"
#include "config_file.h"
#include "include.h"
#include "options.h"
#include "path_util.h"
#include "proxies.h"
#include "symbol.h"
#include "trans_unit.h"
#include "util.h"

// system
#include <assert.h>
#include <stdlib.h>
#include <sysexits.h>

////////// enums //////////////////////////////////////////////////////////////

/**
 * **include-tidy**-specific exit status codes.
 */
enum {
  TIDY_EX_VIOLATIONS          = 1,      ///< One or more violations.
  TIDY_EX_NO_VIOLATIONS_ERROR = 2       ///< No violations, but error anyway.
};

////////// extern variables ///////////////////////////////////////////////////

/// @cond DOXYGEN_IGNORE
/// Otherwise Doxygen generates two entries.

char const   *prog_name;
tidy_test_t   tidy_test;

/// @endcond

////////// local functions ////////////////////////////////////////////////////

/**
 * Gets the status **include-tidy** should exit with.
 *
 * @return Returns said exit status.
 */
NODISCARD
static int tidy_status( void ) {
  if ( opt_error != TIDY_ERROR_NEVER ) {
    if ( tidy_includes_missing > 0 || tidy_includes_unnecessary > 0 )
      return TIDY_EX_VIOLATIONS;
    if ( opt_error == TIDY_ERROR_ALWAYS )
      return TIDY_EX_NO_VIOLATIONS_ERROR;
  }
  return EX_OK;
}

/**
 * Parses the value of the test environment variable.
 *
 * @param env_var
 * @parblock
 * The name of the environment variable containing an **include-tidy** test
 * format string (case sensitive) to parse.  Valid formats are:
 *
 * Format | Meaning
 * -------|-----------------------------------------------------------------
 * `e`    | Don't read files under `/etc/xdg/include-tidy` by default.
 * `h`    | Don't read files under the user's home directory by default.
 *
 * Multiple formats may be given, one immediately after the other, e.g., `eh`.
 * Alternatively, `*` may be given to mean "all" or either the empty string or
 * `-` may be given to mean "none."
 * @endparblock
 * @return Returns the parsed value.
 */
NODISCARD
static tidy_test_t tidy_test_parse( char const *env_var ) {
  assert( env_var != NULL );

  char const *value = null_if_empty( getenv( env_var ) );
  if ( value == NULL )
    return TIDY_TEST_NONE;              // LCOV_EXCL_LINE

  option_str_set_all_or_none( &value, "h" );
  tidy_test_t t = TIDY_TEST_NONE;

  for ( char const *s = value; *s != '\0'; ++s ) {
    switch ( *s ) {
      case 'e':
        t |= TIDY_TEST_NO_ETC_XDG;
        break;
      case 'h':
        t |= TIDY_TEST_NO_HOME;
        break;
      default:
        // LCOV_EXCL_START
        fatal_error( EX_USAGE,
          "\"%s\": invalid value for %s; must be [eh]|*|-\n",
          value, env_var
        );
        // LCOV_EXCL_STOP
    } // switch
  } // for

  return t;
}

////////// extern functions ///////////////////////////////////////////////////

/**
 * The main entry point.
 *
 * @param argc The command-line argument count.
 * @param argv The command-line argument values.
 * @return Returns 0 on success, non-zero on failure.
 */
int main( int argc, char const *argv[] ) {
  prog_name = path_basename( argv[0] );
  tidy_test = tidy_test_parse( "INCLUDE_TIDY_TEST" );

  // Initialization MUST happen in this order.
  cli_options_init( &argc, &argv );
  colors_init();
  trans_unit_init( argc, argv );
  includes_init();
  config_init();
  if ( !tidy_is_source_path_ignored ) {
    trans_unit_check_for_errors();
    implicit_proxies_init();
    symbols_init();
    includes_print();
  }
  return tidy_status();
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
