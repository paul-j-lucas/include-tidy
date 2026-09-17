/*
**      include-tidy -- #include tidier
**      src/print.c
**
**      Copyright (C) 2017-2026  Paul J. Lucas
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
 * Defines functions for printing errors, warnings, and other things.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "print.h"
#include "array.h"
#include "clang_util.h"
#include "cli_options.h"
#include "color.h"
#include "include-tidy.h"
#include "options.h"
#include "path_util.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>                     /* for free */
#include <string.h>
#include <sysexits.h>

/// @endcond

/**
 * @addtogroup printing-group
 * @{
 */

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct fl_print_args fl_print_args;

////////// structs ////////////////////////////////////////////////////////////

/**
 * Additional arguments for fl_print_impl().
 */
struct fl_print_args {
  char const   *tidy_file;              ///< Called-from source file.
  int           tidy_line;              ///< Called-from source line.
  char const   *what;                   ///< Print what: `error` or `warning`.
  char const   *what_color;             ///< Color for \ref what.

  /**
   * Fields for libclang messages.
   */
  struct {
    bool        is_libclang_message;    ///< Is message from libclang?
  };

  /**
   * Fields for source file messages.
   */
  struct {
    char const *source_path;            ///< Source path or NULL for none.
    unsigned    source_line;            ///< Source line or zero for none.
    unsigned    source_col;             ///< Source column or zero for none.
  };
};

////////// local functions ////////////////////////////////////////////////////

// LCOV_EXCL_START
/**
 * Visits each child cursor of a cursor collecting its CXSourceRange.
 *
 * @param cursor being visited.
 * @param parent The parent cursor of \a cursor.
 * @param data A pointer to an array_t to append the CXSourceRange to.
 * @return Always returns `CXChildVisit_Continue`.
 */
static enum CXChildVisitResult cursor_ranges_visitor( CXCursor cursor,
                                                      CXCursor parent,
                                                      CXClientData data ) {
  (void)parent;

  //
  // Restrict range collection to expressions and statements only.
  //
  // If other AST nodes (such as TypeRef, type specifiers, or children of a
  // DeclStmt) were treated as elidable child ranges:
  //
  //  + A DeclStmt would elide its inner VarDecl child node entirely, reducing
  //    the output to just ";".
  //
  //  + A VarDecl would elide its type tokens (e.g., "unsigned", "int", etc.)
  //    into "..." leaving only the variable identifier (e.g., "x").
  //
  // Restricting collection to expressions and statements ensures sub-
  // expressions (e.g., function call arguments, return values) and sub-
  // statements (e.g., loop/if bodies) elide cleanly into "..." without eliding
  // type names, storage classes, or declaration spellings.
  //
  enum CXCursorKind const kind = clang_getCursorKind( cursor );
  if ( clang_isExpression( kind ) || clang_isStatement( kind ) ) {
    CXSourceRange const range = clang_getCursorExtent( cursor );
    assert( data != NULL );
    array_t *const ranges = data;
    *(CXSourceRange*)array_push_back( ranges ) = range;
  }

  return CXChildVisit_Continue;
}
// LCOV_EXCL_STOP

/**
 * Prints a message to standard error.
 *
 * @note In debug mode, also prints the file & line where the function was
 * called from.
 * @note A newline is _not_ printed.
 *
 * @param flpa The The fl_print_args to use.
 * @param format The `printf()` style format string.
 * @param args The `printf()` arguments.
 */
static void fl_print_impl( fl_print_args const *flpa, char const *format,
                           va_list args ) {
  assert( flpa != NULL );
  assert( flpa->tidy_file != NULL );
  assert( flpa->tidy_line > 0 );
  assert( flpa->what != NULL );
  assert( format != NULL );

  if ( flpa->is_libclang_message ) {
    EPRINTF( "libclang (via %s): ", prog_name );
  }
  else if ( flpa->source_path != NULL ) {
    color_start( stderr, sgr_locus );
    EPRINTF( "\"%s\"", path_no_dot_slash( flpa->source_path ) );
    color_end( stderr, sgr_locus );

    if ( flpa->source_line > 0 ) {
      EPUTC( ':' );
      color_start( stderr, sgr_locus );
      EPRINTF( "%u", flpa->source_line );
      color_end( stderr, sgr_locus );

      if ( flpa->source_col > 0 ) {
        EPUTC( ',' );
        color_start( stderr, sgr_locus );
        EPRINTF( "%u", flpa->source_col );
        color_end( stderr, sgr_locus );
      }
    }
    EPUTS( ": " );
  }
  else {
    EPRINTF( "%s: ", prog_name );
  }

  color_start( stderr, flpa->what_color );
  EPUTS( flpa->what );
  color_end( stderr, flpa->what_color );
  EPUTS( ": " );

  // LCOV_EXCL_START
  if ( opt_debug )
    EPRINTF( "[%s:%d] ", flpa->tidy_file, flpa->tidy_line );
  // LCOV_EXCL_STOP

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
  vfprintf( stderr, format, args );
#pragma GCC diagnostic pop
}

