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
#include "color.h"
#include "include-tidy.h"
#include "options.h"
#include "path_util.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>                     /* for free */
#include <sysexits.h>

/// @endcond

/**
 * @addtogroup printing-group
 * @{
 */

////////// local functions ////////////////////////////////////////////////////

/**
 * Prints a message to standard error.
 *
 * @note In debug mode, also prints the file & line where the function was
 * called from.
 * @note A newline is _not_ printed.
 *
 * @param caller_file The name of the file where this function was called from.
 * @param caller_line The line number within \a caller_file where this function
 * was called from.
 * @param origin Message origin, if any.
 * @param source_path The source file's path or NULL for none.
 * @param source_line The source file's error line or zero for none.
 * @param source_col The source file's error column or zero for none.
 * @param what What kind of message, e.g., `"error"` or `"warning"`.
 * @param what_color The color to print \a what in, if any.
 * @param format The `printf()` style format string.
 * @param args The `printf()` arguments.
 */
static void fl_vprint_impl( char const *caller_file, int caller_line,
                            char const *origin, char const *source_path,
                            unsigned source_line, unsigned source_col,
                            char const *what, char const *what_color,
                            char const *format, va_list args ) {
  assert( caller_file != NULL );
  assert( caller_line > 0 );
  assert( what != NULL );
  assert( format != NULL );

  if ( origin != NULL ) {
    EPRINTF( "%s (via %s): ", origin, prog_name );
  }
  else if ( source_path != NULL ) {
    color_start( stderr, sgr_locus );
    EPRINTF( "\"%s\"", path_no_dot_slash( source_path ) );
    color_end( stderr, sgr_locus );

    if ( source_line > 0 ) {
      EPUTC( ':' );
      color_start( stderr, sgr_locus );
      EPRINTF( "%u", source_line );
      color_end( stderr, sgr_locus );

      if ( source_col > 0 ) {
        EPUTC( ',' );
        color_start( stderr, sgr_locus );
        EPRINTF( "%u", source_col );
        color_end( stderr, sgr_locus );
      }
    }
    EPUTS( ": " );
  }
  else {
    EPRINTF( "%s: ", prog_name );
  }

  color_start( stderr, what_color );
  EPUTS( what );
  color_end( stderr, what_color );
  EPUTS( ": " );

  // LCOV_EXCL_START
  if ( opt_debug )
    EPRINTF( "[%s:%d] ", caller_file, caller_line );
  // LCOV_EXCL_STOP

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
  vfprintf( stderr, format, args );
#pragma GCC diagnostic pop
}

////////// extern functions ///////////////////////////////////////////////////

void fl_print_error( char const *caller_file, int caller_line,
                     char const *origin, char const *source_path,
                     unsigned source_line, unsigned source_col,
                     char const *format, ... ) {
  assert( caller_file != NULL );
  assert( caller_line > 0 );
  assert( format != NULL );

  va_list args;
  va_start( args, format );
  fl_vprint_impl(
    caller_file, caller_line, origin,
    source_path, source_line, source_col,
    "error", sgr_error,
    format, args
  );
  va_end( args );
}

void fl_print_warning( char const *caller_file, int caller_line,
                       char const *source_path, unsigned source_line,
                       unsigned source_col, char const *format, ... ) {
  assert( caller_file != NULL );
  assert( caller_line > 0 );
  assert( format != NULL );

  va_list args;
  va_start( args, format );
  fl_vprint_impl(
    caller_file, caller_line, /*origin=*/NULL,
    source_path, source_line, source_col,
    "warning", sgr_warning,
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

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
