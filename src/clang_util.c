/*
**      include-tidy -- #include tidier
**      src/includes.c
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
 * Defines utility functions for libclang.
 */

// local
#include "pjl_config.h"
#include "clang_util.h"
#include "strbuf.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <stdbool.h>
#include <string.h>

/// @endcond

/**
 * @addtogroup clang-util-group
 * @{
 */

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct tidy_getCursorByName_data tidy_getCursorByName_data;

////////// structures /////////////////////////////////////////////////////////

/**
 * Additional data passed to getCursorByName_visitor.
 */
struct tidy_getCursorByName_data {
  char const *find_name;                ///< The name to find.
  CXCursor    found_cursor;             ///< The name's cursor, if found.
  CXCursor    skip_cursor;              ///< Skip this cursor.
};

////////// local functions ////////////////////////////////////////////////////

/**
 * Visits each symbol within a scope attempting to find one having \ref
 * tidy_getCursorByName_data::find_name "find_name".
 *
 * @param cursor The cursor being visited.
 * @param parent Not used.
 * @param data The tidy_getCursorByName_data to use.
 * @return Returns either `CXChildVisit_Break` or `CXChildVisit_Recurse`.
 */
static enum CXChildVisitResult getCursorByName_visitor( CXCursor cursor,
                                                        CXCursor parent,
                                                        CXClientData data ) {
  (void)parent;
  assert( data != NULL );

  enum CXChildVisitResult           rv = CXChildVisit_Continue;
  tidy_getCursorByName_data *const  tgcbnd = data;

  if ( clang_Cursor_isNull( tgcbnd->skip_cursor ) ||
       !clang_equalCursors( cursor, tgcbnd->skip_cursor ) ) {
    CXString const    name_cxs = clang_getCursorSpelling( cursor );
    char const *const name = clang_getCString( name_cxs );

    if ( name != NULL && strcmp( name, tgcbnd->find_name ) == 0 ) {
      tgcbnd->found_cursor = cursor;
      rv = CXChildVisit_Break;
    }

    clang_disposeString( name_cxs );
  }

  return rv;
}

/**
 * Given a cursor at a local name of an enumeration, class, class data member,
 * class member function, structure, union, or namespace, gets the fully scoped
 * name.
 *
 * @param cursor The cursor at a symbol.
 * @param sbuf The strbuf to use.
 */
static void getCursorScopedName_impl( CXCursor cursor, strbuf_t *sbuf ) {
  assert( sbuf != NULL );

  CXCursor const    parent_cursor = clang_getCursorSemanticParent( cursor );
  enum CXCursorKind parent_kind = clang_getCursorKind( parent_cursor );
  CXString          name_cxs;

  if ( !clang_isInvalid( parent_kind ) &&
       parent_kind != CXCursor_TranslationUnit ) {
    name_cxs = clang_getCursorSpelling( parent_cursor );
    bool const has_parent = clang_getCString( name_cxs ) != NULL;
    clang_disposeString( name_cxs );
    if ( has_parent ) {
      // Recurse all the way up to the outermost scope ...
      getCursorScopedName_impl( parent_cursor, sbuf );
      // ... then on the way back down, add the "::" ...
      strbuf_putsn( sbuf, "::", STRLITLEN( "::" ) );
    }
  }

  // ... followed by the scope name.
  name_cxs = clang_getCursorSpelling( cursor );
  strbuf_puts( sbuf, clang_getCString( name_cxs ) );
  clang_disposeString( name_cxs );
}

////////// extern functions ///////////////////////////////////////////////////

bool tidy_Cursor_isBeforeInTranslationUnit( CXCursor i_cursor,
                                            CXCursor j_cursor ) {
  if ( tidy_Cursor_isInvalid( i_cursor ) || tidy_Cursor_isInvalid( j_cursor ) )
    return false;
  CXSourceLocation const i_loc = clang_getCursorLocation( i_cursor );
  CXSourceLocation const j_loc = clang_getCursorLocation( j_cursor );
  return clang_isBeforeInTranslationUnit( i_loc, j_loc );
}

bool tidy_Cursor_isInFile( CXCursor cursor, CXFile file ) {
  assert( file != NULL );

  if ( tidy_Cursor_isInvalid( cursor ) )
    return false;
  CXFile const cursor_file = tidy_getCursorLocation_File( cursor );
  return cursor_file != NULL && clang_File_isEqual( cursor_file, file );
}

bool tidy_Cursor_isInvalid( CXCursor cursor ) {
  return clang_Cursor_isNull( cursor ) || clang_isInvalid( cursor.kind );
}

int tidy_File_cmp_by_name( CXFile i_file, CXFile j_file ) {
  assert( i_file != NULL );
  assert( j_file != NULL );

  CXString const    i_name_cxs = clang_getFileName( i_file );
  CXString const    j_name_cxs = clang_getFileName( j_file );
  char const *const i_name = clang_getCString( i_name_cxs );
  char const *const j_name = clang_getCString( j_name_cxs );
  int const         cmp = strcmp( i_name, j_name );

  clang_disposeString( i_name_cxs );
  clang_disposeString( j_name_cxs );
  return cmp;
}

CXString tidy_File_getRealPathName( CXFile file ) {
  assert( file != NULL );

  CXString          abs_path_cxs = clang_File_tryGetRealPathName( file );
  char const *const abs_path = clang_getCString( abs_path_cxs );

  if ( abs_path == NULL || abs_path[0] == '\0' ) {
    clang_disposeString( abs_path_cxs );
    abs_path_cxs = clang_getFileName( file );
  }

  return abs_path_cxs;
}

