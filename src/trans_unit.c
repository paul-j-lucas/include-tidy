/*
**      include-tidy -- #include tidier
**      src/trans_unit.c
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
#include "trans_unit.h"
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

// extern variables
enum CXLanguageKind       tidy_lang;

////////// inline functions ///////////////////////////////////////////////////

/**
 * Returns an `"s"` or not based on \a n to pluralize a word.
 *
 * @param n A quantity.
 * @return Returns the empty string only if \a n == 1; otherwise returns `"s"`.
 */
NODISCARD
static inline char const* plural_s( unsigned long long n ) {
  return n == 1 ? "" : "s";
}

////////// local functions ////////////////////////////////////////////////////

/**
 * Prints translation unit errors, if any.
 */
static void print_diagnostics( void ) {
  unsigned const diag_count = clang_getNumDiagnostics( tidy_tu );
  if ( diag_count == 0 )
    return;

  unsigned const diag_opts = clang_defaultDiagnosticDisplayOptions();
  unsigned error_count = 0;

  for ( unsigned i = 0; i < diag_count; ++i ) {
    CXDiagnostic diag = clang_getDiagnostic( tidy_tu, i );
    switch ( clang_getDiagnosticSeverity( diag ) ) {
      case CXDiagnostic_Error:
      case CXDiagnostic_Fatal:
        ++error_count;
        CXString diag_cxs = clang_formatDiagnostic( diag, diag_opts );
        EPRINTF( "%s: %s\n", prog_name, clang_getCString( diag_cxs ) );
        clang_disposeString( diag_cxs );
        break;
      default:
        /* suppress warning */;
    } // switch
    clang_disposeDiagnostic( diag );
  } // for

  if ( error_count > 0 ) {
    fatal_error( EX_DATAERR,
      "%u error%s\n", error_count, plural_s( error_count )
    );
  }
}

/**
 * Cleans-up the translation unit.
 */
static void trans_unit_cleanup( void ) {
  if ( tidy_tu != NULL )
    clang_disposeTranslationUnit( tidy_tu );
  if ( tidy_index != NULL )
    clang_disposeIndex( tidy_index );
}

////////// extern functions ///////////////////////////////////////////////////

CXTranslationUnit trans_unit_init( int argc, char const *const argv[] ) {
  ASSERT_RUN_ONCE();
  assert( argc > 0 );

  ATEXIT( &trans_unit_cleanup );

  tidy_index = clang_createIndex(
    /*excludeDeclarationsFromPCH=*/false,
    /*displayDisgnostics=*/false
  );

  tidy_tu = clang_parseTranslationUnit(
    tidy_index, 
    arg_source_path,
    argv + 1, argc - 1,                 // skip argv[0] (program name)
    /*unsaved_files=*/NULL, 
    /*num_unsaved_files=*/0,
    CXTranslationUnit_DetailedPreprocessingRecord
  );

  if ( tidy_tu == NULL )
    fatal_error( EX_DATAERR, "error: failed to parse the translation unit\n" );
  print_diagnostics();

  CXCursor const cursor = clang_getTranslationUnitCursor( tidy_tu );
  tidy_lang = clang_getCursorLanguage( cursor );

  return tidy_tu;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
