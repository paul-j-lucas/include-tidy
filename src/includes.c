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
 * Defines functions for keeping track of files included.
 */

// local
#include "pjl_config.h"
#include "includes.h"
#include "array.h"
#include "clang_util.h"
#include "color.h"
#include "config_file.h"
#include "options.h"
#include "print.h"
#include "red_black.h"
#include "strbuf.h"
#include "symbols.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// libclang
#include <clang-c/Index.h>

// standard
#include <assert.h>
#include <errno.h>
#include <limits.h>                     /* for PATH_MAX */
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

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct includes_init_visitor_data   includes_init_visitor_data;
typedef struct includes_print_visitor_data  includes_print_visitor_data;

////////// structures /////////////////////////////////////////////////////////

/**
 * Additional data for includes_print_visitor().
 */
struct includes_print_visitor_data {
  bool  print_blank_line;               ///< Print a blank line?
  bool  print_local;                    ///< Print local includes?
  bool  print_standard;                 ///< Print standard includes?
  bool  printed_any_includes;           ///< Print any includes?
  bool  printed_source_file;            ///< Printed source file name?
};

/**
 * Additional data passed to includes_init_visitor().
 */
struct includes_init_visitor_data {
  bool  verbose_printed;                ///< Printed any verbose output?
};

////////// local functions ////////////////////////////////////////////////////

NODISCARD
static tidy_include*  include_find_by_CXFile( CXFile );

NODISCARD
static bool           is_local_include( char const* );

NODISCARD
static char*          make_symbols_used_comment( tidy_include const* );

NODISCARD
char const*           tidy_File_getRelativePath( CXFile );

static void           tidy_include_cleanup( tidy_include* );

////////// extern variables ///////////////////////////////////////////////////

unsigned tidy_includes_missing;         ///< Number of includes missing.
unsigned tidy_includes_unnecessary;     ///< Number of includes unnecessary.

////////// local variables ////////////////////////////////////////////////////

static rb_tree_t  include_set;          ///< Set of included files.

////////// local functions ////////////////////////////////////////////////////

/**
 * Gets the include delimiters, either local or system, to use.
 *
 * @param is_local `true` only if the include file is local.
 * @param delim The 2-element array to receive the delimiters.
 */
static void get_include_delims( bool is_local, char delim[static 2] ) {
  if ( is_local ) {
    delim[0] = '"';
    delim[1] = '"';
  }
  else {
    delim[0] = '<';
    delim[1] = '>';
  }
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
    goto done;

  CXFile const included_file = clang_getIncludedFile( cursor );
  assert( included_file != NULL );
  tidy_include *const included = include_find_by_CXFile( included_file );
  assert( included != NULL );

  if ( included->proxy != NULL )
    goto done;
  if ( included->depth == 0 )           // directly included: no proxy
    goto done;
  if ( included->is_local )             // only non-local can have a proxy
    goto done;

  tidy_include *const includer = included->includer;
  assert( includer != NULL );           // since directly included
  if ( includer->is_local )             // only non-local can be a proxy
    goto done;

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
      // include the real standard one.  The local header should be a proxy
      // for the real one.
      //
      strcmp( base_name( included->rel_path ),
              base_name( includer->rel_path ) ) == 0 ) {
    included->proxy = includer;
  }

done:
  return CXChildVisit_Continue;
}

/**
 * Attempts to find \a file by its unique file ID among the set of files
 * included.
 *
 * @param file The file to find.
 * @return Returns the corresponding tidy_include if found or NULL if not.
 */
NODISCARD
static tidy_include* include_find_by_CXFile( CXFile file ) {
  assert( file != NULL );

  tidy_include find_include = {
    .file_id = tidy_getFileUniqueID( file )
  };
  rb_node_t const *const found_rb = rb_tree_find( &include_set, &find_include );
  return found_rb != NULL ? RB_DINT( found_rb ) : NULL;
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
  rb_iterator_init( &include_set, &iter );

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
    verbose_printf(
      "  \"%s\" -> \"%s\"\n",
      include->abs_path, include->proxy->abs_path
    );
  } // for

  if ( printed_any )
    verbose_printf( "\n" );
}

