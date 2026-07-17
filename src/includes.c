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
 * Defines structures and functions for keeping track of files included.
 */

// local
#include "pjl_config.h"
#include "includes.h"
#include "array.h"
#include "clang_util.h"
#include "cli_options.h"
#include "color.h"
#include "config_file.h"
#include "options.h"
#include "path_util.h"
#include "print.h"
#include "red_black.h"
#include "strbuf.h"
#include "symbols.h"
#include "tidy_util.h"
#include "trans_unit.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>                     /* for PATH_MAX */
#ifdef NEED_II_MATRIX                   /* See comment above ii_matrix def. */
#include <stdalign.h>
#endif /* NEED_II_MATRIX */
#include <stdbool.h>
#include <stdlib.h>                     /* for atexit(3) */
#include <string.h>
#include <sysexits.h>

/// @endcond

/**
 * @addtogroup tidy-includes-group
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

#define INCLUDE_VERBOSE_INDENT    2     /**< Spaces per include depth. */

////////// enums //////////////////////////////////////////////////////////////

/**
 * Include files are printed in groups.
 */
enum print_group {
  PRINT_LOCAL,                          ///< Local includes (with `""`).
  PRINT_3RD_PARTY,                      ///< 3rd-party includes (with `<>`).
  PRINT_STANDARD                        ///< Standard includes (with `<>`).
};

////////// typedefs ///////////////////////////////////////////////////////////

#ifdef NEED_II_MATRIX                   /* See comment above ii_matrix def. */
typedef unsigned char ii_matrix_t;      ///< Element type for ii_matrix.
#endif /* NEED_II_MATRIX */

typedef struct  includes_init_visitor_data  includes_init_visitor_data;
typedef struct  includes_print_visitor_data includes_print_visitor_data;
typedef enum    print_group                 print_group;

////////// structs ////////////////////////////////////////////////////////////

/**
 * Additional data passed to includes_init_visitor().
 */
struct includes_init_visitor_data {
  bool  verbose_printed;                ///< Printed any verbose output?
};

/**
 * Additional data for includes_print_visitor().
 */
struct includes_print_visitor_data {
  bool        print_blank_line;         ///< Print a blank line?
  bool        printed_any_includes;     ///< Print any includes?
  bool        printed_source_file;      ///< Printed source file name?
  print_group want_group;               ///< Print only this include group.
};

////////// local functions ////////////////////////////////////////////////////

#ifdef NEED_II_MATRIX                   /* See comment above ii_matrix def. */
static void   ii_matrix_visitor( CXFile, CXSourceLocation*, unsigned,
                                 CXClientData );
#endif /* NEED_II_MATRIX */

NODISCARD
static char*  make_symbols_comment( tidy_include const* );

NODISCARD
static char*  tidy_File_getRelativePath( CXFile );

static void   tidy_include_cleanup( tidy_include* );

NODISCARD
static int    tidy_symbol_ptr_cmp_by_name_length( void const*, void const* ),
              tidy_symbol_ptr_cmp_by_ref_count( void const*, void const* );

////////// extern variables ///////////////////////////////////////////////////

/// @cond DOXYGEN_IGNORE
/// Otherwise Doxygen generates two entries.

rb_tree_t tidy_include_set;
unsigned  tidy_includes_missing;
unsigned  tidy_includes_unnecessary;

/// @endcond

////////// local variables ////////////////////////////////////////////////////

#ifdef NEED_II_MATRIX
// All code guarded by NEED_II_MATRIX is code used for calculating an include-
// include matrix (see below).  As some point, I thought it was necessary, but,
// at least currently, it's apparently not.  But I didn't want to delete the
// code (even though it still would be in the git repo) just in case it turns
// out to be necessary after all at some point.

/**
 * Include-include matrix.
 *
 * @remarks
 * @parblock
 * <code>ii_matrix[</code>\e i<code>][</code>\e j<code>]</code> is
 * &gt; 0 only if include file \e i includes include file \e j.  The value
 * indicates the number of includes between them, i.e., 1 means \e i includes
 * \e j directly, 2 means \e i includes \e k that includes \e j, and so on.
 * Indicies are values of \ref tidy_include::instance_id "instance_id".
 *
 * The zeroth column is special in that if <code>ii_matrix[0][</code>\e
 * j<code>]</code> is &gt; 0, it means that \ref tidy_source_path includes
 * include file \e j.
 * @endparblock
 */
