/*
**      include-tidy -- #include tidier
**      src/proxies.c
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
 * Defines functions for include proxies.
 */

// local
#include "pjl_config.h"
#include "proxies.h"
#include "cli_options.h"
#include "include.h"
#include "config_file.h"
#include "options.h"
#include "path_util.h"
#include "print.h"
#include "red_black.h"
#include "trans_unit.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <limits.h>                     /* for PATH_MAX */
#include <stdbool.h>
#include <stdlib.h>                     /* for atexit(3) */
#include <string.h>

/// @endcond

/**
 * @addtogroup tidy-proxies-group
 * @{
 */

////////// local functions ////////////////////////////////////////////////////

/**
 * Gets the corresponding C++ header name of \a c_name, e.g., given `string.h`,
 * returns `cstring`.
 *
 * @param c_name The C header name.
 * @param path_buf A path buffer to receive the C++ header name, if any.
 * @return
 * @parblock
 * Returns \a path_buf containing the corresponding C++ header name of \a
 * c_name only if \a c_name:
 *  + Has a filename extension of `.h`; and:
 *  + \a path_buf is big enough to hold the result.
 *
 * Otherwise returns NULL.
 * @endparblock
 */
PJL_DISCARD
static char const* get_cxx_header( char const *c_name,
                                   char path_buf[static PATH_MAX] ) {
  assert( c_name != NULL );

  char const *const c_ext = path_ext( c_name );
  if ( c_ext == NULL || strcmp( c_ext, "h" ) != 0 )
    return NULL;

  char const *const dot = c_ext - 1;
  size_t const base_len = STATIC_CAST( size_t, dot - c_name );
  if ( base_len + 1/*'c'*/ + 1/*'\0'*/ > PATH_MAX )
    return NULL;

  path_buf[0] = 'c';
  memcpy( path_buf + 1, c_name, base_len );
  path_buf[ base_len + 1 ] = '\0';
  return path_buf;
}

/**
 * Visits each `#include` directive in a translation unit for initializing
 * implicit include proxies.
 *
 * @param cursor The cursor for the symbol in the AST being visited.
 * @param parent Not used.
 * @param data Not used.
 * @return Always returns `CXChildVisit_Continue`.
 */
static enum CXChildVisitResult implicit_proxies_visitor( CXCursor cursor,
                                                         CXCursor parent,
                                                         CXClientData data ) {
  (void)parent;
  (void)data;

  if ( clang_getCursorKind( cursor ) != CXCursor_InclusionDirective )
    goto skip;

  CXFile const included_file = clang_getIncludedFile( cursor );
  assert( included_file != NULL );
  tidy_include *const included = include_find_by_File( included_file );
  assert( included != NULL );

  if ( included->proxy != NULL )
    goto skip;
  if ( included->depth == 0 )           // directly included: no proxy
    goto skip;
  if ( included->is_local )             // only non-local can have a proxy
    goto skip;

  tidy_include *const includer = included->includer;
  assert( includer != NULL );           // since directly included
  if ( includer->is_local )             // only non-local can be a proxy
    goto skip;

  tidy_include *proxy = NULL;

  if (// This handles a case like:
      //
      //      </usr/include/stdlib.h>
      //        </usr/include/_stdlib.h>
      //
      // That is, a standard header includes an implementation header that
      // isn't a standard header.  The standard header should be a proxy for
      // the implementation header.
      //
      !config_is_standard_include( included->rel_path ) ||

      // This handles a case like:
      //
      //      <../lib/stdlib.h>
      //        </usr/include/stdlib.h>
      //
      // That is, a local implementation of a standard header (as is done when
      // using Gnulib) eventually does a (non-standard) #include_next to
      // include the real standard one.  The local header (even though it's
      // standard) should be a proxy for the real one.
      //
      strcmp( path_basename( included->rel_path ),
              path_basename( includer->rel_path ) ) == 0 ) {
    proxy = includer;
    goto done;
  }

  // Remaining cases are valid only for C++.
  if ( !tidy_source_is_cxx )
    goto skip;

  // Remaining cases are valid only for paths that are just filenames.
  if ( !path_is_filename( included->rel_path ) )
    goto skip;

  char cxx_path[ PATH_MAX ];
  if ( get_cxx_header( included->rel_path, cxx_path ) != NULL ) {
    if ( strcmp( includer->rel_path, cxx_path ) == 0 ) {
      //
      // This handles a case like:
      //
      //      </usr/include/cstring>
      //        </usr/include/string.h>
      //
      // That is, a standard C++ header is the C++ wrapper of a C standard
      // header.
      //
      proxy = includer;
    }
    else {
      //
      // This handles a case similar to the above except check to see if the
      // standard C++ wrapper has been included at all.
      //
      proxy = include_find_by_rel_path( cxx_path );
    }
  }

done:
  if ( proxy != NULL ) {
    while ( proxy->proxy != NULL )
      proxy = proxy->proxy;
    included->proxy = proxy;
  }

skip:
  return CXChildVisit_Continue;
}

/**
 * Dumps include proxies.
 *
 * @param want_explicit If `true`, dump explicit proxies only; if `false`, dump
 * implicit proxies only.
 */
static void include_proxies_dump( bool want_explicit ) {
  rb_iterator_t iter;
  rb_iterator_init( &iter, &tidy_include_set );
  bool printed_proxies_header = false;

  for ( tidy_include const *include;
        (include = rb_iterator_next( &iter )) != NULL; ) {
    if ( include->proxy == NULL )
      continue;
    if ( include->is_proxy_explicit != want_explicit )
      continue;
    if ( verbose_section_begin( &printed_proxies_header ) ) {
      verbose_printf(
        "%s proxies:\n",
        want_explicit ? "explicit" : "implicit"
      );
    }
    char delims[2], proxy_delims[2];
    include_get_delims( include, delims );
    include_get_delims( include->proxy, proxy_delims );
    verbose_printf(
      "  %c%s%c -> %c%s%c\n",
      delims[0], include->abs_path, delims[1],
      proxy_delims[0], include->proxy->abs_path, proxy_delims[1]
    );
  } // for
}

////////// extern functions ///////////////////////////////////////////////////

void implicit_proxies_init( void ) {
  ASSERT_RUN_ONCE();

  CXCursor const cursor = clang_getTranslationUnitCursor( tidy_tu );
  clang_visitChildren( cursor, &implicit_proxies_visitor, /*data=*/NULL );

  if ( IS_VERBOSE( PROXIES_EXPLICIT ) )
    include_proxies_dump( /*want_explicit=*/true );
  if ( IS_VERBOSE( PROXIES_IMPLICIT ) )
    include_proxies_dump( /*want_explicit=*/false );
}

bool include_proxy_would_cycle( tidy_include const *from_include,
                                tidy_include const *to_include ) {
  assert( from_include != NULL );
  assert( to_include != NULL );

  for ( tidy_include const *include = to_include; include != NULL;
        include = include->proxy ) {
    if ( include == from_include )
      return true;
  } // for

  return false;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