/**
 * Cleans-up set of included files.
 */
static void includes_cleanup( void ) {
  rb_tree_cleanup(
    &include_set, POINTER_CAST( rb_free_fn_t, &tidy_include_cleanup )
  );
}

/**
 * Visits each `#include` directive in a translation unit to initialize \ref
 * include_set.
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

  includes_init_visitor_data *const iivd =
    POINTER_CAST( includes_init_visitor_data*, data );

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

    print_error(
      path_no_dot_slash( includer_name ), include_line, include_col,
      "\"%s\": %s (missing -I option?)\n",
      included_name, strerror( ENOENT )
    );
    exit( EX_DATAERR );
  }

  tidy_include new_include = {
    .file_id = tidy_getFileUniqueID( included_file )
  };
  rb_insert_rv_t const rv_rbi =
    rb_tree_insert( &include_set, &new_include, sizeof new_include );
  tidy_include *const included = RB_DINT( rv_rbi.node );

  if ( rv_rbi.inserted ) {
    CXString const abs_path_cxs = tidy_File_getRealPathName( included_file );
    included->abs_path = check_strdup( clang_getCString( abs_path_cxs ) );
    clang_disposeString( abs_path_cxs );

    included->file     = included_file;
    included->is_local = is_local_include( included->abs_path );

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
    //      #include <editline/readline.h>  // DELETE line xxxx
    //
    // that the user didn't include with that name.  Therefore, relativize the
    // original CXFile's path.
    //
    included->rel_path = tidy_File_getRelativePath( included_file );

    if ( !is_direct ) {
      included->includer = include_find_by_CXFile( includer_file );
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
      // a header (e.g., sys/signal.h) so sys/signal.h's includer is set to
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
      tidy_include *const includer = include_find_by_CXFile( includer_file );
      if ( includer != old_includer &&
           strcmp( base_name( included->rel_path ),
                   base_name( includer->rel_path ) ) == 0 ) {
        included->includer = includer;
      }
    }
    goto done;
  }

  if ( (opt_verbose & TIDY_VERBOSE_INCLUDES) != 0 ) {
    if ( false_set( &iivd->verbose_printed ) )
      verbose_printf( "includes:\n" );

    char inc_delim[2];
    get_include_delims( included->is_local, inc_delim );

    verbose_printf(
      "  %2u%*s %c%s%c\n",
      included->depth,
      STATIC_CAST( int, included->depth * INCLUDE_VERBOSE_INDENT ), "",
      inc_delim[0], included->abs_path, inc_delim[1]
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
       tidy_CXFile_cmp_by_name( included_file, included->file ) != 0 ) ) {
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
 * @param node_data The tidy_include to visit.
 * @param visit_data Contains a `bool` specifying whether to print only local
 * includes.
 * @return Always returns `false` (keep visiting).
 */
