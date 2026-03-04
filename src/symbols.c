/*
**      include-tidy -- #include tidier
**      src/symbols.c
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
#include "util.h"

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

///////////////////////////////////////////////////////////////////////////////

struct symbol_visitor_data {
  CXFile source_file;
};
typedef struct symbol_visitor_data symbol_visitor_data;

/**
 * TODO.
 *
 * @param parent Not used.
 * @param client_data TODO.
 * @return Always returns `CXChildVisit_Recurse`.
 */
static enum CXChildVisitResult symbol_visitor( CXCursor cursor, CXCursor parent,
                                               CXClientData client_data ) {
  (void)parent;
  assert( client_data != NULL );
  symbol_visitor_data const *const svd =
    POINTER_CAST( symbol_visitor_data const*, client_data );

  CXSourceLocation ref_loc = clang_getCursorLocation( cursor );
  CXFile ref_file;
  clang_getSpellingLocation(
    ref_loc, &ref_file, /*line=*/NULL, /*column=*/NULL, /*offset=*/NULL
  );
  if ( !ref_file || !clang_File_isEqual( ref_file, svd->source_file ) )
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
      clang_getSpellingLocation(
        decl_loc, &decl_file, &decl_line, /*column=*/NULL, /*offset=*/NULL
      );

      // 3. Check: Is the declaration file different from our main file?
      if ( !decl_file || clang_File_isEqual( decl_file, svd->source_file) )
        break;

      CXString symbol_name = clang_getCursorSpelling( decl );
      CXString file_name = clang_getFileName( decl_file );

      printf( "%-15s | Declared In: %-15s (Line %u)\n",
              clang_getCString( symbol_name ),
              clang_getCString( file_name ),
              decl_line );

      clang_disposeString( symbol_name );
      clang_disposeString( file_name );
      break;

    default:
      break;
  } // switch

skip:
  return CXChildVisit_Recurse;
}

////////// extern functions ///////////////////////////////////////////////////

/**
 * TODO
 */
void symbols_init( CXTranslationUnit tu ) {
  ASSERT_RUN_ONCE();
  CXCursor cursor = clang_getTranslationUnitCursor( tu );
  symbol_visitor_data svd = { clang_getFile( tu, tidy_source_path ) };
  clang_visitChildren( cursor, &symbol_visitor, &svd );
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
