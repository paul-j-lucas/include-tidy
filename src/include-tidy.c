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
#include "config_file.h"
#include "includes.h"
#include "options.h"
#include "symbols.h"
#include "trans_unit.h"
#include "util.h"

// system
#include <sysexits.h>

// libclang
#include <clang-c/Index.h>

/**
 * **include-tidy**-specific exit status codes.
 */
enum {
  TIDY_EX_VIOLATIONS  = 1,              ///< One or more violations.
  TIDY_EX_ERROR       = 2               ///< `--error` or `-e` was given.
};

/// @cond DOXYGEN_IGNORE
/// Otherwise Doxygen generates two entries.

// extern variable definitions
char const *prog_name;

/// @endcond

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
  options_init();
  cli_options_init( &argc, &argv );
  CXTranslationUnit tu = trans_unit_init( argc, argv );
  includes_init( tu );
  config_init();
  if ( (opt_verbose & TIDY_VERBOSE_CONFIG_PROXIES) != 0 )
    include_proxies_dump();
  symbols_init( tu );
  includes_print();
  if ( tidy_includes_missing > 0 || tidy_includes_unnecessary > 0 )
    return TIDY_EX_VIOLATIONS;
  return opt_error ? TIDY_EX_ERROR : EX_OK;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
