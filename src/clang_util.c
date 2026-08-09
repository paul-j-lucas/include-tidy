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
#include <stdint.h>
#include <string.h>

/// @endcond

/**
 * @addtogroup clang-util-group
 * @{
 */

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct      getCursorByName_data getCursorByName_data;
typedef CXString  (*getCursorName_fn)( CXCursor );
typedef struct      isBaseClass_data     isBaseClass_data;

////////// structs ////////////////////////////////////////////////////////////

/**
 * Additional data passed to getCursorByName_visitor().
 */
struct getCursorByName_data {
  char const *find_name;                ///< The name to find.
  CXCursor    found_csr;                ///< The name's cursor, if found.
  CXCursor    skip_csr;                 ///< Skip this cursor.
  bool        cxx_recurse_into_scope;   ///< C++: recurse into scope?
};

/**
 * Additional data passed to isBaseClass_visitor().
 */
struct isBaseClass_data {
  /**
   * A candidate base class.
   *
   * @note This _must_ be set to a canonical cursor.
   */
  CXCursor base_csr;

  /**
   * Set to `true` only if \ref base_csr is a base class of a given cursor.
   */
  bool is_base;
};

////////// local functions ////////////////////////////////////////////////////

NODISCARD
static CXCursor tidy_Cursor_skipUnexposedDown( CXCursor );

////////// local functions ////////////////////////////////////////////////////

/**
 * A helper function for tidy_getCursorByName() that Visits each symbol within
 * a scope attempting to find one having \ref getCursorByName_data::find_name
 * "find_name".
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

  if ( !clang_Cursor_isNull( gcbnd->skip_csr ) &&
       clang_equalCursors( cursor, gcbnd->skip_csr ) ) {
    goto skip;
  }

  CXString const    name_cxs = clang_getCursorSpelling( cursor );
  char const *const name_cs = clang_getCString( name_cxs );
  bool const        is_found = strcmp( name_cs, gcbnd->find_name ) == 0;

  clang_disposeString( name_cxs );

  if ( is_found ) {
    gcbnd->found_csr = cursor;
    return CXChildVisit_Break;
  }

  enum CXCursorKind const kind = clang_getCursorKind( cursor );
  switch ( kind ) {
    case CXCursor_CXXBaseSpecifier:;
      CXCursor base_csr = clang_getCursorReferenced( cursor );
      if ( tidy_Cursor_isInvalid( base_csr ) )
        base_csr = clang_getTypeDeclaration( clang_getCursorType( cursor ) );
      if ( !tidy_Cursor_isInvalid( base_csr ) )
        clang_visitChildren( base_csr, &getCursorByName_visitor, data );
      break;

    case CXCursor_Namespace:
      if ( gcbnd->cxx_recurse_into_scope )
        return CXChildVisit_Recurse;
      break;

    default:
      /* suppress warning */;
  } // switch

skip:
  return CXChildVisit_Continue;
}

/**
 * A helper function for tidy_Cursor_getFirstChild() that visits only the first
 * child cursor, if any, of a cursor.
 *
 * @param cursor The cursor being visited.
 * @param parent Not used.
 * @param data A pointer to receive the first child cursor.
 * @return Always returns `CXChildVisit_Break`.
 *
 * @sa getFirstExprChild_visitor()
 */
static enum CXChildVisitResult getFirstChild_visitor( CXCursor cursor,
                                                      CXCursor parent,
                                                      CXClientData data ) {
  (void)parent;
  assert( data != NULL );

  CXCursor *const first_csr = data;
  *first_csr = cursor;
  return CXChildVisit_Break;
}

/**
 * A helper function for tidy_Cursor_getFirstExprChild() that visits only the
 * first expression child cursor, if any, of a cursor.
 *
 * @param cursor The cursor being visited.
 * @param parent The parent of \a cursor.
 * @param data A pointer to receive the first expression child cursor.
 * @return Returns `CXChildVisit_Break` if \a cursor is an expression or
 * `CXChildVisit_Continue` otherwise.
 *
 * @sa getFirstChild_visitor()
 */
