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
#include "options.h"
#include "util.h"

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

/// @cond DOXYGEN_IGNORE
/// Otherwise Doxygen generates two entries.

// extern variable definitions
char const *prog_name;
char const *tidy_source_path;

/// @endcond

struct visitor_data {
  CXFile source_file;
};
typedef struct visitor_data visitor_data;

enum CXChildVisitResult visitor( CXCursor cursor, CXCursor parent,
                                 CXClientData client_data ) {
  (void)parent;
  assert( client_data != NULL );
  visitor_data const *const vd =
    POINTER_CAST( visitor_data const*, client_data );

  CXSourceLocation ref_loc = clang_getCursorLocation( cursor );
  CXFile ref_file;
  clang_getSpellingLocation( ref_loc, &ref_file, NULL, NULL, NULL );

  if ( !ref_file || !clang_File_isEqual( ref_file, vd->source_file ) )
    goto skip;

  enum CXCursorKind const kind = clang_getCursorKind( cursor );
  switch ( kind ) {
    case CXCursor_CallExpr:
    case CXCursor_DeclRefExpr:
    case CXCursor_MacroExpansion:;

      // Follow the reference to the actual declaration
      CXCursor decl = clang_getCursorReferenced( cursor );
      if ( clang_isInvalid( decl.kind ) )
        break;

      CXSourceLocation decl_loc = clang_getCursorLocation( decl );
      CXFile decl_file;
      unsigned decl_line;
      clang_getSpellingLocation( decl_loc, &decl_file, &decl_line, NULL, NULL );

      // 3. Check: Is the declaration file different from our main file?
      if ( !decl_file || clang_File_isEqual( decl_file, vd->source_file) )
        break;

      CXString symbol_name = clang_getCursorSpelling( decl );
      CXString file_name = clang_getFileName( decl_file );
      CXString kind_name = clang_getCursorKindSpelling( clang_getCursorKind( decl ) );

      printf( "%-15s | Type: %-12s | Declared In: %-15s (Line %u)\n",
              clang_getCString( symbol_name ),
              clang_getCString( kind_name ),
              clang_getCString( file_name ),
              decl_line );

      clang_disposeString( symbol_name );
      clang_disposeString( file_name );
      clang_disposeString( kind_name );
      break;

    default:
      break;
  } // switch

skip:
  return CXChildVisit_Recurse;
}

/**
 * The main entry point.
 *
 * @param argc The command-line argument count.
 * @param argv The command-line argument values.
 * @return Returns 0 on success, non-zero on failure.
 */
int main( int argc, char const *const argv[] ) {
  prog_name = base_name( argv[0] );
  options_init( argc, argv );

  CXIndex index = clang_createIndex( 0, 0 );

  // We need detailed preprocessing records to extract macro definitions
  char const *const args[] = { "-detailed-preprocessing-record" };

  CXTranslationUnit tu = clang_parseTranslationUnit(
    index, 
    tidy_source_path,
    args, 
    ARRAY_SIZE( args ), 
    /*unsaved_files=*/NULL, 
    /*num_unsaved_files=*/0,
    CXTranslationUnit_DetailedPreprocessingRecord
  );

  int rv = EX_OK;

  if ( tu == NULL )
    fatal_error( EX_DATAERR, "error: failed to parse the translation unit\n" );

  CXCursor cursor = clang_getTranslationUnitCursor( tu );
  visitor_data vd = { clang_getFile( tu, tidy_source_path ) };
  clang_visitChildren( cursor, visitor, &vd );

  clang_disposeTranslationUnit( tu );
  clang_disposeIndex( index );

  return rv;
}