// LCOV_EXCL_START
/**
 * Gets whether \a loc is in any range in \a ranges.
 *
 * @param loc The location to check.
 * @param ranges An array of ranges to check against.
 * @return Returns `true` only if loc is in any range in \a ranges.
 */
static bool is_loc_in_any_range( CXSourceLocation loc, array_t const *ranges ) {
  assert( ranges != NULL );

  if ( ranges->len == 0 )
    return false;

  CXFile file;
  unsigned offset;
  clang_getFileLocation( loc, &file, /*line=*/NULL, /*column=*/NULL, &offset );

  for ( size_t i = 0; i < ranges->len; ++i ) {
    CXSourceRange const *const  range = array_at_nc( ranges, i );
    CXSourceLocation const      start_loc = clang_getRangeStart( *range );
    CXSourceLocation const      end_loc = clang_getRangeEnd( *range );
    CXFile                      start_file, end_file;
    unsigned                    start_offset, end_offset;

    clang_getFileLocation( start_loc, &start_file, NULL, NULL, &start_offset );
    clang_getFileLocation( end_loc, &end_file, NULL, NULL, &end_offset );

    if ( file == start_file && offset >= start_offset && offset < end_offset )
      return true;
  } // for

  return false;
}

/**
 * Prints an abridged subset of tokens comprising \a cursor.
 *
 * @remarks For kinds of cursor that don't have a simple name, e.g., a
 * CompoundStmt, print an abridged set of tokens comprising it.
 *
 * @param cursor The cursor to print the tokens for.
 */
static void tidy_Cursor_printAbridgedTokens( CXCursor cursor ) {
  CXSourceRange const     range = tidy_getCursorExtent( cursor );
  CXTranslationUnit const tu = clang_Cursor_getTranslationUnit( cursor );

  CXToken *tokens = NULL;
  unsigned token_count = 0;
  clang_tokenize( tu, range, &tokens, &token_count );

  if ( token_count == 0 ) {
    PUTS( "null" );
    return;
  }

  array_t cursor_ranges = ARRAY_INIT( sizeof( CXSourceRange ) );
  clang_visitChildren( cursor, cursor_ranges_visitor, &cursor_ranges );

  bool        eliding = false;          // eliding tokens?
  bool        prev_token_is_semicolon = false;
  bool        prev_token_is_space_after_keyword = false;
  CXTokenKind prev_token_kind = STATIC_CAST( CXTokenKind, -1 );
  bool        printed = false;          // print anything yet?

  for ( unsigned i = 0; i < token_count; ++i ) {
    CXTokenKind const token_kind = clang_getTokenKind( tokens[i] );
    if ( token_kind == CXToken_Comment )
      continue;

    CXSourceLocation const loc = clang_getTokenLocation( tu, tokens[i] );
    if ( is_loc_in_any_range( loc, &cursor_ranges ) ) {
      eliding = true;
      continue;
    }

    if ( eliding ) {
      if ( printed )
        PUTS( " ... " );
      eliding = false;
      prev_token_is_semicolon = false;
      prev_token_is_space_after_keyword = false;
      prev_token_kind = CXToken_Punctuation;
    }

    CXString const    token_cxs = clang_getTokenSpelling( tu, tokens[i] );
    char const *const token_cs = clang_getCString( token_cxs );

    bool const print_space =
      printed && (
        (prev_token_kind != CXToken_Punctuation &&
              token_kind != CXToken_Punctuation) ||
        prev_token_is_semicolon ||
        (prev_token_is_space_after_keyword && strcmp( token_cs, "(" ) == 0)
      );

    if ( print_space )
      putchar( ' ' );
    fputs_escaped( token_cs, stdout );
    printed = true;

    prev_token_is_semicolon = strcmp( token_cs, ";" ) == 0;
    prev_token_kind = token_kind;
    prev_token_is_space_after_keyword = prev_token_kind == CXToken_Keyword &&
      (str_is_any( token_cs, "for", "if", "switch", "while" ) ||
       (tidy_source_is_cxx && str_is_any( token_cs, "catch" )));

    clang_disposeString( token_cxs );
  } // for

  if ( printed && eliding )
    PUTS( " ..." );

  array_cleanup( &cursor_ranges, /*free_fn=*/NULL );
  clang_disposeTokens( tu, tokens, token_count );
}
// LCOV_EXCL_STOP