NODISCARD
static bool includes_print_visitor( void *node_data, void *visit_data ) {
  assert( node_data != NULL );
  tidy_include const *const include = node_data;
  includes_print_visitor_data *const ipvd = visit_data;

  if ( include->keep && !opt_all_includes )
    goto skip;
  if ( ipvd->print_local != include->is_local )
    goto skip;
  if ( ipvd->print_standard != config_is_standard_include( include->rel_path ) )
    goto skip;

  if ( (opt_verbose & TIDY_VERBOSE_SOURCE_FILE) != 0 &&
       false_set( &ipvd->printed_source_file ) ) {
    verbose_printf( "%s\n", arg_source_path );
  }

  char       *comment = NULL;
  bool        do_print_include = false;
  char        inc_delim[2];
  bool const  is_direct = include->depth == 0;
  bool        reset_opt_comment_style = false;
  char const *sgr_color = NULL;

  if ( include->is_needed || include->keep ) {
    if ( is_direct || include->keep ) {
      do_print_include = opt_all_includes;
    }
    else {
      sgr_color = sgr_include_add;
      do_print_include = true;
    }
    if ( do_print_include && opt_comment_style[0][0] != '\0' )
      comment = make_symbols_used_comment( include );
  }
  else if ( is_direct ) {
    if ( opt_comment_style[0][0] == '\0' ) {
      opt_comment_style[0] = "// ";
      reset_opt_comment_style = true;
    }
    unsigned const line = *(unsigned*)array_at( &include->lines, 0 );
    check_asprintf( &comment, "DELETE LINE %u", line );
    sgr_color = sgr_include_del;
    do_print_include = true;
  }

  get_include_delims( include->is_local, inc_delim );
  if ( do_print_include ) {
    if ( true_clear( &ipvd->print_blank_line ) )
      PUTC( '\n' );
    print_include( sgr_color, inc_delim, include->rel_path, comment );
    ipvd->printed_any_includes = true;
  }

  if ( include->lines.len > 1 ) {
    if ( opt_comment_style[0][0] == '\0' ) {
      opt_comment_style[0] = "// ";
      reset_opt_comment_style = true;
    }
    if ( true_clear( &ipvd->print_blank_line ) )
      PUTC( '\n' );

    unsigned const line_first = *(unsigned*)array_at( &include->lines, 0 );
    for ( unsigned i = 1; i < include->lines.len; ++i ) {
      free( comment );
      unsigned const line = *(unsigned*)array_at_nocheck( &include->lines, i );
      if ( include->is_needed ) {
        check_asprintf( &comment,
          "DELETE line %u (same as line %u)", line, line_first
        );
      }
      else {
        check_asprintf( &comment, "DELETE line %u", line );
      }
      print_include( sgr_include_del, inc_delim, include->rel_path, comment );
    } // for

    ipvd->printed_any_includes = true;
  }

  free( comment );
  if ( reset_opt_comment_style )
    opt_comment_style[0] = "";

skip:
  return false;
}

/**
 * Gets whether \a include is the associated header for the file currently
 * being tidied.
 *
 * @param include The include to check.
 * @param assoc_file_name The file name associated with the file being tidied,
 * if any.
 * @param source_file_no_ext arg_source_path but without its filename
 * extension.
 * @return Returns `true` only if \a include is the associated header for the
 * file currently being tidied.
 */
static bool is_assoc_header( tidy_include const *include,
                             char const *assoc_file_name,
                             char const *source_file_no_ext ) {
  assert( include != NULL );
  assert( source_file_no_ext != NULL );

  if ( assoc_file_name != NULL )
    return strcmp( include->rel_path, assoc_file_name ) == 0;

  char const *const include_ext = path_ext( include->rel_path );
  if ( include_ext == NULL || include_ext[0] != 'h' )
    return false;

  char path_buf[ PATH_MAX ];
  char const *const include_no_ext = path_no_ext( include->rel_path, path_buf );
  //
  // If this include file's name matches the source file's (without
  // extensions), it's the .h corresponding to the .c, so sort the this
  // include file first, e.g.:
  //
  //      // foo.c
  //      #include "foo.h"      // corresponding header sorted first
  //      #include "a.h"
  //      #include "b.h"
  //
  return strcmp( include_no_ext, source_file_no_ext ) == 0;
}

/**
 * Gets whether \a abs_path refers to a local include file (as opposed to a
 * system include file).
 *
 * @param abs_path The absolute path of an include file.
 * @return Returns `true` only if \a abs_path refers to a local include file.
 */
static bool is_local_include( char const *abs_path ) {
  assert( abs_path != NULL );
  assert( abs_path[0] == '/' );

  size_t cwd_path_len;
  char const *const cwd_path = get_cwd( &cwd_path_len );
  return strncmp( abs_path, cwd_path, cwd_path_len ) == 0;
}

