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

// local
#include "pjl_config.h"
#include "include-tidy.h"
#include "cli_options.h"
#include "color.h"
#include "config_file.h"
#include "includes.h"
#include "options.h"
#include "symbols.h"
#include "trans_unit.h"
#include "util.h"

// libclang
#include <clang-c/Index.h>

// system
#include <sysexits.h>

////////// enumerations ///////////////////////////////////////////////////////

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

char const *prog_name;

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

////////// extern functions ///////////////////////////////////////////////////

/**
 * The main entry point.
 *
 * @param argc The command-line argument count.
 * @param argv The command-line argument values.
 * @return Returns 0 on success, non-zero on failure.
 */
int main( int argc, char const *argv[] ) {
  prog_name = base_name( argv[0] );

  // Initialization MUST happen in this order.
  options_init();
  cli_options_init( &argc, &argv );
  colors_init();
  CXTranslationUnit tu = trans_unit_init( argc, argv );
  includes_init( tu );
  config_init();
  associated_header_init();
  implicit_proxies_init( tu );
  symbols_init( tu );

  includes_print();
  return tidy_status();
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
