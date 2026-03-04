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

// libclang
#include <clang-c/Index.h>

///////////////////////////////////////////////////////////////////////////////

/**
 * A file that was included.
 */
struct tidy_include_file {
  CXFile    file;                       ///< File that was included.
  unsigned  count;                      ///< Number of times included.
  unsigned  depth;                      ///< "Depth" of include.
  unsigned  line;                       ///< Line included from.
};
typedef struct tidy_include_file tidy_include_file;

////////// extern functions ///////////////////////////////////////////////////

/**
 * Initializes the set of files included in the given translation unit.
 *
 * @param tu The translation unit to use.
 */
void includes_init( CXTranslationUnit tu );

///////////////////////////////////////////////////////////////////////////////

#endif /* include_tidy_includes_H */
/* vim:set et sw=2 ts=2: */
