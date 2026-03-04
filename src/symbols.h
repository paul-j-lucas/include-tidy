/*
**      include-tidy -- #include tidier
**      src/symbols.h
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

#ifndef include_tidy_symbols_H
#define include_tidy_symbols_H

// libclang
#include <clang-c/Index.h>

///////////////////////////////////////////////////////////////////////////////

/**
 * TODO
 */
struct tidy_symbol {
  CXString  name;                       ///< Symbol name.
  CXFile    decl_file;                  ///< File declared in.
  unsigned  decl_line;                  ///< Line declared on.
};
typedef struct tidy_symbol tidy_symbol;

////////// extern functions ///////////////////////////////////////////////////

/**
 * Initializes the internal set of all symbols from the given translation unit.
 */
void symbols_init( CXTranslationUnit tu );

///////////////////////////////////////////////////////////////////////////////

#endif /* include_tidy_symbols_H */
/* vim:set et sw=2 ts=2: */
