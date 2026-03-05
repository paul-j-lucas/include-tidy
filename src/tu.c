/*
**      include-tidy -- #include tidier
**      src/tu.c
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
#include "options.h"
#include "util.h"

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <sysexits.h>

// local variables
static CXIndex            tidy_index;
static CXTranslationUnit  tidy_tu;

////////// local functions ////////////////////////////////////////////////////

/**
 * Cleans-up the translation unit.
 */
static void tu_cleanup( void ) {
  clang_disposeTranslationUnit( tidy_tu );
  clang_disposeIndex( tidy_index );
}

////////// extern functions ///////////////////////////////////////////////////

/**
 * Initializes the translation unit by parsing the source file.
 *
 * @param argc The command-line argument count.
 * @param argv The command-line argument values.
 * @return Returns a new translation unit.
 */
CXTranslationUnit tu_new( int argc, char const *const argv[] ) {
  assert( argc > 0 );

  tidy_index = clang_createIndex( 0, 0 );
  tidy_tu = clang_parseTranslationUnit(
    tidy_index, 
    tidy_source_path,
    argv + 1, argc - 1,                 // skip argv[0] (program name)
    /*unsaved_files=*/NULL, 
    /*num_unsaved_files=*/0,
    CXTranslationUnit_DetailedPreprocessingRecord
  );

  unsigned const num_diagnostics = clang_getNumDiagnostics( tidy_tu );
  if ( num_diagnostics > 0 ) {
    unsigned const diag_opts = clang_defaultDiagnosticDisplayOptions();
    for ( unsigned i = 0; i < num_diagnostics; ++i ) {
      CXDiagnostic diag = clang_getDiagnostic( tidy_tu, i );
      CXString diag_string = clang_formatDiagnostic( diag, diag_opts );
      EPRINTF( "%s\n", clang_getCString( diag_string ) );
      clang_disposeString( diag_string );
      clang_disposeDiagnostic( diag );
    } // for
  }

  if ( tidy_tu == NULL )
    fatal_error( EX_DATAERR, "error: failed to parse the translation unit\n" );

  ATEXIT( &tu_cleanup );
  return tidy_tu;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
