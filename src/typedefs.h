/*
**      include-tidy -- #include tidier
**      src/typedefs.h
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

#ifndef include_tidy_typedef_H
#define include_tidy_typedef_H

/**
 * @file
 * Defines structures and functions for keeping track of `typedef`s in the
 * translation unit.
 */

// local
#include "pjl_config.h"

/// @cond DOXYGEN_IGNORE

// libclang
#include <clang-c/Index.h>

/// @endcond

/**
 * @defgroup tidy-typedefs-group Type Aliases
 * Structures and functions for keeping track of `typedef`s in the translation
 * unit.
 * @{
 */

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct tidy_typedef tidy_typedef;

////////// structs ////////////////////////////////////////////////////////////

/**
 * Maps a cursor for either a libclang `TypedefDecl` or a `TypeAliasDecl` to
 * its scoped alias name.
 *
 * @remarks
 * @parblock
 * It's necessary to keep a map of cursors for type aliases to their "pretty"
 * scoped alias names.
 * @endparblock
 *
 * @par Example
 * @parblock
 * Given:
 *
 *      namespace std {
 *        // ...
 *        using ostream = basic_ostream<char>;
 *        // ...
 *      }
 *
 * the \ref type_cursor is the entire `using` declaration and \ref alias_name
 * is `"std::ostream"`.  This mapping is needed to include the "pretty" names
 * in include comments.
 *
 * If the file being tidied uses `std::ostream` like:
 *
 *      void f( std::ostream& );
 *
 * then the symbol in the comment will be `std::ostream` and not
 * `std::basic_ostream`:
 *
 *      #include <ostream>              // std::ostream
 *
 * @endparblock
 */
struct tidy_typedef {
  CXCursor    type_cursor;              ///< `TypedefDecl` or `TypeAliasDecl`.
  char const *alias_name;               ///< Scoped alias name.
};

////////// extern functions ///////////////////////////////////////////////////

/**
 * Adds either a libclang `TypedefDecl` or `TypeAliasDecl` to a global map
 * where \a cursor is the key and its scoped alias name is its value.
 *
 * @param cursor The type cursor to add.
 *
 * @sa typedef_find()
 */
void typedef_add( CXCursor cursor );

/**
 * Attempts to find the type \a cursor in the global map of `typedef`s.
 *
 * @param cursor The type cursor to find.
 * @return Returns a pointer to the corresponding tidy_typedef or NULL if not
 * found.
 *
 * @sa typedef_add()
 */
NODISCARD
tidy_typedef const* typedef_find( CXCursor cursor );

/**
 * Initializes the internal map of all `typedef`s in the translation unit.
 */
void typedefs_init( void );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tidy_typedef_H */
/* vim:set et sw=2 ts=2: */