////////// extern functions ///////////////////////////////////////////////////

void fl_print_error( char const *tidy_file, int tidy_line,
                     char const *source_path, unsigned source_line,
                     unsigned source_col, char const *format, ... ) {
  assert( tidy_file != NULL );
  assert( tidy_line > 0 );
  assert( format != NULL );

  va_list args;
  va_start( args, format );
  fl_print_impl(
    &(fl_print_args){
      .tidy_file = tidy_file,
      .tidy_line = tidy_line,
      .source_path = source_path,
      .source_line = source_line,
      .source_col = source_col,
      .what = "error",
      .what_color = sgr_error
    },
    format, args
  );
  va_end( args );
}

void fl_print_libclang_error( char const *tidy_file, int tidy_line,
                                char const *format, ... ) {
  assert( tidy_file != NULL );
  assert( tidy_line > 0 );
  assert( format != NULL );

  va_list args;
  va_start( args, format );
  fl_print_impl(
    &(fl_print_args){
      .tidy_file = tidy_file,
      .tidy_line = tidy_line,
      .is_libclang_message = true,
      .what = "error",
      .what_color = sgr_error
    },
    format, args
  );
  va_end( args );
}

void fl_print_warning( char const *tidy_file, int tidy_line,
                       char const *source_path, unsigned source_line,
                       unsigned source_col, char const *format, ... ) {
  assert( tidy_file != NULL );
  assert( tidy_line > 0 );
  assert( format != NULL );

  va_list args;
  va_start( args, format );
  fl_print_impl(
    &(fl_print_args){
      .tidy_file = tidy_file,
      .tidy_line = tidy_line,
      .source_path = source_path,
      .source_line = source_line,
      .source_col = source_col,
      .what = "warning",
      .what_color = sgr_warning
    },
    format, args
  );
  va_end( args );
}

void print_include( char const *sgr_color, char const delims[static 2],
                    char const *rel_path, char const *comment ) {
  assert( rel_path != NULL );

  color_start( stdout, sgr_color );
  int const raw_len = printf(
    "#include %c%s%c", delims[0], rel_path, delims[1]
  );
  if ( unlikely( raw_len < 0 ) ) {
    // LCOV_EXCL_START
    color_end( stdout, sgr_color );
    perror_exit( EX_IOERR );
    // LCOV_EXCL_STOP
  }

  if ( comment != NULL ) {
    unsigned const column = STATIC_CAST( unsigned, raw_len ) + 1;
    if ( column < opt_align_column )
      FPUTNSP( opt_align_column - column, stdout );
    printf( "%s%s%s", opt_comment_style[0], comment, opt_comment_style[1] );
  }

  color_end( stdout, sgr_color );
  putchar( '\n' );
}

void print_source_line( char const *path, unsigned line, unsigned col,
                        unsigned offset ) {
  assert( path != NULL );
  assert( line > 0 );
  assert( col > 0 );

  long const line_pos =
    STATIC_CAST( long, offset ) - (STATIC_CAST( long, col ) - 1);
  if ( unlikely( line_pos < 0 ) )
    return;                             // LCOV_EXCL_LINE
  FILE *const fsource = fopen( path, "rb" );
  if ( unlikely( fsource == NULL ) )
    return;                             // LCOV_EXCL_LINE
  if ( unlikely( fseek( fsource, line_pos, SEEK_SET ) == -1 ) )
    goto done;                          // LCOV_EXCL_LINE

  char         *line_buf = NULL;
  size_t        line_cap = 0;
  ssize_t const raw_len = getline( &line_buf, &line_cap, fsource );
  if ( unlikely( raw_len == -1 ) )
    goto done;                          // LCOV_EXCL_LINE
  unsigned const line_len = STATIC_CAST( unsigned, raw_len );

  EPRINTF( "%5u | %s", line, line_buf );
  //
  // getline() includes the \n in the buffer except if EOF is reached, so check
  // if the last character is \n: if not, print one explicitly.
  //
  if ( line_len > 0 && unlikely( line_buf[ line_len - 1 ] != '\n' ) )
    EPUTC( '\n' );                      // LCOV_EXCL_LINE

  EPRINTF( "%5s | ", "" );
  for ( unsigned i = 1; i < col && (i - 1) < line_len; ++i )
    EPUTC( line_buf[i - 1] == '\t' ? '\t' : ' ' );

  color_start( stderr, sgr_caret );
  EPUTC( '^' );
  color_end( stderr, sgr_caret );
  EPUTC( '\n' );

  free( line_buf );

done:
  fclose( fsource );
}

