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

// local
#include "pjl_config.h"

// libclang
#include <clang-c/Index.h>

///////////////////////////////////////////////////////////////////////////////

/**
 * A symbol declared in a translation unit.
 */
struct tidy_symbol {
  CXString  name_cxs;                   ///< Symbol name.
};
typedef struct tidy_symbol tidy_symbol;

////////// extern functions ///////////////////////////////////////////////////

/**
 * Initializes the internal set of all symbols from the given translation unit.
 *
 * @param tu The translation unit to use.
 */
void symbols_init( CXTranslationUnit tu );

/**
 * Compares two \ref tidy_symbol objects.
 *
 * @param i_sym The first symbol.
 * @param j_sym The second symbol.
 * @return Returns a number less than 0, 0, or greater than 0 if the name of \a
 * i_sym is less than, equal to, or greater than the name of \a j_sym,
 * respectively.
 */
NODISCARD
int tidy_symbol_cmp( tidy_symbol const *i_sym, tidy_symbol const *j_sym );

///////////////////////////////////////////////////////////////////////////////

#endif /* include_tidy_symbols_H */
/* vim:set et sw=2 ts=2: */