static enum CXChildVisitResult getFirstExprChild_visitor( CXCursor cursor,
                                                          CXCursor parent,
                                                          CXClientData data ) {
  (void)parent;

  enum CXCursorKind const kind = clang_getCursorKind( cursor );
  if ( clang_isExpression( kind ) ) {
    assert( data != NULL );
    CXCursor *const expr_csr = data;
    *expr_csr = cursor;
    return CXChildVisit_Break;
  }
  return CXChildVisit_Continue;
}

/**
 * Given a cursor at a local name of an enumeration, class, class data member,
 * class member function, structure, union, or namespace, gets the fully scoped
 * name skipping inline namespaces.
 *
 * @param cursor The cursor at a symbol.
 * @param name_fn The libclang function to use to get the name of \a cursor.
 * @param sbuf The strbuf to use.
 */
static void getScopedName_impl( CXCursor cursor, getCursorName_fn name_fn,
                                strbuf_t *sbuf ) {
  assert( name_fn != NULL );
  assert( sbuf != NULL );

  CXCursor sem_parent = cursor;
  do {
    sem_parent = clang_getCursorSemanticParent( sem_parent );
  } while ( clang_Cursor_isInlineNamespace( sem_parent ) );

  if ( tidy_Cursor_isScopeDecl( sem_parent ) )
    getScopedName_impl( sem_parent, name_fn, sbuf );

  CXString const name_cxs = (*name_fn)( cursor );
  char const *const name = null_if_empty( clang_getCString( name_cxs ) );
  // Skip "(anonymous ...)" and "(unnamed ...)".
  if ( name != NULL && name[0] != '(' ) {
    if ( sbuf->len > 0 )
      strbuf_putsn( sbuf, "::", STRLITLEN( "::" ) );
    // Don't include function signatures.
    char const *const lparen = strchr_nul( name, '(' );
    size_t name_len = STATIC_CAST( size_t, lparen - name );
    // Don't include empty "<>".
    if ( name_len >= 2 && STRNCMPLIT( name + name_len - 2, "<>" ) == 0 )
      name_len -= 2;
    strbuf_putsn( sbuf, name, name_len );
  }
  clang_disposeString( name_cxs );
}

/**
 * Helper function for both tidy_Cursor_getScopedDisplayName() and
 * tidy_Cursor_getScopedSimpleName().
 *
 * @param cursor The cursor at a symbol.
 * @param name_fn The libclang function to use to get the name of \a cursor.
 * @return Returns the fully scoped name.  The caller is responsible for
 * freeing it.
 */
static char* getScopedName_thunk( CXCursor cursor, getCursorName_fn name_fn ) {
  assert( name_fn != NULL );

  CXCursor const ref_csr = clang_getCursorReferenced( cursor );
  if ( !tidy_Cursor_isInvalid( ref_csr ) )
    cursor = ref_csr;

  strbuf_t sbuf;
  strbuf_init( &sbuf );
  getScopedName_impl( cursor, name_fn, &sbuf );
  return strbuf_take( &sbuf );
}

/**
 * Helper function for tidy_Cursor_getTypeRef() that visits the children of \a
 * cursor looking for a TypeRef to a class.
 *
 * @param cursor The cursor being visited.
 * @param parent Not used.
 * @param data A pointer to receive the type cursor.
 * @return Returns either `CXChildVisit_Break` or `CXChildVisit_Recurse`.
 */
static enum CXChildVisitResult getTypeRef_visitor( CXCursor cursor,
                                                   CXCursor parent,
                                                   CXClientData data ) {
  (void)parent;
  assert( data != NULL );

  enum CXCursorKind const kind = clang_getCursorKind( cursor );
  switch ( kind ) {
    case CXCursor_TemplateRef:
    case CXCursor_TypeRef:;
      CXCursor const ref_csr = clang_getCursorReferenced( cursor );
      if ( tidy_Cursor_isClassDecl( ref_csr ) ) {
        CXCursor *const type_ref_csr = data;
        *type_ref_csr = ref_csr;
        return CXChildVisit_Break;
      }
      break;
    default:
      /* suppress warning */;
  } // switch

  return CXChildVisit_Continue;
}