CXCursor tidy_getCursorByName( char const *name, CXCursor scope_cursor ) {
  assert( name != NULL );

  tidy_getCursorByName_data tgcbnd = {
    .find_name = name,
    .found_cursor = clang_getNullCursor(),
    .skip_cursor = clang_getNullCursor()
  };

  while ( !clang_Cursor_isNull( scope_cursor ) ) {
    clang_visitChildren( scope_cursor, &getCursorByName_visitor, &tgcbnd );
    if ( !clang_Cursor_isNull( tgcbnd.found_cursor ) )
      return tgcbnd.found_cursor;
    if ( clang_getCursorKind( scope_cursor ) == CXCursor_TranslationUnit )
      break;

    CXCursor const parent = clang_getCursorSemanticParent( scope_cursor );
    if ( tidy_Cursor_isInvalid( parent ) ||
         clang_equalCursors( parent, scope_cursor ) ) {
      break;
    }

    tgcbnd.skip_cursor = scope_cursor;
    scope_cursor = parent;
  } // while

  return clang_getNullCursor();
}

CXSourceRange tidy_getCursorExtent( CXCursor cursor ) {
  CXSourceRange const range = clang_getCursorExtent( cursor );
  CXSourceLocation    start_loc = clang_getRangeStart( range );
  CXSourceLocation    end_loc = clang_getRangeEnd( range );

  CXFile start_file, end_file;
  unsigned start_offset, end_offset;

  clang_getFileLocation(
    start_loc, &start_file, /*line=*/NULL, /*column=*/NULL, &start_offset
  );
  clang_getFileLocation(
    end_loc, &end_file, /*line=*/NULL, /*column=*/NULL, &end_offset
  );

  if ( start_file == NULL || start_file != end_file )
    return range;

  CXTranslationUnit const tu = clang_Cursor_getTranslationUnit( cursor );

  start_loc = clang_getLocationForOffset( tu, start_file, start_offset );
  end_loc   = clang_getLocationForOffset( tu, end_file, end_offset );

  return clang_getRange( start_loc, end_loc );
}

CXFile tidy_getCursorLocation_File( CXCursor cursor ) {
  CXSourceLocation const  loc = clang_getCursorLocation( cursor );
  CXFile                  file = tidy_getSpellingLocation_File( loc );

  if ( file == NULL ) {
    //
    // If tidy_getSpellingLocation_File() returns a NULL file, it can mean that
    // the symbol was formed via preprocessor token pasting, e.g., foo_ ## bar.
    // Fall back to tidy_getFileLocation_File().
    //
    file = tidy_getFileLocation_File( loc );
  }

  return file;
}

char* tidy_getCursorSpelling( CXCursor cursor ) {
  CXString cxs = clang_getCursorSpelling( cursor );
  char *const s = check_strdup( clang_getCString( cxs ) );
  clang_disposeString( cxs );
  return s;
}

CXFile tidy_getFileLocation_File( CXSourceLocation loc ) {
  CXFile file;
  clang_getFileLocation(
    loc, &file, /*line=*/NULL, /*column=*/NULL, /*offset=*/NULL
  );
  return file;
}

CXFileUniqueID tidy_getFileUniqueID( CXFile file ) {
  assert( file != NULL );

  CXFileUniqueID id;
  if ( unlikely( clang_getFileUniqueID( file, &id ) != 0 ) ) {
    // clang_getFileUniqueID() failed, but we still want an ID: get a hash of
    // its full path.
    CXString const    abs_path_cxs = tidy_File_getRealPathName( file );
    char const *const abs_path = clang_getCString( abs_path_cxs );
    fnv1a_t const     hash = fnv1a_s( abs_path );

    clang_disposeString( abs_path_cxs );
    id = (CXFileUniqueID){
      .data = {
#if HAVE_UNSIGNED_INT128
        STATIC_CAST( unsigned long long, hash >> 64 ),
#endif /* HAVE_UNSIGNED_INT128 */
        STATIC_CAST( unsigned long long, hash )
      }
    };
  }
  return id;
}

char const* tidy_getCursorScopedName( CXCursor cursor ) {
  strbuf_t sbuf;
  strbuf_init( &sbuf );
  getCursorScopedName_impl( cursor, &sbuf );
  return strbuf_take( &sbuf );
}

CXCursor tidy_getCursorUnderlying( CXCursor cursor ) {
  if ( clang_getCursorKind( cursor ) != CXCursor_TypeRef )
    return clang_getNullCursor();

  CXType type = clang_getCanonicalType( clang_getCursorType( cursor ) );
  for (;;) {
    switch ( type.kind ) {
      case CXType_Pointer:
      case CXType_LValueReference:
      case CXType_RValueReference:
        type = clang_getPointeeType( type );
        break;
      default:
        return clang_getTypeDeclaration( type );
    } // switch
  } // for
}

CXFile tidy_getSpellingLocation_File( CXSourceLocation loc ) {
  CXFile file;
  clang_getSpellingLocation(
    loc, &file, /*line=*/NULL, /*column=*/NULL, /*offset=*/NULL
  );
  return file;
}

bool tidy_Token_isEqual( CXTranslationUnit tu, CXToken token,
                         char const *value ) {
  assert( value != NULL );

  CXString const    token_cxs = clang_getTokenSpelling( tu, token );
  char const *const token_cs = clang_getCString( token_cxs );
  int const         cmp = strcmp( token_cs, value );

  clang_disposeString( token_cxs );
  return cmp == 0;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/// @cond DOXYGEN_IGNORE

extern inline int tidy_CXFileUniqueID_cmp( CXFileUniqueID const*,
                                           CXFileUniqueID const* );

/// @endcond

/* vim:set et sw=2 ts=2: */
