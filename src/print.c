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
 * Defines functions for printing error and warning messages.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "print.h"
#include "color.h"
#include "options.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

/// @endcond

/**
 * @addtogroup printing-errors-warnings-group
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
 * @param tidy_file The name of the file where this function was called from.
 * @param tidy_line The line number within \a tidy_file where this function was
 * called from.
 * @param source_path The source file's path.
 * @param source_line The source file's error line.
 * @param source_col The source file's error column.
 * @param what What to print, e.g., `error` or `warning`.
 * @param what_color The color to print \a what in, if any.
 * @param format The `printf()` style format string.
 * @param args The `printf()` arguments.
 */
static void fl_print_impl( char const *tidy_file, int tidy_line,
                           char const *source_path, unsigned source_line,
                           unsigned source_col, char const *what,
                           char const *what_color, char const *format,
                           va_list args ) {
  assert( tidy_file != NULL );
  assert( tidy_line > 0 );
  assert( what != NULL );
  assert( format != NULL );

  if ( source_path != NULL ) {
    color_start( stderr, sgr_locus );
    EPRINTF( "\"%s\"", source_path );
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
  }

  EPUTS( ": " );
  color_start( stderr, what_color );
  EPUTS( what );
  color_end( stderr, what_color );
  EPUTS( ": " );

#if 0
  if ( opt_debug )
    EPRINTF( "[%s:%d] ", tidy_file, tidy_line );
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
  vfprintf( stderr, format, args );
#pragma GCC diagnostic pop
}

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
    tidy_file, tidy_line, source_path, source_line, source_col, "error",
    sgr_error, format, args
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
    tidy_file, tidy_line, source_path, source_line, source_col, "warning",
    sgr_warning, format, args
  );
  va_end( args );
}

int verbose_printf( char const *format, ... ) {
  if ( opt_verbose == TIDY_VERBOSE_NONE )
    return 0;
  fputs( "// tidy | ", stdout );
  va_list args;
  va_start( args, format );
  int const raw_len = vprintf( format, args );
  va_end( args );
  return raw_len;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