/**
 * A helper function for tidy_Cursor_isInheritedFrom() that visits the children
 * of a class looking for a base class.
 *
 * @param cursor The cursor being visited.
 * @param parent Not used.
 * @param data The isBaseClass_data to use.
 * @return Returns `CXChildVisit_Break` only if a base class is found;
 * otherwise returns `CXChildVisit_Continue`.
 */
static enum CXChildVisitResult isBaseClass_visitor( CXCursor cursor,
                                                    CXCursor parent,
                                                    CXClientData data ) {
  (void)parent;

  enum CXCursorKind const kind = clang_getCursorKind( cursor );
  if ( kind == CXCursor_CXXBaseSpecifier ) {
    CXCursor ref_csr = clang_getCursorReferenced( cursor );
    if ( tidy_Cursor_isInvalid( ref_csr ) ) {
      CXType const type = clang_getCursorType( cursor );
      ref_csr = clang_getTypeDeclaration( type );
    }
    ref_csr = clang_getCanonicalCursor( ref_csr );

    assert( data != NULL );
    isBaseClass_data *const ibcd = data;

    if ( clang_equalCursors( ref_csr, ibcd->base_csr ) ||
         tidy_Cursor_isTemplateSpecializationOf( ibcd->base_csr, ref_csr ) ||
         tidy_Cursor_isInheritedFrom( ref_csr, ibcd->base_csr ) ) {
      ibcd->is_base = true;
      return CXChildVisit_Break;
    }
  }

  return CXChildVisit_Continue;
}

/**
 * Gets the first expression child cursor of \a cursor, if any.
 *
 * @param cursor The cursor to get the first expression child cursor of, if
 * any.
 * @return Returns the first child expression cursor of \a cursor, or the null
 * cursor if none.
 *
 * @sa tidy_Cursor_getFirstChild()
 */
static CXCursor tidy_Cursor_getFirstExprChild( CXCursor cursor ) {
  CXCursor child_csr = clang_getNullCursor();
  clang_visitChildren( cursor, &getFirstExprChild_visitor, &child_csr );
  return child_csr;
}

/**
 * Gets the TypeRef child cursor of \a cursor, if any.
 *
 * @param cursor The cursor to get the TypeRef child cursor of.
 * @return Returns the TypeRef child cursor of \a cursor or the null cursor if
 * none.
 */
NODISCARD
static CXCursor tidy_Cursor_getTypeRef( CXCursor cursor ) {
  CXCursor type_csr = clang_getNullCursor();
  clang_visitChildren( cursor, &getTypeRef_visitor, &type_csr );
  return type_csr;
}

/**
 * Gets the root initializer for a variable.
 *
 * @par Example
 * @parblock
 * Given:
 *
 *      int a = 42;
 *      int b = a;
 *      int c = b;
 *
 * If passed the cursor for the DeclRefExpr for `b`, will return `42`.
 * @endparblock
 *
 * @param expr_csr The cursor for the initializing expression.
 * @return Returns the root initializer expression or \a expr_csr (without
 * UnexposedExpr) if no other initializer expressions exist.
 */
NODISCARD
static CXCursor tidy_Cursor_getVarInitializer( CXCursor expr_csr ) {
  expr_csr = tidy_Cursor_skipUnexposedDown( expr_csr );
  enum CXCursorKind kind = clang_getCursorKind( expr_csr );
  if ( kind == CXCursor_DeclRefExpr ) {
    CXCursor const ref_csr = clang_getCursorReferenced( expr_csr );
    kind = clang_getCursorKind( ref_csr );
    if ( kind == CXCursor_VarDecl ) {
      CXCursor const init_csr = tidy_Cursor_getFirstExprChild( ref_csr );
      if ( !clang_Cursor_isNull( init_csr ) )
        return tidy_Cursor_getVarInitializer( init_csr );
    }
  }
  return expr_csr;
}

/**
 * Gets whether the kind of \a cursor can be inherited from a base class into a
 * derived class.
 *
 * @param cursor The cursor to check.
 * @return Returns `true` only if \a cursor is inheritable.
 */