/**
 * For \a include, makes a comment containing a comma-separated list of the
 * symbols used.
 *
 * @param include The tidy_include to make the comment for.
 * @return Returns said comment.  The caller is responsible for freeing it.
 */
static char* make_symbols_used_comment( tidy_include const *include ) {
  assert( include != NULL );

  size_t const fixed_len = opt_align_column +
    strlen( opt_comment_style[0] ) + strlen( opt_comment_style[1] );

  bool          comma = false;
  bool          done = false;
  rb_iterator_t iter;
  strbuf_t      symbols_buf;

  strbuf_init( &symbols_buf );
  rb_iterator_init( &include->symbol_set, &iter );

  for ( tidy_symbol const *sym;
        !done && (sym = rb_iterator_next( &iter )) != NULL; ) {
    char const   *sym_name = sym->name;
    size_t const  sym_name_len = strlen( sym_name );
    size_t        add_len = (comma ? STRLITLEN( ", " ) : 0) + sym_name_len;
    size_t        new_line_len = fixed_len + symbols_buf.len + add_len;

    if ( new_line_len > opt_line_length ) {
      // The next symbol doesn't fit: try adding ", ..." instead.
      add_len = STRLITLEN( ", ..." );
      new_line_len = fixed_len + symbols_buf.len + add_len;
      if ( new_line_len > opt_line_length )
        break;                          // ", ..." didn't fit either
      sym_name = "...";
      done = true;
    }

    strbuf_sepsn_puts(
      &symbols_buf, ", ", STRLITLEN( ", " ), &comma, sym_name
    );
  } // for

  return strbuf_take( &symbols_buf );
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
char const* tidy_File_getRelativePath( CXFile file ) {
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

  char const *const rel_path =
    check_strdup( opt_include_paths_relativize( path ) );
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
  return tidy_CXFileUniqueID_cmp( &i_include->file_id, &j_include->file_id );
}

/**
 * Compares two \ref tidy_include objects by their relative paths for printing.
 *
 * @param i_include The first tidy_include.
 * @param j_include The second tidy_include.
 * @return Returns a number less than 0, 0, or greater than 0 if the relative
 * path of \a i_include is less than, equal to, or greater than the relative
 * path of \a j_include, respectively.
 */
NODISCARD
static int tidy_include_cmp_for_print( tidy_include const *i_include,
                                       tidy_include const *j_include ) {
  assert( i_include != NULL );
  assert( j_include != NULL );
  if ( i_include->sort_rank < j_include->sort_rank )
    return -1;
  if ( i_include->sort_rank > j_include->sort_rank )
    return 1;
  return strcmp( i_include->rel_path, j_include->rel_path );
}

////////// extern functions ///////////////////////////////////////////////////

tidy_include const* include_add_symbol( CXFile include_file,
                                        tidy_symbol *sym ) {
  assert( include_file != NULL );
  assert( sym != NULL );

  tidy_include *include = include_find_by_CXFile( include_file );
  if ( include == NULL )
    return NULL;
  while ( include->proxy != NULL )
    include = include->proxy;
  include->is_needed = true;
  PJL_DISCARD_RV( rb_tree_insert( &include->symbol_set, sym, sizeof *sym ) );
  return include;
}

tidy_include* include_find_by_rel_path( char const *rel_path ) {
  assert( rel_path != NULL );
  assert( path_is_relative( rel_path ) );

  bool          found = false;
  rb_iterator_t iter;
  size_t const  rel_path_len = strlen( rel_path );

  rb_iterator_init( &include_set, &iter );
  for ( tidy_include *include;
        (include = rb_iterator_next( &iter )) != NULL; ) {
    size_t const abs_path_len = strlen( include->abs_path );

    if ( rel_path_len <= abs_path_len ) {
      char const *const suffix =
        include->abs_path + (abs_path_len - rel_path_len);
      found = strcmp( rel_path, suffix ) == 0 &&
              (suffix == include->abs_path || suffix[-1] == '/');
    }

    if ( found )
      return include;
  } // for

  return NULL;
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

void includes_init( CXTranslationUnit tu ) {
  ASSERT_RUN_ONCE();
  rb_tree_init(
    &include_set, RB_DINT, POINTER_CAST( rb_cmp_fn_t, &tidy_include_cmp_by_id )
  );
  ATEXIT( &includes_cleanup );

  includes_init_visitor_data iivd = { 0 };
  CXCursor cursor = clang_getTranslationUnitCursor( tu );
  clang_visitChildren( cursor, &includes_init_visitor, &iivd );
  if ( iivd.verbose_printed )
    verbose_printf( "\n" );
}

void includes_init_implicit_proxies( CXTranslationUnit tu ) {
  ASSERT_RUN_ONCE();

  CXCursor const cursor = clang_getTranslationUnitCursor( tu );
  clang_visitChildren( cursor, &implicit_proxies_visitor, /*data=*/NULL );

  if ( (opt_verbose & TIDY_VERBOSE_PROXIES_EXPLICIT) != 0 )
    include_proxies_dump( /*want_explicit=*/true );
  if ( (opt_verbose & TIDY_VERBOSE_PROXIES_IMPLICIT) != 0 )
    include_proxies_dump( /*want_explicit=*/false );
}

void includes_print( void ) {
  rb_tree_t include_set_by_rel_path;
  rb_tree_init(
    // Use RB_DPTR to make nodes point to existing tidy_include objects in
    // include_set.
    &include_set_by_rel_path, RB_DPTR,
    POINTER_CAST( rb_cmp_fn_t, &tidy_include_cmp_for_print )
  );

  char path_buf[ PATH_MAX ];
  char const *const source_base_name = base_name( arg_source_path );
  char const *const assoc_file_name =
    config_get_assoc_header( source_base_name );
  char const *const source_file_no_ext =
    path_no_ext( source_base_name, path_buf );

  rb_iterator_t iter;
  rb_iterator_init( &include_set, &iter );
  for ( tidy_include *include;
        (include = rb_iterator_next( &iter )) != NULL; ) {
    bool const is_direct = include->depth == 0;
    if ( (include->is_needed ? (!is_direct || opt_all_includes) : is_direct) ||
         (include->keep && opt_all_includes) ||
         include->lines.len > 1 ) {
      if ( is_assoc_header( include, assoc_file_name, source_file_no_ext ) )
        include->sort_rank = TIDY_SORT_ASSOCIATED;
      PJL_DISCARD_RV( rb_tree_insert( &include_set_by_rel_path, include, 0 ) );
      if ( !include->keep ) {
        if ( !include->is_needed || include->lines.len > 1 )
          ++tidy_includes_unnecessary;
        else if ( !is_direct )
          ++tidy_includes_missing;
      }
    }
  } // for

  includes_print_visitor_data ipvd = { .print_local = true };
  rb_tree_visit( &include_set_by_rel_path, &includes_print_visitor, &ipvd );

  if ( opt_all_includes && true_clear( &ipvd.printed_any_includes ) )
    ipvd.print_blank_line = true;
  ipvd.print_local = !ipvd.print_local;
  rb_tree_visit( &include_set_by_rel_path, &includes_print_visitor, &ipvd );

  if ( opt_all_includes && true_clear( &ipvd.printed_any_includes ) )
    ipvd.print_blank_line = true;
  ipvd.print_standard = true;
  rb_tree_visit( &include_set_by_rel_path, &includes_print_visitor, &ipvd );

  // Because the nodes point to existing tidy_include objects, use NULL.
  rb_tree_cleanup( &include_set_by_rel_path, /*free_fn=*/NULL );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