static ii_matrix_t  **ii_matrix;
#endif /* NEED_II_MATRIX */

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
static char const* get_cpp_header( char const *c_name,
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

#ifdef NEED_II_MATRIX                   /* See comment above ii_matrix def. */
/**
 * Initializes the \ref ii_matrix.
 *
 * @param N The size of the matrix, i.e., <code>[</code>\e N<code>][</code>\e
 * N<code>]</code>.
 *
 * @sa [Floyd-Warshall algorithm](https://en.wikipedia.org/wiki/Floyd–Warshall_algorithm)
 */
static void ii_matrix_init( unsigned N ) {
  ii_matrix = POINTER_CAST( ii_matrix_t**,
    matrix2d_new( sizeof(ii_matrix_t), alignof(ii_matrix_t), N, N )
  );
  for ( unsigned i = 0; i < N; ++i ) {
    for ( unsigned j = 0; j < N; ++j )
      ii_matrix[i][j] = 0;
  } // for

  CXFile const source_file = clang_getFile( tidy_tu, tidy_source_path );
  clang_getInclusions( tidy_tu, &ii_matrix_visitor, source_file );

  for ( unsigned k = 0; k < N; ++k ) {
    for ( unsigned i = 0; i < N; ++i ) {
      if ( ii_matrix[i][k] > 0 ) {
        for ( unsigned j = 0; j < N; ++j ) {
          if ( ii_matrix[k][j] > 0 ) {
            ii_matrix_t const new_dist = ii_matrix[i][k] + ii_matrix[k][j];
            if ( ii_matrix[i][j] == 0 || new_dist < ii_matrix[i][j] )
              ii_matrix[i][j] = new_dist;
          }
        } // for j
      }
    } // for i
  } // for k
}

/**
 * Visits each file included.
 *
 * @param included_file The file being included.
 * @param inclusion_stack The source locations of includes that lead up to \a
 * included_file being included.
 * @param inclusion_len The length of \a inclusion_stack.
 * @param data The `CXFile` for tidy_source_path.
 */
static void ii_matrix_visitor( CXFile included_file,
                               CXSourceLocation *inclusion_stack,
                               unsigned inclusion_len, CXClientData data ) {
  assert( data != NULL );

  if ( included_file == NULL || inclusion_len == 0 )
    return;
  tidy_include *const included = include_find_by_File( included_file );
  if ( included == NULL )
    return;

  CXFile includer_file;
  clang_getFileLocation(
    inclusion_stack[0],
    &includer_file, /*line=*/NULL, /*column=*/NULL, /*offset=*/NULL
  );
  if ( includer_file == NULL )
    return;

  unsigned      includer_instance_id;
  CXFile const  source_file = data;

  if ( clang_File_isEqual( includer_file, source_file ) ) {
    includer_instance_id = 0;
  }
  else {
    tidy_include *const includer = include_find_by_File( includer_file );
    if ( includer == NULL )
      return;
    includer_instance_id = includer->instance_id;
  }

  ii_matrix[ includer_instance_id ][ included->instance_id ] = 1;
}
#endif /* NEED_II_MATRIX */

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
  if ( !tidy_is_cpp )
    goto skip;

  // Remaining cases are valid only for paths not in subdirectories.
  if ( strchr( included->rel_path, '/' ) != NULL )
    goto skip;

  char cpp_path[ PATH_MAX ];
  if ( get_cpp_header( included->rel_path, cpp_path ) != NULL ) {
    if ( strcmp( includer->rel_path, cpp_path ) == 0 ) {
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
      proxy = include_find_by_rel_path( cpp_path );
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
  bool printed_any = false;

  rb_iterator_t iter;
  rb_iterator_init( &iter, &tidy_include_set );

  for ( tidy_include const *include;
        (include = rb_iterator_next( &iter )) != NULL; ) {
    if ( include->proxy == NULL )
      continue;
    if ( include->is_proxy_explicit != want_explicit )
      continue;
    if ( false_set( &printed_any ) ) {
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

  if ( printed_any )
    verbose_printf( "\n" );
}

/**
 * Cleans-up set of included files.
 */
static void includes_cleanup( void ) {
#ifdef NEED_II_MATRIX                   /* See comment above ii_matrix def. */
  free( ii_matrix );
#endif /* NEED_II_MATRIX */
  rb_tree_cleanup(
    &tidy_include_set, POINTER_CAST( rb_free_fn_t, &tidy_include_cleanup )
  );
}

/**
 * Visits each `#include` directive in a translation unit to initialize \ref
 * tidy_include_set.
 *
 * @param cursor The cursor for the symbol in the AST being visited.
 * @param parent Not used.
 * @param data A pointer to a includes_init_visitor_data.
 * @return Always returns `CXChildVisit_Continue`.
 */
static enum CXChildVisitResult includes_init_visitor( CXCursor cursor,
                                                      CXCursor parent,
                                                      CXClientData data ) {
  (void)parent;
  assert( data != NULL );

  if ( clang_getCursorKind( cursor ) != CXCursor_InclusionDirective )
    goto skip;

  includes_init_visitor_data *const iivd = data;

  unsigned          include_line, include_col;
  CXSourceLocation  includer_loc = clang_getCursorLocation( cursor );
  CXFile const      included_file = clang_getIncludedFile( cursor );
  CXFile            includer_file;
  bool const        is_direct = clang_Location_isFromMainFile( includer_loc );

  clang_getSpellingLocation(
    includer_loc, &includer_file, &include_line, &include_col,
    /*offset=*/NULL
  );

  if ( included_file == NULL ) {
    CXString const    included_name_cxs = clang_getCursorSpelling( cursor );
    char const *const included_name = clang_getCString( included_name_cxs );
    CXString const    includer_name_cxs = clang_getFileName( includer_file );
    char const *const includer_name = clang_getCString( includer_name_cxs );

    print_file_error(
      includer_name, include_line, include_col,
      "\"%s\": %s (missing -I option?)\n",
      included_name, strerror( ENOENT )
    );

    clang_disposeString( included_name_cxs );
    clang_disposeString( includer_name_cxs );
    exit( EX_DATAERR );
  }

  tidy_include new_include = {
    .file_id = tidy_getFileUniqueID( included_file )
  };
  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &tidy_include_set, &new_include, sizeof new_include );
  tidy_include *const included = RB_DINT( rv_rbi.node );

  if ( rv_rbi.inserted ) {
    CXString const abs_path_cxs = tidy_File_getRealPathName( included_file );
    included->abs_path = check_strdup( clang_getCString( abs_path_cxs ) );
    clang_disposeString( abs_path_cxs );

    included->file     = included_file;
    included->is_local = path_is_local( included->abs_path );

    //
    // We don't call opt_include_paths_relativize( included->abs_path ) because
    // that would relativize the real path with symlinks, if any, resolved.
    // That might be surprising to the user if the linked-to file has a
    // different name.
    //
    // For example, on macOS, Readline is just a wrapper around Editline and
    // has this:
    //
    //      /usr/include/readline/
    //        readline.h -> ../editline/readline.h
    //
    // If the user includes it unnecessarily, it would print:
    //
    //      #include <editline/readline.h>  // DELETE LINE xx
    //
    // that the user didn't include with that name.  Therefore, relativize the
    // original CXFile's path.
    //
    included->rel_path = tidy_File_getRelativePath( included_file );

#ifdef NEED_II_MATRIX                   /* See comment above ii_matrix def. */
    included->instance_id = tidy_include_set.size;
#endif /* NEED_II_MATRIX */

    if ( !is_direct ) {
      included->includer = include_find_by_File( includer_file );
      assert( included->includer != NULL );
      included->depth = included->includer->depth + 1;
    }
    array_init( &included->lines, sizeof(unsigned) );
    rb_tree_init(
      // Use RB_DPTR to make nodes point to existing tidy_symbol objects in
      // symbol_set in symbols.c.
      &included->symbol_set, RB_DPTR,
      POINTER_CAST( rb_cmp_fn_t, &tidy_symbol_cmp )
    );
  }
  else if ( is_direct ) {
    //
    // This handles a case like:
    //
    //      "util.h"
    //        </usr/include/stdlib.h>
    //      ...
    //      </usr/include/stdlib.h>
    //
    // That is, a header was initially included indirectly, but then later
    // included directly.
    //
    included->depth = 0;
    included->includer = NULL;
  }
  else {
    tidy_include *const old_includer = included->includer;
    if ( old_includer != NULL ) {
      //
      // This handles a case like:
      //
      //      </usr/include/sys/wait.h>
      //        </usr/include/sys/signal.h> // #define SIGSTOP 17
      //      ...
      //      </usr/include/signal.h>
      //        </usr/include/sys/signal.h>
      //
      // That is, a standard header (e.g., sys/wait.h) includes a non-standard
      // header (e.g., sys/signal.h) so sys/signal.h's includer is set to
      // sys/wait.h.
      //
      // Later, the standard header version of the non-standard header (e.g.,
      // signal.h) is included that also includes the non-standard header
      // (sys/signal.h), so sys/signal.h's includer should be reset to be
      // signal.h because, later still, sys/signal's proxy will then be set to
      // be signal.h, not sys/wait.h, so signal.h will be considered the header
      // that defines SIGSTOP (which is correct) instead of sys/wait.h (which
      // is incorrect).
      //
      tidy_include *const includer = include_find_by_File( includer_file );
      if ( includer != old_includer &&
           strcmp( path_basename( included->rel_path ),
                   path_basename( includer->rel_path ) ) == 0 ) {
        included->includer = includer;
      }
    }
    goto done;
  }

  if ( (opt_verbose & TIDY_VERBOSE_INCLUDES) != 0 ) {
    if ( false_set( &iivd->verbose_printed ) )
      verbose_printf( "includes:\n" );

    char delims[2];
    include_get_delims( included, delims );

    verbose_printf(
      "  %2u%*s %c%s%c\n",
      included->depth,
      STATIC_CAST( int, included->depth * INCLUDE_VERBOSE_INDENT ), "",
      delims[0], included->abs_path, delims[1]
    );
  }

done:
  if ( !is_direct )
    goto skip;

  //
  // The file was directly included, so we need to add the line number on which
  // it was included to an array of such lines.  If the array ends up having
  // more than one line in it, then all but the first are duplicate includes.
  //
  if ( (!rv_rbi.inserted &&
       tidy_File_CompareByName( included_file, included->file ) != 0 ) ) {
    //
    // However, if the file wasn't inserted (because it's a duplicate by file
    // ID), but its original name is NOT the same, it means it was either a
    // symbolic or hard link to a file that was already included.  This should
    // NOT be considered a duplicate.
    //
    // For example, when using GNU Readline with history, one typically does:
    //
    //      #include <readline/readline.h>
    //      #include <readline/history.h>
    //
    // However, on macOS, Readline is just a wrapper around Editline and has
    // this:
    //
    //      /usr/include/readline/
    //        history.h -> ../editline/readline.h
    //        readline.h -> ../editline/readline.h
    //
    // Because they're symlink'd to the same file, they'll (of course) have the
    // same unique file ID, so one will be considered a duplicate of the other.
    // But the user used Readline correctly by including both readline.h and
    // history.h, so the user's code should NOT have one considered a duplicate
    // of the other.
    //
    goto skip;
  }

  *(unsigned*)array_push_back( &included->lines ) = include_line;

skip:
  return CXChildVisit_Continue;
}

/**
 * Visits each include file that was included.
 *
 * @param include The tidy_include to visit.
 * @param ipvd The includes_print_visitor_data to use.
 */
static void includes_print_visitor( tidy_include const *include,
                                    includes_print_visitor_data *ipvd ) {
  assert( include != NULL );
  assert( ipvd != NULL );

  bool const keep = include->handling == TIDY_HANDLE_KEEP;

  if ( keep && !opt_all_includes )
    return;

  print_group group;
  if ( include->is_local )
    group = PRINT_LOCAL;
  else if ( config_is_standard_include( include->rel_path ) )
    group = PRINT_STANDARD;
  else
    group = PRINT_3RD_PARTY;
  if ( group != ipvd->want_group )
    return;

  if ( (opt_verbose & TIDY_VERBOSE_SOURCE_FILE) != 0 &&
       false_set( &ipvd->printed_source_file ) ) {
    verbose_printf( "%s\n", tidy_source_path );
  }

  char       *comment = NULL;
  char        delims[2];
  bool        do_print_include = false;
  bool const  is_direct = include->depth == 0;
  bool        reset_opt_comment_style = false;
  char const *sgr_color = NULL;

  if ( include->is_needed || keep ) {
    if ( is_direct || keep ) {
      do_print_include = opt_all_includes;
    }
    else {
      sgr_color = sgr_include_add;
      do_print_include = true;
    }
    if ( do_print_include && opt_comment_style[0][0] != '\0' )
      comment = make_symbols_comment( include );
  }
  else if ( is_direct ) {
    if ( opt_comment_style[0][0] == '\0' ) {
      opt_comment_style[0] = "// ";
      reset_opt_comment_style = true;
    }
    unsigned const line = *(unsigned*)array_front_nc( &include->lines );
    check_asprintf( &comment, "DELETE LINE %u", line );
    sgr_color = sgr_include_del;
    do_print_include = true;
  }

  include_get_delims( include, delims );
  if ( do_print_include ) {
    if ( true_clear( &ipvd->print_blank_line ) )
      PUTC( '\n' );
    print_include( sgr_color, delims, include->rel_path, comment );
    ipvd->printed_any_includes = true;
  }

  if ( include->lines.len > 1 ) {
    if ( opt_comment_style[0][0] == '\0' ) {
      opt_comment_style[0] = "// ";
      reset_opt_comment_style = true;
    }
    if ( true_clear( &ipvd->print_blank_line ) )
      PUTC( '\n' );

    unsigned const first_line = *(unsigned*)array_front_nc( &include->lines );
    for ( unsigned i = 1; i < include->lines.len; ++i ) {
      free( comment );
      unsigned const line = *(unsigned*)array_at_nc( &include->lines, i );
      if ( include->is_needed ) {
        check_asprintf( &comment,
          "DELETE LINE %u (same as line %u)", line, first_line
        );
      }
      else {
        check_asprintf( &comment, "DELETE LINE %u", line );
      }
      print_include( sgr_include_del, delims, include->rel_path, comment );
    } // for

    ipvd->printed_any_includes = true;
  }

  free( comment );
  if ( reset_opt_comment_style )
    opt_comment_style[0] = "";
}

/**
 * Gets whether \a include is the associated header for the file currently
 * being tidied.
 *
 * @param include The include to check.
 * @param source_file_no_ext tidy_source_path but without its filename
 * extension.
 * @return Returns `true` only if \a include is the associated header for the
 * file currently being tidied.
 */
NODISCARD
static bool is_assoc_header( tidy_include const *include,
                             char const *source_file_no_ext ) {
  assert( include != NULL );
  assert( source_file_no_ext != NULL );

  if ( tidy_associated_header_rel_path != NULL )
    return strcmp( include->rel_path, tidy_associated_header_rel_path ) == 0;

  char const *const include_ext = path_ext( include->rel_path );
  if ( include_ext == NULL || include_ext[0] != 'h' )
    return false;

  char path_buf[ PATH_MAX ];
  char const *const include_no_ext = path_no_ext( include->rel_path, path_buf );
  //
  // If this include file's name matches the source file's (without
  // extensions), it's the .h associated with the .c, so sort this include file
  // first, e.g.:
  //
  //      // foo.c
  //      #include "foo.h"              // associated header sorted first
  //      #include "a.h"
  //      #include "b.h"
  //
  return strcmp( include_no_ext, source_file_no_ext ) == 0;
}

/**
 * For \a include, makes a comment containing a comma-separated list of the
 * symbols used.
 *
 * @param include The tidy_include to make the comment for.
 * @return Returns said comment.  The caller is responsible for freeing it.
 */
NODISCARD
static char* make_symbols_comment( tidy_include const *include ) {
  assert( include != NULL );

  size_t const fixed_len = opt_align_column +
    strlen( opt_comment_style[0] ) + strlen( opt_comment_style[1] );

  array_t symbols_array;
  array_init( &symbols_array, sizeof(tidy_symbol*) );
  array_reserve( &symbols_array, include->symbol_set.size );

  rb_iterator_t iter;
  rb_iterator_init( &iter, &include->symbol_set );
  for ( tidy_symbol const *sym; (sym = rb_iterator_next( &iter )) != NULL; )
    *(tidy_symbol const**)array_push_back( &symbols_array ) = sym;

  strbuf_t symbols_buf;
  strbuf_init( &symbols_buf );

  switch ( opt_comment_symbols ) {
    case TIDY_COM_SYM_ALPHA:
      // Array is already sorted alphabetically.
      break;
    case TIDY_COM_SYM_LENGTH:
      array_qsort( &symbols_array, &tidy_symbol_ptr_cmp_by_name_length );
      break;
    case TIDY_COM_SYM_MOST_USED:;
      // We could sort by ref_count descending as in TIDY_COM_SYM_REF_COUNT
      // then simply use only the zeroth element, but sorting is O(n log n),
      // whereas just iterating through the entire array is O(n).
      tidy_symbol const *most_ref_sym =
        *(tidy_symbol const**)array_at_nc( &symbols_array, 0 );
      for ( size_t i = 1; i < symbols_array.len; ++i ) {
        tidy_symbol const *const sym =
          *(tidy_symbol const**)array_at_nc( &symbols_array, i );
        if ( sym->ref_count > most_ref_sym->ref_count )
          most_ref_sym = sym;
      } // for
      strbuf_puts( &symbols_buf, most_ref_sym->name );
      goto done;
    case TIDY_COM_SYM_REF_COUNT:
      array_qsort( &symbols_array, &tidy_symbol_ptr_cmp_by_ref_count );
      break;
  } // switch

  bool comma = false;
  bool is_done = false;

  for ( size_t i = 0; !is_done && i < symbols_array.len; ++i ) {
    tidy_symbol const *const sym =
      *(tidy_symbol const**)array_at_nc( &symbols_array, i );
    char const   *sym_name = sym->name;
    size_t const  sym_name_len = strlen( sym_name );
    size_t        add_len = (comma ? STRLITLEN( ", " ) : 0) + sym_name_len;
    size_t        new_line_len = fixed_len + symbols_buf.len + add_len;

    if ( symbols_buf.len > 0 && new_line_len > opt_line_length ) {
      // The next symbol doesn't fit: try adding ", ..." instead.
      add_len = STRLITLEN( ", ..." );
      new_line_len = fixed_len + symbols_buf.len + add_len;
      if ( new_line_len > opt_line_length )
        break;                          // ", ..." didn't fit either
      sym_name = "...";
      is_done = true;
    }

    strbuf_sepsn_puts(
      &symbols_buf, ", ", STRLITLEN( ", " ), &comma, sym_name
    );
  } // for

done:
  array_cleanup( &symbols_array, /*free_fn=*/NULL );
  return strbuf_take( &symbols_buf );
}

/**
 * Gets whether \a include should be printed.
 *
 * @param include The tidy_include to check.
 * @return Returns `true` only if \a include should be printed.
 */
NODISCARD
static bool should_print_include( tidy_include const *include ) {
  assert( include != NULL );

  if ( include->handling == TIDY_HANDLE_ELIDE )
    return false;
  if ( include->lines.len > 1 )
    return true;

  if ( opt_all_includes &&
       (include->is_needed || include->handling == TIDY_HANDLE_KEEP) ) {
    return true;
  }

  bool const is_direct = include->depth == 0;
  if ( include->is_needed != is_direct )
    return true;

  return false;
}

/**
 * Given a file having either an absolute or relative path, gets its normalized
 * relative path.
 *
 * @param file The file to get the relative path for.
 * @return Returns the normalized, relative path of \a file.  The caller is
 * responsible for freeing it.
 */
NODISCARD
static char* tidy_File_getRelativePath( CXFile file ) {
  assert( file != NULL );

  CXString const    path_cxs = clang_getFileName( file );
  char const *const path = path_normalize( clang_getCString( path_cxs ) );

  clang_disposeString( path_cxs );

  // The string handling here is overly complicated:
  //
  //  + At this point, path is a string we're responsible for.
  //
  //  + opt_include_paths_relativize() returns a pointer within path.
  //
  //  + But at clean-up time, we would have to free the original path.
  //
  //  + Therefore, strdup the relativized path (the portion within path) and
  //    free the original path now.

  char *const rel_path = check_strdup( opt_include_paths_relativize( path ) );
  FREE( path );
  return rel_path;
}

/**
 * Cleans-up all memory associated with \a include but does _not_ free \a
 * include itself.
 *
 * @param include The tidy_include to clean up.  If NULL, does nothing.
 */
static void tidy_include_cleanup( tidy_include *include ) {
  if ( include == NULL )
    return;

  FREE( include->abs_path );
  FREE( include->rel_path );
  array_cleanup( &include->lines, /*free_fn=*/NULL );

  // Because the nodes point to existing tidy_symbol objects, use NULL.
  rb_tree_cleanup( &include->symbol_set, /*free_fn=*/NULL );
}

/**
 * Compares two \ref tidy_include objects by their unique file ID.
 *
 * @param i_include The first tidy_include.
 * @param j_include The second tidy_include.
 * @return Returns a number less than 0, 0, or greater than 0 if the file ID of
 * \a i_include is less than, equal to, or greater than the file ID of \a
 * j_include, respectively.
 */
NODISCARD
static int tidy_include_cmp_by_id( tidy_include const *i_include,
                                   tidy_include const *j_include ) {
  assert( i_include != NULL );
  assert( j_include != NULL );
  return tidy_FileUniqueID_cmp( &i_include->file_id, &j_include->file_id );
}

/**
 * Compares two \ref tidy_include objects by their relative paths for printing.
 *
 * @param pi_data A pointer to the the first tidy_include pointer.
 * @param pj_data A pointer to the second tidy_include pointer.
 * @return Returns a number less than 0, 0, or greater than 0 if the relative
 * path of \a *pi_data is less than, equal to, or greater than the relative
 * path of \a *pj_data, respectively.
 */
NODISCARD
static int tidy_include_cmp_for_print( void const *pi_data,
                                       void const *pj_data ) {
  assert( pi_data != NULL );
  assert( pj_data != NULL );

  tidy_include const *const i_include =
    *POINTER_CAST( tidy_include const**, pi_data );
  tidy_include const *const j_include =
    *POINTER_CAST( tidy_include const**, pj_data );

  if ( i_include->sort_rank < j_include->sort_rank )
    return -1;
  if ( i_include->sort_rank > j_include->sort_rank )
    return 1;
  return strcmp( i_include->rel_path, j_include->rel_path );
}

/**
 * Compares two \ref tidy_symbol objects by their name length.
 *
 * @param i_pp The first pointer to a `tidy_symbol*`.
 * @param j_pp The second pointer to a `tidy_symbol*`.
 * @return Returns a number less than 0, 0, or greater than 0 if the length of
 * the first symbol's name is less than, equal to, or greater than the length
 * of the second symbol's name, respectively.
 *
 * @sa tidy_symbol_ptr_cmp_by_ref_count()
 */
NODISCARD
static int tidy_symbol_ptr_cmp_by_name_length( void const *i_pp,
                                               void const *j_pp ) {
  assert( i_pp != NULL );
  assert( j_pp != NULL );

  tidy_symbol const *const i_sym = *POINTER_CAST( tidy_symbol const**, i_pp );
  tidy_symbol const *const j_sym = *POINTER_CAST( tidy_symbol const**, j_pp );

  int const cmp =
    STATIC_CAST( int, strlen( i_sym->name ) ) -
    STATIC_CAST( int, strlen( j_sym->name ) );

  return cmp != 0 ? cmp : strcmp( i_sym->name, j_sym->name );
}

/**
 * Compares two \ref tidy_symbol objects by their \ref tidy_symbol::ref_count
 * "reference count", descending.
 *
 * @param i_pp The first pointer to a `tidy_symbol*`.
 * @param j_pp The second pointer to a `tidy_symbol*`.
 * @return Returns a number less than 0, 0, or greater than 0 if the reference
 * count of the second symbol is less than, equal to, or greater than the
 * reference count of the first symbol, respectively.
 *
 * @sa tidy_symbol_ptr_cmp_by_name_length()
 */
NODISCARD
static int tidy_symbol_ptr_cmp_by_ref_count( void const *i_pp,
                                             void const *j_pp ) {
  assert( i_pp != NULL );
  assert( j_pp != NULL );

  tidy_symbol const *const i_sym = *POINTER_CAST( tidy_symbol const**, i_pp );
  tidy_symbol const *const j_sym = *POINTER_CAST( tidy_symbol const**, j_pp );

  int const cmp =                       // descending, so j_sym is first
    STATIC_CAST( int, j_sym->ref_count ) -
    STATIC_CAST( int, i_sym->ref_count );

  return cmp != 0 ? cmp : strcmp( i_sym->name, j_sym->name );
}

////////// extern functions ///////////////////////////////////////////////////

tidy_include* get_associated_header( void ) {
  static tidy_include *assoc_include;

  RUN_ONCE {
    char const *const ext = path_ext( tidy_source_path );
    if ( ext == NULL )
      return NULL;
    char const *const lang = get_ext_language( ext );
    if ( lang == NULL )
      return NULL;
    if ( tolower( ext[0] ) != 'c' )
      return NULL;

    char path_buf[ PATH_MAX ];
    char const *const source_path_no_ext =
      path_no_ext( tidy_source_path, path_buf );

    rb_iterator_t iter;
    rb_iterator_init( &iter, &tidy_include_set );
    for ( tidy_include *include;
          (include = rb_iterator_next( &iter )) != NULL; ) {
      if ( is_assoc_header( include, source_path_no_ext ) ) {
        assoc_include = include;
        break;
      }
    } // for
  }

  return assoc_include;
}

void implicit_proxies_init( void ) {
  ASSERT_RUN_ONCE();

  CXCursor const cursor = clang_getTranslationUnitCursor( tidy_tu );
  clang_visitChildren( cursor, &implicit_proxies_visitor, /*data=*/NULL );

  if ( (opt_verbose & TIDY_VERBOSE_PROXIES_EXPLICIT) != 0 )
    include_proxies_dump( /*want_explicit=*/true );
  if ( (opt_verbose & TIDY_VERBOSE_PROXIES_IMPLICIT) != 0 )
    include_proxies_dump( /*want_explicit=*/false );
}

tidy_include const* include_add_symbol( CXFile include_file,
                                        tidy_symbol *sym ) {
  assert( include_file != NULL );
  assert( sym != NULL );

  tidy_include *include = include_find_by_File( include_file );
  if ( include == NULL )
    return NULL;
  while ( include->proxy != NULL )
    include = include->proxy;
  PJL_DISCARD_RV( rb_tree_insert( &include->symbol_set, sym, 0 ) );
  include->is_needed = true;
  return include;
}

tidy_include* include_find_by_File( CXFile file ) {
  assert( file != NULL );

  tidy_include find_include = {
    .file_id = tidy_getFileUniqueID( file )
  };
  rb_node_t const *const found_rb =
    rb_tree_find( &tidy_include_set, &find_include );
  return found_rb != NULL ? RB_DINT( found_rb ) : NULL;
}

tidy_include* include_find_by_rel_path( char const *rel_path ) {
  assert( rel_path != NULL );
  assert( path_is_relative( rel_path ) );

  rb_iterator_t iter;
  size_t const  rel_path_len = strlen( rel_path );

  rb_iterator_init( &iter, &tidy_include_set );
  for ( tidy_include *include;
        (include = rb_iterator_next( &iter )) != NULL; ) {
    if ( !path_ends_with( include->abs_path, rel_path, rel_path_len ) )
      continue;
    for ( ; include->proxy != NULL; include = include->proxy ) {
      if ( !path_ends_with( include->proxy->abs_path, rel_path, rel_path_len ) )
        break;
    } // for
    return include;
  } // for

  return NULL;
}

void include_get_delims( tidy_include const *include, char delim[static 2] ) {
  assert( include != NULL );

  if ( include->is_local ) {
    delim[0] = '"';
    delim[1] = '"';
  }
  else {
    delim[0] = '<';
    delim[1] = '>';
  }
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

#ifdef NEED_II_MATRIX                   /* See comment above ii_matrix def. */
unsigned includes_include( tidy_include const *i_include,
                           tidy_include const *j_include ) {
  assert( j_include != NULL );
  unsigned const i = i_include != NULL ? i_include->instance_id : 0;
  return ii_matrix[ i ][ j_include->instance_id ];
}
#endif /* NEED_II_MATRIX */

void includes_init( void ) {
  ASSERT_RUN_ONCE();
  rb_tree_init(
    &tidy_include_set, RB_DINT,
    POINTER_CAST( rb_cmp_fn_t, &tidy_include_cmp_by_id )
  );
  ATEXIT( &includes_cleanup );

  includes_init_visitor_data iivd = { 0 };
  CXCursor cursor = clang_getTranslationUnitCursor( tidy_tu );
  clang_visitChildren( cursor, &includes_init_visitor, &iivd );
  if ( iivd.verbose_printed )
    verbose_printf( "\n" );
#ifdef NEED_II_MATRIX                   /* See comment above ii_matrix def. */
  ii_matrix_init( tidy_include_set.size + 1 );
#endif /* NEED_II_MATRIX */
}

void includes_print( void ) {
  array_t include_array;
  array_init( &include_array, sizeof(tidy_include*) );
  array_reserve( &include_array, tidy_include_set.size );

  tidy_include *include = get_associated_header();
  if ( include != NULL ) {
    include->is_needed = true;
    include->sort_rank = TIDY_SORT_ASSOCIATED;
  }

  rb_iterator_t iter;
  rb_iterator_init( &iter, &tidy_include_set );
  while ( (include = rb_iterator_next( &iter )) != NULL ) {
    if ( should_print_include( include ) ) {
      *(tidy_include const**)array_push_back( &include_array ) = include;
      if ( include->handling != TIDY_HANDLE_KEEP ) {
        if ( !include->is_needed )
          ++tidy_includes_unnecessary;
        else if ( include->depth > 0 )
          ++tidy_includes_missing;
      }
      if ( include->lines.len > 1 ) {
        tidy_includes_unnecessary +=
          STATIC_CAST( unsigned, include->lines.len ) - 1;
      }
    }
  } // while

  array_qsort( &include_array, &tidy_include_cmp_for_print );

  // Print local includes.
  includes_print_visitor_data ipvd = { 0 };
  for ( size_t i = 0; i < include_array.len; ++i ) {
    includes_print_visitor(
      *(tidy_include const**)array_at_nc( &include_array, i ), &ipvd
    );
  } // for

  // Print non-local, non-standard includes.
  if ( opt_all_includes && true_clear( &ipvd.printed_any_includes ) )
    ipvd.print_blank_line = true;
  ipvd.want_group = PRINT_3RD_PARTY;
  for ( size_t i = 0; i < include_array.len; ++i ) {
    includes_print_visitor(
      *(tidy_include const**)array_at_nc( &include_array, i ), &ipvd
    );
  } // for

  // Print standard includes.
  if ( opt_all_includes && true_clear( &ipvd.printed_any_includes ) )
    ipvd.print_blank_line = true;
  ipvd.want_group = PRINT_STANDARD;
  for ( size_t i = 0; i < include_array.len; ++i ) {
    includes_print_visitor(
      *(tidy_include const**)array_at_nc( &include_array, i ), &ipvd
    );
  } // for

  array_cleanup( &include_array, /*free_fn=*/NULL );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
