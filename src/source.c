/*
**      include-tidy -- #include tidier
**      src/source.c
**
**      Copyright (C) 2026  Paul J. Lucas, et al.
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
 * Defines functions for a source file being tidied.
 */

// local
#include "pjl_config.h"
#include "source.h"
#include "include-tidy.h"
#include "print.h"
#include "util.h"

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>                     /* for exit(3) */
#include <sysexits.h>
#include <unistd.h>                     /* for access(2) */

/**
 * @addtogroup tidy-source-group
 * @{
 */

////////// local variables ////////////////////////////////////////////////////

static CXIndex  tidy_index;             ///< Current libclang index.

////////// local functions ////////////////////////////////////////////////////

/**
 * Cleans-up libclang.
 */
static void libclang_cleanup( void ) {
  if ( tidy_index != NULL )
    clang_disposeIndex( tidy_index );
}

////////// extern functions ///////////////////////////////////////////////////

void source_check_for_errors( tidy_source const *source ) {
  assert( source != NULL );
  assert( source->tu != NULL );

  unsigned const diag_count = clang_getNumDiagnostics( source->tu );
  if ( diag_count == 0 )
    return;

  unsigned error_count = 0;

  for ( unsigned i = 0; i < diag_count; ++i ) {
    CXDiagnostic const diag = clang_getDiagnostic( source->tu, i );
    enum CXDiagnosticSeverity const sev = clang_getDiagnosticSeverity( diag );
    switch ( sev ) {
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
        char const *const diag_msg_cs = clang_getCString( diag_msg_cxs );

        if ( diag_file_cs != NULL ) {
          print_file_error(
            diag_file_cs, diag_line, diag_col, "%s\n", diag_msg_cs
          );
          print_source_line( diag_file_cs, diag_line, diag_col, diag_offset );
        }
        else {
          print_error_from( "libclang", "%s\n", diag_msg_cs );
        }

        clang_disposeString( diag_msg_cxs );
        clang_disposeString( diag_file_cxs );
        break;
      default:
        /* suppress warning */;
    } // switch
    clang_disposeDiagnostic( diag );
  } // for

  if ( error_count > 0 ) {
    EPRINTF(
      "%s: %u error%s generated\n",
      prog_name, error_count, plural_s( error_count )
    );
    exit( EX_DATAERR );
  }
}

void source_cleanup( tidy_source *source ) {
  if ( source != NULL ) {
    FREE( source->assoc_header_rel_path );
    // source->path points to an argv so it doesn't need freeing
    if ( source->tu != NULL )
      clang_disposeTranslationUnit( source->tu );
  }
};

void source_init( tidy_source *source, int argc, char const *const argv[] ) {
  assert( source != NULL );
  assert( argc > 0 );
  assert( argv != NULL );

  RUN_ONCE ATEXIT( &libclang_cleanup );

  tidy_index = clang_createIndex(
    /*excludeDeclarationsFromPCH=*/false,
    //
    // We only want errors printed and not warnings, so set this to false and
    // print errors ourselves.
    //
    /*displayDiagnostics=*/false
  );

  enum CXErrorCode const error_code = clang_parseTranslationUnit2(
    tidy_index,
    source->path,
    argv + 1, argc - 1,                 // skip argv[0] (program name)
    /*unsaved_files=*/NULL,
    /*num_unsaved_files=*/0,
    CXTranslationUnit_DetailedPreprocessingRecord,
    &source->tu
  );

  switch ( error_code ) {
    // LCOV_EXCL_START
    case CXError_ASTReadError:
      print_file_error( source->path, 0, 0, "libclang AST error\n" );
      exit( EX_UNAVAILABLE );
    case CXError_Crashed:
      print_file_error( source->path, 0, 0, "libclang crashed\n" );
      exit( EX_UNAVAILABLE );
    case CXError_InvalidArguments:
      print_error( "invalid arguments given to libclang\n" );
      exit( EX_SOFTWARE );
    // LCOV_EXCL_STOP
    case CXError_Failure:
      //
      // Libclang isn't specific about the cause of a failure, so see if the
      // reason is because the source file doesn't exist or isn't readable.
      //
      // Yes, this is TOCTAU (well, TAUTOC since we're checking after the
      // fact), but it's better than nothing.
      //
      if ( access( source->path, R_OK ) == -1 ) {
        print_file_error( source->path, 0, 0, "%s\n", STRERROR() );
        exit( EX_NOINPUT );
      }
      // LCOV_EXCL_START
      print_file_error( source->path, 0, 0, "libclang failed\n" );
      exit( EX_DATAERR );
      // LCOV_EXCL_STOP
    case CXError_Success:
      //
      // All a CXError_Success means is that libclang's parser didn't crash; it
      // doesn't mean the code is valid, so we have to check for errors later
      // via trans_unit_check_for_errors().
      //
      // We don't just check now because we have yet to parse the config file
      // to see if ignore-as-argument is true: if so, we must ignore the file
      // completely and not print any errors for it.
      //
      break;
  } // switch

  source->file = clang_getFile( source->tu, source->path );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
