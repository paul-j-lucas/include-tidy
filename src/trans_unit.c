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

/**
 * @file
 * Defines variables and functions for the translation unit.
 */

// local
#include "pjl_config.h"
#include "trans_unit.h"
#include "include-tidy.h"
#include "options.h"
#include "print.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

// libclang
#include <clang-c/Index.h>

/// @endcond

/**
 * @addtogroup tidy-trans-unit-group
 * @{
 */

// local variables
static CXIndex            tidy_index;   ///< Current libclang index.
static CXTranslationUnit  tidy_tu;      ///< Current libclang translation unit.

/// @cond DOXYGEN_IGNORE
/// Otherwise Doxygen generates two entries.

// extern variables
enum CXLanguageKind       tidy_lang;

/// @endcond

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
 *
 * @param tu The CXTranslationUnit to use.
 */
static void print_diagnostics( CXTranslationUnit tu ) {
  assert( tu != NULL );

  unsigned const diag_count = clang_getNumDiagnostics( tu );
  if ( diag_count == 0 )
    return;

  unsigned const diag_opts = clang_defaultDiagnosticDisplayOptions();
  unsigned error_count = 0;

  for ( unsigned i = 0; i < diag_count; ++i ) {
    CXDiagnostic diag = clang_getDiagnostic( tu, i );
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

/**
 * Handles a translation unit CXError_Failure.
 *
 * @param tu The CXTranslationUnit to use.  May be NULL.
 */
_Noreturn
static void trans_unit_failure( CXTranslationUnit tu ) {
  if ( tu == NULL ) {
    // libclang isn't specific enough about a failure, so see if the reason is
    // because the source file doesn't exist or isn't readable.
    FILE *const file = fopen( arg_source_path, "r" );
    if ( file == NULL ) {
      print_error( arg_source_path, 0, 0, "%s\n", STRERROR() );
    }
    else {
      fclose( file );
      print_error( arg_source_path, 0, 0, "failed to parse\n" );
    }
    exit( EX_DATAERR );
  }

  print_diagnostics( tu );

  // This function is called only upon failure, so we should have caught the
  // failure above.
  assert( false && "should have caught the failure" );
}

////////// extern functions ///////////////////////////////////////////////////

CXTranslationUnit trans_unit_init( int argc, char const *const argv[] ) {
  ASSERT_RUN_ONCE();
  assert( argc > 0 );
  assert( argv != NULL );

  ATEXIT( &trans_unit_cleanup );

  tidy_index = clang_createIndex(
    /*excludeDeclarationsFromPCH=*/false,
    /*displayDisgnostics=*/false
  );

  enum CXErrorCode const error_code = clang_parseTranslationUnit2(
    tidy_index,
    arg_source_path,
    argv + 1, argc - 1,                 // skip argv[0] (program name)
    /*unsaved_files=*/NULL, 
    /*num_unsaved_files=*/0,
    CXTranslationUnit_DetailedPreprocessingRecord,
    &tidy_tu
  );

  switch ( error_code ) {
    case CXError_ASTReadError:
      fatal_error( EX_UNAVAILABLE, "libclang AST error\n" );
    case CXError_Crashed:
      fatal_error( EX_UNAVAILABLE, "libclang crashed\n" );
    case CXError_Failure:
      trans_unit_failure( tidy_tu );
    case CXError_InvalidArguments:
      fatal_error( EX_SOFTWARE, "invalid arguments given to libclang\n" );
    case CXError_Success:
      break;
  } // switch

  CXCursor const cursor = clang_getTranslationUnitCursor( tidy_tu );
  tidy_lang = clang_getCursorLanguage( cursor );

  return tidy_tu;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
