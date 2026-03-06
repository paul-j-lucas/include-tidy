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

// local
#include "pjl_config.h"

// libclang
#include <clang-c/Index.h>

// standard
#include <stdbool.h>

///////////////////////////////////////////////////////////////////////////////

/**
 * A file that was included.
 */
struct tidy_include {
  CXFile    file;                       ///< File that was included.
  unsigned  count;                      ///< Number of times included.
  unsigned  depth;                      ///< "Depth" of include.
  unsigned  line;                       ///< Line included from.
  bool      is_needed;                  ///< Is this include needed?
};
typedef struct tidy_include tidy_include;

////////// extern functions ///////////////////////////////////////////////////

/**
 * Attempts to find \a file among the set of files included.
 *
 * @param file The file to find.
 * @return Returns the corresponding tidy_include if found or NULL if not.
 */
NODISCARD
tidy_include* include_find( CXFile file );

/**
 * Initializes the set of files included in the given translation unit.
 *
 * @param tu The translation unit to use.
 */
void includes_init( CXTranslationUnit tu );

/**
 * Print unneeded include files.
 */
void includes_print_unneeded( void );

///////////////////////////////////////////////////////////////////////////////

#endif /* include_tidy_includes_H */
/* vim:set et sw=2 ts=2: */
