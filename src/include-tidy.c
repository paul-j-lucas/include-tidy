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
#include "config_file.h"
#include "includes.h"
#include "options.h"
#include "symbols.h"
#include "util.h"

// libclang
#include <clang-c/Index.h>

// standard
#include <sysexits.h>

/// @cond DOXYGEN_IGNORE
/// Otherwise Doxygen generates two entries.

// extern variable definitions
char const *prog_name;

/// @endcond

void              cli_options_init( int*, char const **[] );
CXTranslationUnit tu_new( int, char const *const[] );

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
  CXTranslationUnit tu = tu_new( argc, argv );
  includes_init( tu );
  config_init( tu );
  config_resolve_includes();
  symbols_init( tu );
  includes_print();
  return EX_OK;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
