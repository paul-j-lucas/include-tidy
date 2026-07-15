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

typedef struct getCursorByName_data getCursorByName_data;

////////// structs ////////////////////////////////////////////////////////////

/**
 * Additional data passed to getCursorByName_visitor.
 */
struct getCursorByName_data {
  char const *find_name;                ///< The name to find.
  CXCursor    found_cursor;             ///< The name's cursor, if found.
  CXCursor    skip_cursor;              ///< Skip this cursor.
  bool        cpp_recurse_into_scope;   ///< C++: recurse into scope?
};

////////// local functions ////////////////////////////////////////////////////

/**
 * Visits each symbol within a scope attempting to find one having \ref
 * getCursorByName_data::find_name "find_name".
 *
 * @param cursor The cursor being visited.
 * @param parent Not used.
 * @param data The getCursorByName_data to use.
 * @return Returns either `CXChildVisit_Break` or `CXChildVisit_Recurse`.
 */
static enum CXChildVisitResult getCursorByName_visitor( CXCursor cursor,
                                                        CXCursor parent,
                                                        CXClientData data ) {
  (void)parent;
  assert( data != NULL );

  getCursorByName_data *const gcbnd = data;

  if ( !clang_Cursor_isNull( gcbnd->skip_cursor ) &&
       clang_equalCursors( cursor, gcbnd->skip_cursor ) ) {
    goto skip;
  }

  CXString const    name_cxs = clang_getCursorSpelling( cursor );
  char const *const name = clang_getCString( name_cxs );
  bool const        found_name = strcmp( name, gcbnd->find_name ) == 0;

  clang_disposeString( name_cxs );

  if ( found_name ) {
    gcbnd->found_cursor = cursor;
    return CXChildVisit_Break;
  }

  enum CXCursorKind const kind = clang_getCursorKind( cursor );
  switch ( kind ) {
    case CXCursor_ClassDecl:
    case CXCursor_ClassTemplate:
    case CXCursor_EnumDecl:
    case CXCursor_Namespace:
    case CXCursor_StructDecl:
    case CXCursor_UnionDecl:
      if ( !gcbnd->cpp_recurse_into_scope )
        break;
      FALLTHROUGH;
    case CXCursor_CXXBaseSpecifier:
      return CXChildVisit_Recurse;
    default:
      /* suppress warning */;
  } // switch

skip:
  return CXChildVisit_Continue;
}

/**
 * Given a cursor at a local name of an enumeration, class, class data member,
 * class member function, structure, union, or namespace, gets the fully scoped
 * name skipping inline namespaces.
 *
 * @param cursor The cursor at a symbol.
 * @param sbuf The strbuf to use.
 */
static void getCursorScopedName_impl( CXCursor cursor, strbuf_t *sbuf ) {
  assert( sbuf != NULL );

  CXCursor parent_cursor = cursor;
  do {
    parent_cursor = clang_getCursorSemanticParent( parent_cursor );
  } while ( clang_Cursor_isInlineNamespace( parent_cursor ) );

  enum CXCursorKind const parent_kind = clang_getCursorKind( parent_cursor );

  if ( !clang_isInvalid( parent_kind ) &&
       parent_kind != CXCursor_TranslationUnit ) {
    getCursorScopedName_impl( parent_cursor, sbuf );
  }

  CXString const name_cxs = clang_getCursorSpelling( cursor );
  char const *const name = null_if_empty( clang_getCString( name_cxs ) );
  if ( name != NULL ) {
    if ( sbuf->len > 0 )
      strbuf_putsn( sbuf, "::", STRLITLEN( "::" ) );
    strbuf_puts( sbuf, name );
  }
  clang_disposeString( name_cxs );
}

/**
 * A helper function for tidy_Cursor_getFirstChild() that visits only the first
 * child cursor, if any, of a cursor.
 *
 * @param cursor The child cursor being visited.
 * @param parent Not used.
 * @param data A pointer to receive the \a cursor, the first child cursor.
 * @return Always returns `CXChildVisit_Break`.
 */