NODISCARD
static bool tidy_Cursor_isInheritable( CXCursor cursor ) {
  enum CXCursorKind const kind = clang_getCursorKind( cursor );
  switch ( kind ) {
    // data members
    case CXCursor_FieldDecl:
    case CXCursor_VarDecl:
    // member functions
    case CXCursor_CXXMethod:
    case CXCursor_ConversionFunction:
    case CXCursor_FunctionTemplate:
    // types
    case CXCursor_TypeAliasDecl:
    case CXCursor_TypedefDecl:
      return true;
    default:
      return false;
  } // switch
}

/**
 * If \a cursor is a LinkageSpec, gets its parent cursor, recursively.
 *
 * @remarks Linkage specifications are just noise when determining include file
 * dependencies.
 *
 * @param cursor The cursor.
 * @return If \a cursor is a LinkageSpec, returns its parent; otherwise returns
 * \a cursor.
 */
NODISCARD
static CXCursor tidy_Cursor_skipLinkageSpec( CXCursor cursor ) {
  while ( clang_getCursorKind( cursor ) == CXCursor_LinkageSpec )
    cursor = clang_getCursorSemanticParent( cursor );
  return cursor;
}

/**
 * If \a cursor is an UnexposedExpr, gets its first child cursor.
 *
 * @param cursor The cursor.
 * @return If \a cursor is an UnexposedExpr, returns its first exposed child;
 * otherwise returns \a cursor.
 *
 * @sa tidy_Cursor_getFirstExposedChild()
 */
NODISCARD
static CXCursor tidy_Cursor_skipUnexposedDown( CXCursor cursor ) {
  while ( clang_isUnexposed( clang_getCursorKind( cursor ) ) )
    cursor = tidy_Cursor_getFirstChild( cursor );
  return cursor;
}

/**
 * Similar to `clang_getCursorReferenced()` except returns \a cursor as-is if
 * it's not a reference rather than the null cursor.
 *
 * @param cursor The cursor to get the reference of.
 * @param pkind If not NULL, receives the (referenced) kind of cursor.
 * @return Returns the cursor referenced by \a cursor or \a cursor if it's not
 * a reference.
 */
NODISCARD
static CXCursor tidy_getCursorReferenced( CXCursor cursor,
                                          enum CXCursorKind *pkind ) {
  enum CXCursorKind kind = clang_getCursorKind( cursor );
  switch ( kind ) {
    case CXCursor_DeclRefExpr:
    case CXCursor_MemberRefExpr:
    case CXCursor_NamespaceRef:
    case CXCursor_OverloadedDeclRef:
    case CXCursor_TemplateRef:
    case CXCursor_TypeRef:
      cursor = clang_getCursorReferenced( cursor );
      kind = clang_getCursorKind( cursor );
      break;
    default:
      /* suppress warning */;
  } // switch

  if ( pkind != NULL )
    *pkind = kind;
  return cursor;
}

////////// extern functions ///////////////////////////////////////////////////

int tidy_Cursor_compare( CXCursor i_csr, CXCursor j_csr ) {
  if ( i_csr.kind < j_csr.kind )
    return -1;
  if ( i_csr.kind > j_csr.kind )
    return 1;

  if ( i_csr.xdata < j_csr.xdata )
    return -1;
  if ( i_csr.xdata > j_csr.xdata )
    return 1;

  // See <https://github.com/llvm/llvm-project/blob/4f5675a0500f9ccc60dcbabb57e1c4dc88c40a84/clang/tools/libclang/CIndex.cpp#L6706>.
  if ( clang_isDeclaration( i_csr.kind ) )
    i_csr.data[1] = j_csr.data[1] = NULL;

  for ( unsigned i = 0; i < ARRAY_SIZE( ((CXCursor*)0)->data ); ++i ) {
    uintptr_t const i_uint = STATIC_CAST( uintptr_t, i_csr.data[i] );
    uintptr_t const j_uint = STATIC_CAST( uintptr_t, j_csr.data[i] );
    if ( i_uint < j_uint )
      return -1;
    if ( i_uint > j_uint )
      return 1;
  } // for

  return 0;
}

