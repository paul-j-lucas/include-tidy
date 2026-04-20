/*
**      include-tidy -- #include tidier
**      src/includes.h
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

#ifndef include_tidy_includes_H
#define include_tidy_includes_H

/**
 * @file
 * Declares structures and functions for keeping track of files included.
 */

// local
#include "pjl_config.h"
#include "red_black.h"
#include "array.h"
#include "symbols.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <stdbool.h>
#include <stdint.h>

// libclang
#include <clang-c/Index.h>

/// @endcond

/**
 * @defgroup tidy-includes-group Include Files
 * Structures and functions for keeping track of files included.
 * @{
 */

////////// enumerations ///////////////////////////////////////////////////////

/**
 * Sorting "ranks" when includes are printed.
 *
 * @remarks In order for an include's \ref tidy_include::sort_rank "sort_rank"
 * to default to #TIDY_SORT_DEFAULT, it's value should be 0 since the value of
 * members whose designated initializers are omitted defaults to 0.  Given
 * that, the other values must be negative to sort before the default.
 */
enum tidy_include_sort_rank {
  TIDY_SORT_FIRST         = -2,         ///< The very first `#include`.
  TIDY_SORT_CORRESPONDING = -1,         ///< After first, but before default.
  TIDY_SORT_DEFAULT       =  0          ///< Default sort rank.
};

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct tidy_include tidy_include;

////////// structures /////////////////////////////////////////////////////////

/**
 * A file that was included.
 */
struct tidy_include {
  CXFile          file;                 ///< File that was included.
  CXFileUniqueID  file_id;              ///< Unique file ID.
  char const     *abs_path;             ///< Absolute path of \a file.
  char const     *rel_path;             ///< Relative path of \a file.
  tidy_include   *includer;             ///< Include including this, if any.
  tidy_include   *proxy;                ///< Proxy include, if any.
  unsigned        depth;                ///< Include depth.
  array_t         lines;                ///< Line number(s) included from.
  bool            is_local;             ///< Local include file?
  bool            is_needed;            ///< Include needed?
  bool            is_proxy_explicit;    ///< Was \ref proxy explicitly added?
  int8_t          sort_rank;            ///< Sorting rank.
  rb_tree_t       symbol_set;           ///< Symbols referenced from this file.
};

////////// extern variables ///////////////////////////////////////////////////

/**
 * Number of missing include files.
 *
 * @sa tidy_includes_unnecessary
 */
extern unsigned tidy_includes_missing;

/**
 * Number of unnecessary include files.
 *
 * @sa tidy_includes_missing
 */
extern unsigned tidy_includes_unnecessary;

////////// extern functions ///////////////////////////////////////////////////

/**
 * Adds \a sym to the set of symbols that are used in the file being tidied and
 * declared in \a include_file.
 *
 * @param include_file The file that declares \a sym.
 * @param sym The symbol that is used.
 * @return Returns the include file is was added to or NULL for none.
 */
NODISCARD
tidy_include const* include_add_symbol( CXFile include_file, tidy_symbol *sym );

/**
 * Given a relative path to an include file, e.g.: `clang-c/Index.h`, gets its
 * corresponding include.
 *
 * @param rel_path The relative path of the include file to find.
 * @return Returns its corresponding tidy_include or NULL if not found.
 */
NODISCARD
tidy_include* include_find_by_rel_path( char const *rel_path );

/**
 * Checks whether adding a proxy from \a from_include to \a to_include would
 * cause a cycle.
 *
 * @param from_include The tidy_include to start from.
 * @param to_include The tidy_include to end at.
 * @return Returns `true` only if adding a proxy would cause a cycle.
 */
NODISCARD
bool include_proxy_would_cycle( tidy_include const *from_include,
                                tidy_include const *to_include );

/**
 * Initializes the set of files included in the given translation unit.
 *
 * @param tu The translation unit to use.
 *
 * @sa includes_init_implicit_proxies()
 */
void includes_init( CXTranslationUnit tu );

/**
 * Initializes the implicit include proxies for the given translation unit.
 *
 * @param tu The translation unit to use.
 *
 * @sa includes_init()
 */
void includes_init_implicit_proxies( CXTranslationUnit tu );

/**
 * Prints include files.
 */
void includes_print( void );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tidy_includes_H */
/* vim:set et sw=2 ts=2: */
