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
#include "options.h"
#include "print.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>                     /* for exit(3) */
#include <sysexits.h>
#include <unistd.h>                     /* for access(2) */

/// @endcond

/**
 * @addtogroup tidy-trans-unit-group
 * @{
 */

////////// local variables ////////////////////////////////////////////////////

static CXIndex            tidy_index;   ///< Current libclang index.

////////// extern variables ///////////////////////////////////////////////////

/// @cond DOXYGEN_IGNORE
/// Otherwise Doxygen generates two entries.

enum CXLanguageKind       tidy_lang;
CXTranslationUnit         tidy_tu;

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
 * Cleans-up the translation unit.
 */
static void trans_unit_cleanup( void ) {
  if ( tidy_tu != NULL )
    clang_disposeTranslationUnit( tidy_tu );
  if ( tidy_index != NULL )
    clang_disposeIndex( tidy_index );
}

////////// extern functions ///////////////////////////////////////////////////

void trans_unit_check_for_errors( void ) {
  assert( tidy_tu != NULL );

  unsigned const diag_count = clang_getNumDiagnostics( tidy_tu );
  if ( diag_count == 0 )
    return;

  unsigned error_count = 0;

  for ( unsigned i = 0; i < diag_count; ++i ) {
    CXDiagnostic const diag = clang_getDiagnostic( tidy_tu, i );
    switch ( clang_getDiagnosticSeverity( diag ) ) {
      case CXDiagnostic_Error:
      case CXDiagnostic_Fatal:
        ++error_count;
        CXSourceLocation const diag_loc = clang_getDiagnosticLocation( diag );
        CXFile diag_file;
        unsigned diag_line, diag_col, diag_offset;
        clang_getSpellingLocation(
          diag_loc, &diag_file, &diag_line, &diag_col, &diag_offset
        );
        CXString const    diag_file_cxs = clang_getFileName( diag_file );
        char const *const diag_file_cs = clang_getCString( diag_file_cxs );
        CXString const    diag_msg_cxs = clang_getDiagnosticSpelling( diag );
        print_error(
          diag_file_cs, diag_line, diag_col,
          "%s\n", clang_getCString( diag_msg_cxs )
        );
        print_source_line( diag_file_cs, diag_line, diag_col, diag_offset );
        clang_disposeString( diag_msg_cxs );
        clang_disposeString( diag_file_cxs );
        break;
      default:
        /* suppress warning */;
    } // switch
    clang_disposeDiagnostic( diag );
  } // for

  if ( error_count > 0 ) {
    fatal_error( EX_DATAERR,
      "%u error%s generated\n", error_count, plural_s( error_count )
    );
  }
}

void trans_unit_init( int argc, char const *const argv[] ) {
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
    tidy_source_path,
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
    case CXError_InvalidArguments:
      fatal_error( EX_SOFTWARE, "invalid arguments given to libclang\n" );
    case CXError_Failure:
      //
      // Libclang isn't specific about the cause of a failure, so see if the
      // reason is because the source file doesn't exist or isn't readable.
      //
      if ( access( tidy_source_path, R_OK ) == -1 ) {
        print_error( tidy_source_path, 0, 0, "%s\n", STRERROR() );
        exit( EX_DATAERR );
      }
      break;
    case CXError_Success:
      //
      // All a CXError_Success means is that clang's parser didn't crash; it
      // doesn't mean the code is valid, so we have to check for errors later
      // via trans_unit_check_for_errors().
      //
      break;
  } // switch

  CXCursor const cursor = clang_getTranslationUnitCursor( tidy_tu );
  tidy_lang = clang_getCursorLanguage( cursor );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