CXCursor tidy_Cursor_getCanonicalTypeDeclaration( CXCursor cursor ) {
  CXType const type = clang_getCanonicalType( clang_getCursorType( cursor ) );
  return clang_getTypeDeclaration( type );
}

CXCursor tidy_Cursor_getClassAsWritten( CXCursor cursor ) {
  cursor = tidy_Cursor_skipUnexposedDown( cursor );
  CXCursor const ref_csr = clang_getCursorReferenced( cursor );
  if ( !clang_Cursor_isNull( ref_csr ) ) {
    CXCursor const type_csr = tidy_Cursor_getTypeRef( ref_csr );
    if ( !clang_Cursor_isNull( type_csr ) )
      return type_csr;
  }
  return tidy_Cursor_getUnderlyingType( cursor );
}

CXCursor tidy_Cursor_getFirstChild( CXCursor cursor ) {
  CXCursor child_csr = clang_getNullCursor();
  clang_visitChildren( cursor, &getFirstChild_visitor, &child_csr );
  return child_csr;
}

CXCursor tidy_Cursor_getFirstExposedChild( CXCursor cursor ) {
  return tidy_Cursor_skipUnexposedDown( tidy_Cursor_getFirstChild( cursor ) );
}

CXCursor tidy_Cursor_getFunctionScope( CXCursor fn_csr ) {
  CXCursor const sem_parent = clang_getCursorSemanticParent( fn_csr );

  // If it's a member function, return its class directly.
  if ( tidy_Cursor_isClassDecl( sem_parent ) )
    return sem_parent;

  // For a non-member function or operator, inspect parameter types for an
  // associated class.
  int const num_args = clang_Cursor_getNumArguments( fn_csr );
  assert( num_args >= 0 && "fn_csr is not a function" );
  for ( unsigned i = 0; i < STATIC_CAST( unsigned, num_args ); ++i ) {
    CXCursor arg_csr = clang_Cursor_getArgument( fn_csr, i );
    arg_csr = tidy_Cursor_getUnderlyingType( arg_csr );
    CXCursor const class_csr = tidy_Cursor_getOutermostClass( arg_csr );
    if ( !clang_Cursor_isNull( class_csr ) )
      return class_csr;
  } // for

  return tidy_Cursor_skipLinkageSpec( sem_parent );
}

CXCursor tidy_Cursor_getOutermostClass( CXCursor cursor ) {
  CXCursor enclosing_csr = clang_getNullCursor();

  while ( !clang_Cursor_isNull( cursor ) ) {
    if ( tidy_Cursor_isClassDecl( cursor ) )
      enclosing_csr = cursor;
    else if ( !clang_Cursor_isNull( enclosing_csr ) )
      break;
    cursor = clang_getCursorSemanticParent( cursor );
  } // wnile

  return enclosing_csr;
}

char* tidy_Cursor_getScopedDisplayName( CXCursor cursor ) {
  return getScopedName_thunk( cursor, &clang_getCursorDisplayName );
}

char* tidy_Cursor_getScopedSimpleName( CXCursor cursor ) {
  return getScopedName_thunk( cursor, &clang_getCursorSpelling );
}

CXCursor tidy_Cursor_getUnderlyingType( CXCursor cursor ) {
  CXType type = clang_getCursorType( cursor );
  for ( bool done = false; !done; ) {
    switch ( type.kind ) {
      case CXType_Pointer:
        type = clang_getPointeeType( type );
        break;
      case CXType_LValueReference:
      case CXType_RValueReference:
        type = clang_getNonReferenceType( type );
        break;
      default:
        done = true;
        break;
    } // switch
  } // for

  return clang_getTypeDeclaration( type );
}

bool tidy_Cursor_isBeforeInTranslationUnit( CXCursor i_csr, CXCursor j_csr ) {
  if ( tidy_Cursor_isInvalid( i_csr ) || tidy_Cursor_isInvalid( j_csr ) )
    return false;
  CXSourceLocation const i_loc = clang_getCursorLocation( i_csr );
  CXSourceLocation const j_loc = clang_getCursorLocation( j_csr );
  return clang_isBeforeInTranslationUnit( i_loc, j_loc );
}