enum CXChildVisitResult getFirstChild_visitor( CXCursor cursor,
                                               CXCursor parent,
                                               CXClientData data ) {
  (void)parent;
  assert( data != NULL );

  CXCursor *const first_cursor = POINTER_CAST( CXCursor*, data );
  *first_cursor = cursor;
  return CXChildVisit_Break;
}

////////// extern functions ///////////////////////////////////////////////////

CXCursor tidy_Cursor_getFirstChild( CXCursor cursor ) {
  CXCursor first_cursor = clang_getNullCursor();
  clang_visitChildren( cursor, &getFirstChild_visitor, &first_cursor );
  return first_cursor;
}

CXCursor tidy_Cursor_getUnderlying( CXCursor cursor ) {
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

bool tidy_Cursor_isBeforeInTranslationUnit( CXCursor i_cursor,
                                            CXCursor j_cursor ) {
  if ( tidy_Cursor_isInvalid( i_cursor ) || tidy_Cursor_isInvalid( j_cursor ) )
    return false;
  CXSourceLocation const i_loc = clang_getCursorLocation( i_cursor );
  CXSourceLocation const j_loc = clang_getCursorLocation( j_cursor );
  return clang_isBeforeInTranslationUnit( i_loc, j_loc );
}

bool tidy_Cursor_isInvalid( CXCursor cursor ) {
  return clang_Cursor_isNull( cursor ) || clang_isInvalid( cursor.kind );
}

int tidy_File_CompareByName( CXFile i_file, CXFile j_file ) {
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

  getCursorByName_data gcbnd = {
    .find_name = name,
    .found_cursor = clang_getNullCursor(),
    .skip_cursor = clang_getNullCursor()
  };

  while ( !clang_Cursor_isNull( scope_cursor ) ) {
    clang_visitChildren( scope_cursor, &getCursorByName_visitor, &gcbnd );
    if ( !clang_Cursor_isNull( gcbnd.found_cursor ) )
      return gcbnd.found_cursor;
    if ( clang_getCursorKind( scope_cursor ) == CXCursor_TranslationUnit )
      break;

    CXCursor const parent = clang_getCursorSemanticParent( scope_cursor );
    if ( tidy_Cursor_isInvalid( parent ) ||
         clang_equalCursors( parent, scope_cursor ) ) {
      break;
    }

    gcbnd.cpp_recurse_into_scope = true;
    gcbnd.skip_cursor = scope_cursor;
    scope_cursor = parent;
  } // while

  return (CXCursor){ .kind = CXCursor_NoDeclFound };
}

CXCursor tidy_getCursorByNameToken( CXTranslationUnit tu, CXToken token,
                                    CXCursor scope_cursor ) {
  if ( clang_getTokenKind( token ) != CXToken_Identifier )
    return clang_getNullCursor();

  CXString const    token_cxs = clang_getTokenSpelling( tu, token );
  char const *const token_cs = clang_getCString( token_cxs );
  CXCursor const    rv_cursor = tidy_getCursorByName( token_cs, scope_cursor );

  clang_disposeString( token_cxs );
  return rv_cursor;
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

char const* tidy_getCursorScopedName( CXCursor cursor ) {
  strbuf_t sbuf;
  strbuf_init( &sbuf );
  getCursorScopedName_impl( cursor, &sbuf );
  return strbuf_take( &sbuf );
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

#ifdef HAVE_TYPEOF
    typedef typeof( ((CXFileUniqueID*)0)->data[0] ) CXFileUniqueID_data_t;
#else
    typedef unsigned long long CXFileUniqueID_data_t;
#endif /* HAVE_TYPEOF */

    id = (CXFileUniqueID){
      .data = {
#ifdef HAVE_UNSIGNED_INT128
        STATIC_CAST( CXFileUniqueID_data_t, hash >> 64 ),
#endif /* HAVE_UNSIGNED_INT128 */
        STATIC_CAST( CXFileUniqueID_data_t, hash )
      }
    };
  }
  return id;
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

extern inline int tidy_FileUniqueID_cmp( CXFileUniqueID const*,
                                         CXFileUniqueID const* );

/// @endcond

/* vim:set et sw=2 ts=2: */
