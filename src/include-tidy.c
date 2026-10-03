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
#include "source.h"
#include "str_util.h"
#include "symbol.h"
#include "trans_unit.h"
#include "util.h"

// system
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

/// @endcond

////////// local functions ////////////////////////////////////////////////////

/**
 * Initializes testing.
 *
 * @param env_var The name of the environment variable containing a test format
 * string (case sensitive) to parse.
 */
void test_init( char const *env_var ) {
  char const *const value = empty_if_null( getenv( env_var ) );
  if ( !opt_test_parse( value ) ) {
    // LCOV_EXCL_START
    fatal_error( EX_USAGE,
      "\"%s\": invalid value for %s; must be [" OPT_TEST_ALL "]|*|-\n",
      value, env_var
    );
    // LCOV_EXCL_STOP
  }
}

/**
 * Gets the status **include-tidy** should exit with.
 *
 * @param source The source file being tidied.
 * @return Returns said exit status.
 */
NODISCARD
static int tidy_status( tidy_source const *source ) {
  if ( opt_error != TIDY_ERROR_NEVER ) {
    if ( source->includes_missing > 0 || source->includes_unnecessary > 0 )
      return TIDY_EX_VIOLATIONS;
    if ( opt_error == TIDY_ERROR_ALWAYS )
      return TIDY_EX_NO_VIOLATIONS_ERROR;
  }
  return EX_OK;
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

  // Initialization MUST happen in this order.
  test_init( "INCLUDE_TIDY_TEST" );
  tidy_source source = cli_options_init( &argc, &argv );
  colors_init();
  trans_unit_init( source.path, argc, argv );
  includes_init( &source );
  config_init( &source );

  if ( !source.is_ignored ) {
    trans_unit_check_for_errors();
    implicit_proxies_init( &source );
    symbols_init( &source );
    includes_print( &source );
  }

  int const status = tidy_status( &source );
  tidy_source_cleanup( &source );
  return status;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