bool tidy_Cursor_isClassDecl( CXCursor cursor ) {
  enum CXCursorKind const kind = clang_getCursorKind( cursor );
  switch ( kind ) {
    case CXCursor_ClassDecl:
    case CXCursor_StructDecl:
    case CXCursor_ClassTemplate:
    case CXCursor_ClassTemplatePartialSpecialization:
    case CXCursor_UnionDecl:
      return true;
    default:
      return false;
  } // switch
}

bool tidy_Cursor_isFunctionDecl( CXCursor cursor ) {
  enum CXCursorKind const kind = clang_getCursorKind( cursor );
  switch ( kind ) {
    case CXCursor_Constructor:
    case CXCursor_ConversionFunction:
    case CXCursor_CXXMethod:
    case CXCursor_Destructor:
    case CXCursor_FunctionDecl:
    case CXCursor_FunctionTemplate:
      return true;
    default:
      return false;
  } // switch
}

bool tidy_Cursor_isInFile( CXCursor cursor, CXFile file ) {
  assert( file != NULL );

  CXFile const cursor_file = tidy_getCursorLocation_File( cursor );
  return cursor_file != NULL && clang_File_isEqual( cursor_file, file );
}

bool tidy_Cursor_isInheritedFrom( CXCursor cursor, CXCursor base_csr ) {
  if ( tidy_Cursor_isInvalid( cursor ) || tidy_Cursor_isInvalid( base_csr ) )
    return false;

  if ( tidy_Cursor_isInheritable( base_csr ) )
    base_csr = clang_getCursorSemanticParent( base_csr );
  if ( !tidy_Cursor_isClassDecl( base_csr ) )
    return false;
  base_csr = clang_getCanonicalCursor( base_csr );

  while ( !tidy_Cursor_isInvalid( cursor ) ) {
    enum CXCursorKind const kind = clang_getCursorKind( cursor );
    if ( clang_isTranslationUnit( kind ) )
      break;

    if ( tidy_Cursor_isClassDecl( cursor ) ) {
      if ( tidy_Cursor_isTemplateSpecializationOf( base_csr, cursor ) )
        return true;
      isBaseClass_data ibcd = { .base_csr = base_csr };
      clang_visitChildren( cursor, &isBaseClass_visitor, &ibcd );
      if ( ibcd.is_base )
        return true;
    }

    CXCursor parent = clang_getCursorSemanticParent( cursor );
    if ( tidy_Cursor_isInvalid( parent ) ||
         clang_equalCursors( parent, cursor ) ) {
      parent = clang_getCursorLexicalParent( cursor );
    }
    cursor = parent;
  } // while

  return false;
}

bool tidy_Cursor_isInheritedMemberFunctionCall( CXCursor expr_csr,
                                                CXCursor *pclass_csr ) {
  expr_csr = tidy_Cursor_getVarInitializer( expr_csr );
  expr_csr = tidy_Cursor_skipUnexposedDown( expr_csr );

  enum CXCursorKind const kind = clang_getCursorKind( expr_csr );
  if ( kind != CXCursor_CallExpr )
    return false;

  // Get the class of the member function.
  CXCursor const callee_csr = tidy_Cursor_getFirstExposedChild( expr_csr );
  if ( clang_getCursorKind( callee_csr ) != CXCursor_MemberRefExpr )
    return false;
  CXCursor const mbr_fn_csr = clang_getCursorReferenced( callee_csr );
  if ( clang_Cursor_isNull( mbr_fn_csr ) )
    return false;
  CXCursor mbr_fn_class_csr = clang_getCursorSemanticParent( mbr_fn_csr );
  mbr_fn_class_csr = clang_getCanonicalCursor( mbr_fn_class_csr );
  if ( clang_Cursor_isNull( mbr_fn_class_csr ) )
    return false;

  // Get the object the member function is being called on.
  CXCursor const obj_csr = tidy_Cursor_getFirstExposedChild( callee_csr );
  if ( clang_Cursor_isNull( obj_csr ) )
    return false;
  CXCursor obj_class_csr = tidy_Cursor_getUnderlyingType( obj_csr );
  obj_class_csr = clang_getCanonicalCursor( obj_class_csr );
  if ( clang_Cursor_isNull( obj_class_csr ) )
    return false;

  // Is the object's class inherited from the member function's class?
  if ( !tidy_Cursor_isInheritedFrom( obj_class_csr, mbr_fn_class_csr ) )
    return false;

  if ( pclass_csr != NULL )
    *pclass_csr = mbr_fn_class_csr;
  return true;
}