// LCOV_EXCL_START: -I, -isystem, etc., options contain absolute paths.
void verbose_print_argv( char const *label, int argc,
                         char const *const argv[] ) {
  verbose_section_begin( /*printed_header=*/NULL );
  verbose_printf( "%s argv:\n", label );
  for ( int i = 0; i < argc; ++i )
    verbose_printf( "  %2d = \"%s\"\n", i, argv[i] );
}
// LCOV_EXCL_STOP

// LCOV_EXCL_START: function cursors have platform-specific signatures.
void verbose_print_cursor_impl( char const *label, CXCursor cursor ) {
  label = empty_if_null( label );
  char const *const space = label[0] != '\0' ? " " : "";

  if ( clang_Cursor_isNull( cursor ) ) {
    verbose_printf( "%s%scursor: null (Null)\n", label, space );
    return;
  }

  CXSourceLocation const loc = clang_getCursorLocation( cursor );
  CXFile file;
  unsigned line, col;
  clang_getSpellingLocation( loc, &file, &line, &col, /*offset=*/NULL );

  CXString const abs_path_cxs = file != NULL ?
    tidy_File_getRealPathName( file ) : (CXString){ 0 };

  char const *const       abs_path = clang_getCString( abs_path_cxs );
  enum CXCursorKind const kind = clang_getCursorKind( cursor );
  CXString const          kind_cxs = clang_getCursorKindSpelling( kind );
  char const *const       kind_cs = clang_getCString( kind_cxs );
  char       *const       name = tidy_Cursor_getScopedDisplayName( cursor );

  verbose_printf( "%s%scursor: ", label, space );

  putchar( '"' );
  if ( null_if_empty( name ) != NULL ) {
    //
    // Use fputs_escaped() since C++ user-defined literals contain quotes,
    // e.g.:
    //
    //      constexpr double operator"" _km( long double val ) {
    //        return val * 1000.0;
    //      }
    //
    fputs_escaped( name, stdout );
  }
  else {
    //
    // The kind of cursor doesn't have a simple name, e.g., a CompoundStmt, so
    // print an abridged set of tokens comprising it.
    //
    tidy_Cursor_printAbridgedTokens( cursor );
  }
  putchar( '"' );

  printf( " (%s), \"%s\":%u,%u\n", kind_cs, abs_path, line, col );

  clang_disposeString( abs_path_cxs );
  clang_disposeString( kind_cxs );
  free( name );
}
// LCOV_EXCL_STOP

#ifndef NDEBUG
// LCOV_EXCL_START since this is used for debugging.
void verbose_print_tokens( CXCursor cursor ) {
  CXSourceRange const     range = tidy_getCursorExtent( cursor );
  CXTranslationUnit const tu = clang_Cursor_getTranslationUnit( cursor );

  CXToken *tokens;
  unsigned tokens_len;
  clang_tokenize( tu, range, &tokens, &tokens_len );

  verbose_printf( "tokens: [\n" );
  verbose_printf( "  " );
  for ( unsigned i = 0; i < tokens_len; ++i ) {
    CXString const    token_cxs = clang_getTokenSpelling( tu, tokens[i] );
    char const *const token_cs = clang_getCString( token_cxs );
    printf( "%s\"%s\"", i > 0 ? ", " : "", token_cs );
    clang_disposeString( token_cxs );
  }
  putchar( '\n' );
  verbose_printf( "]\n" );

  clang_disposeTokens( tu, tokens, tokens_len );
}
// LCOV_EXCL_STOP
#endif /* NDEBUG */

int verbose_printf( char const *format, ... ) {
  PUTS( "// tidy | " );
  va_list args;
  va_start( args, format );
  int const raw_len = vprintf( format, args );
  va_end( args );
  return raw_len;
}

bool verbose_print_statistics( void ) {
  static bool printed_header;
  bool const want_statistics = IS_VERBOSE( STATISTICS );
  if ( want_statistics && verbose_section_begin( &printed_header ) )
    verbose_printf( "statistics:\n" );
  return want_statistics;
}

bool verbose_section_begin( bool *printed_header ) {
  if ( printed_header != NULL && true_or_set( printed_header ) )
    return false;
  static bool print_blank_line;
  if ( true_or_set( &print_blank_line ) )
    verbose_printf( "\n" );
  return true;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
