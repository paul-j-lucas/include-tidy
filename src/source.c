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
#include <stddef.h>
#include <stdlib.h>                     /* for exit(3) */
#include <sysexits.h>

/**
 * @addtogroup tidy-source-group
 * @{
 */

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

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