bool tidy_Cursor_isInvalid( CXCursor cursor ) {
  return clang_Cursor_isNull( cursor ) || clang_isInvalid( cursor.kind );
}

bool tidy_Cursor_isOutOfLineDefinition( CXCursor cursor, CXCursor parent,
                                        CXCursor *pclass_csr ) {
  enum CXCursorKind const kind = clang_getCursorKind( cursor );
  switch ( kind ) {
    case CXCursor_Constructor:
    case CXCursor_ConversionFunction:
    case CXCursor_CXXMethod:
    case CXCursor_Destructor:
    case CXCursor_VarDecl:
      if ( clang_isCursorDefinition( cursor ) ) {
        CXCursor const class_csr = clang_getCursorSemanticParent( cursor );
        bool const is_out_of_line = !clang_equalCursors( parent, class_csr );
        if ( is_out_of_line && pclass_csr != NULL )
          *pclass_csr = class_csr;
        return is_out_of_line;
      }
      break;
    default:
      /* suppress warning */;
  } // switch
  return false;
}

bool tidy_Cursor_isScopeDecl( CXCursor cursor ) {
  enum CXCursorKind const kind = clang_getCursorKind( cursor );
  switch ( kind ) {
    case CXCursor_ClassDecl:
    case CXCursor_ClassTemplate:
    case CXCursor_ClassTemplatePartialSpecialization:
    case CXCursor_EnumDecl:
    case CXCursor_Namespace:
    case CXCursor_StructDecl:
    case CXCursor_TypeAliasDecl:
    case CXCursor_TypedefDecl:
    case CXCursor_UnionDecl:
      return true;
    default:
      return false;
  } // switch
}

bool tidy_Cursor_isTemplateSpecializationOf( CXCursor cursor,
                                             CXCursor template_csr ) {
  template_csr = clang_getSpecializedCursorTemplate( template_csr );
  if ( !tidy_Cursor_isInvalid( template_csr ) ) {
    template_csr = clang_getCanonicalCursor( template_csr );
    if ( clang_equalCursors( cursor, template_csr ) )
      return true;
  }
  return false;
}

bool tidy_Cursor_isTypeAliasOf( CXCursor alias_csr, CXCursor underlying_csr ) {
  enum CXCursorKind kind;
  alias_csr = tidy_getCursorReferenced( alias_csr, &kind );
  if ( kind != CXCursor_TypedefDecl && kind != CXCursor_TypeAliasDecl )
    return false;

  CXType alias_type = clang_getTypedefDeclUnderlyingType( alias_csr );
  alias_type = clang_getCanonicalType( alias_type );

  CXType underlying_type = clang_getCursorType( underlying_csr );
  underlying_type = clang_getCanonicalType( underlying_type );

  if ( clang_equalTypes( alias_type, underlying_type ) )
    return true;

  alias_csr = clang_getTypeDeclaration( alias_type );
  if ( tidy_Cursor_isInvalid( alias_csr ) )
    return false;

  CXCursor spec_csr = clang_getSpecializedCursorTemplate( alias_csr );
  if ( !tidy_Cursor_isInvalid( spec_csr ) )
    alias_csr = spec_csr;

  spec_csr = clang_getSpecializedCursorTemplate( underlying_csr );
  if ( !tidy_Cursor_isInvalid( spec_csr ) )
    underlying_csr = spec_csr;

  return clang_equalCursors( alias_csr, underlying_csr );
}

