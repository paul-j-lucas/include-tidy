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
  visitor_data const *const data = (visitor_data const*)client_data;

  enum CXCursorKind const kind = clang_getCursorKind( cursor );

  if ( !(clang_isDeclaration( kind ) || kind == CXCursor_MacroDefinition) )
    goto skip_kind;

  CXString name = clang_getCursorSpelling( cursor );
  char const *const name_cstr = clang_getCString( name );

  if ( name_cstr == NULL || name_cstr[0] == '\0' )
    goto skip_symbol;

  CXSourceLocation loc = clang_getCursorLocation( cursor );

  CXFile file;
  unsigned line, column, offset;
  clang_getSpellingLocation( loc, &file, &line, &column, &offset );
  if ( !clang_File_isEqual( file, data->source_file ) )
    return CXChildVisit_Continue;

  CXString cxFileName;
  char const *file_name = "unknown";

  if ( file ) {
    cxFileName = clang_getFileName( file );
    file_name = clang_getCString( cxFileName );
  }

  printf( "%s -> %s:%u\n", name_cstr, file_name, line );

  if ( file )
    clang_disposeString( cxFileName );

skip_symbol:
  clang_disposeString( name );

skip_kind:
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

  if ( tu == NULL ) {
    fprintf( stderr,
      "%s: error: failed to parse the translation unit\n",
      prog_name
    );
    rv = EX_DATAERR;
    goto error;
  }

  CXCursor cursor = clang_getTranslationUnitCursor( tu );
  visitor_data data = { clang_getFile( tu, tidy_source_path ) };
  clang_visitChildren( cursor, visitor, &data );

  clang_disposeTranslationUnit( tu );

error:
  clang_disposeIndex( index );
  return rv;
}
