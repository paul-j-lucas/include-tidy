#include <clang-c/Index.h>

#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#define ARRAY_SIZE(ARRAY)         ( sizeof ((ARRAY)) / sizeof 0[ (ARRAY) ] )

char const *prog_name;

struct visitor_data {
  CXFile source_file;
};
typedef struct visitor_data visitor_data;

enum CXChildVisitResult visitor( CXCursor cursor, CXCursor parent,
                                 CXClientData client_data ) {
  visitor_data const *const data = (visitor_data const*)client_data;
  enum CXCursorKind const kind = clang_getCursorKind( cursor );

  if ( !(clang_isDeclaration( kind ) || kind == CXCursor_MacroDefinition) )
    goto skip_kind;

  CXString name = clang_getCursorSpelling( cursor );
  char const *const name_cstr = clang_getCString( name );

  // Ignore empty/anonymous symbols
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

_Noreturn
static void print_usage( int status ) {
  FILE *const fout = status == EX_OK ? stdout : stderr;
  fprintf( fout, "usage: %s [options] source-file\n", prog_name );
  exit( status );
}

int main( int argc, char const *const argv[] ) {
  prog_name = argv[0];
  if ( argc < 2 )
    print_usage( EX_USAGE );
  char const *const source_file = argv[1];

  CXIndex index = clang_createIndex( 0, 0 );

  // We need detailed preprocessing records to extract macro definitions
  char const *const args[] = { "-detailed-preprocessing-record" };

  CXTranslationUnit tu = clang_parseTranslationUnit(
    index, 
    source_file,
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
  visitor_data data = { clang_getFile( tu, source_file ) };
  clang_visitChildren( cursor, visitor, &data );

  clang_disposeTranslationUnit( tu );

error:
  clang_disposeIndex( index );
  return rv;
}