int tidy_File_compareByName( CXFile i_file, CXFile j_file ) {
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

CXCursor tidy_getCursorByName( char const *name, CXCursor scope_csr ) {
  assert( name != NULL );

  getCursorByName_data gcbnd = {
    .find_name = name,
    .found_csr = clang_getNullCursor(),
    .skip_csr = clang_getNullCursor()
  };

  while ( !tidy_Cursor_isInvalid( scope_csr ) ) {
    clang_visitChildren( scope_csr, &getCursorByName_visitor, &gcbnd );
    if ( !clang_Cursor_isNull( gcbnd.found_csr ) )
      return gcbnd.found_csr;
    enum CXCursorKind const kind = clang_getCursorKind( scope_csr );
    if ( kind == CXCursor_TranslationUnit )
      break;
    CXCursor const sem_parent = clang_getCursorSemanticParent( scope_csr );
    if ( clang_equalCursors( sem_parent, scope_csr ) )
      break;

    gcbnd.cxx_recurse_into_scope = true;
    gcbnd.skip_csr = scope_csr;
    scope_csr = sem_parent;
  } // while

  return (CXCursor){ .kind = CXCursor_NoDeclFound };
}

CXCursor tidy_getCursorByNameToken( CXTranslationUnit tu, CXToken token,
                                    CXCursor scope_csr ) {
  if ( clang_getTokenKind( token ) != CXToken_Identifier )
    return clang_getNullCursor();

  CXString const    token_cxs = clang_getTokenSpelling( tu, token );
  char const *const token_cs = clang_getCString( token_cxs );
  CXCursor const    rv_csr = tidy_getCursorByName( token_cs, scope_csr );

  clang_disposeString( token_cxs );
  return rv_csr;
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

CXToken const* tidy_Token_getNext( CXToken const tokens[], unsigned token_count,
                                   unsigned *ptoken_idx ) {
  assert( ptoken_idx != NULL );

  CXToken const *pnext_token = NULL;
  unsigned i;

  for ( i = *ptoken_idx + 1; i < token_count; ++i ) {
    CXTokenKind const kind = clang_getTokenKind( tokens[i] );
    if ( kind != CXToken_Comment ) {
      pnext_token = &tokens[i];
      break;
    }
  } // for

  *ptoken_idx = i;
  return pnext_token;
}

CXToken const* tidy_Token_getPrev( CXToken const tokens[], int *ptoken_idx ) {
  assert( ptoken_idx != NULL );

  CXToken const *pprev_token = NULL;
  int i;

  for ( i = *ptoken_idx - 1; i >= 0; --i ) {
    CXTokenKind const kind = clang_getTokenKind( tokens[i] );
    if ( kind != CXToken_Comment ) {
      pprev_token = &tokens[i];
      break;
    }
  } // for

  *ptoken_idx = i;
  return pprev_token;
}

bool tidy_Token_isEqualTo( CXTranslationUnit tu, CXToken token,
                           char const *value ) {
  assert( value != NULL );

  CXString const    token_cxs = clang_getTokenSpelling( tu, token );
  char const *const token_cs = clang_getCString( token_cxs );
  int const         cmp = strcmp( token_cs, value );

  clang_disposeString( token_cxs );
  return cmp == 0;
}

int tidy_Token_isEqualToAny_impl( CXTranslationUnit tu, CXToken token,
                                  char const *values[] ) {
  assert( values != NULL );

  CXString const    token_cxs = clang_getTokenSpelling( tu, token );
  char const *const token_cs = clang_getCString( token_cxs );

  int equal_idx = -1;

  for ( int i = 0; values[i] != NULL; ++i ) {
    if ( strcmp( token_cs, values[i] ) == 0 ) {
      equal_idx = i;
      break;
    }
  } // for

  clang_disposeString( token_cxs );
  return equal_idx;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/// @cond DOXYGEN_IGNORE

extern inline int tidy_FileUniqueID_compare( CXFileUniqueID const*,
                                             CXFileUniqueID const* );

/// @endcond

/* vim:set et sw=2 ts=2: */
