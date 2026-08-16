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

/**
 * @file
 * Declares structures and functions for keeping track of symbols referenced.
 */

// local
#include "pjl_config.h"
#include "hash_table.h"

/**
 * @defgroup tidy-symbols-group Symbols
 * Structures and functions for keeping track of symbols referenced.
 * @{
 */

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct tidy_symbol tidy_symbol;

////////// structs ////////////////////////////////////////////////////////////

/**
 * A symbol used in a translation unit.
 */
struct tidy_symbol {
  /**
   * The symbol name without signature (for functions or operators) or template
   * parameters (for templates) used in `#include` comments.
   *
   * @note In C++, this is not guaranteed to be unique due to overloaded
   * functions or specialized templates.
   */
  char const *name;

  /**
   * The symbol name with signature (for functions or operators) or template
   * parameters (for templates) used as a unique key.
   */
  char const *name_key;

  unsigned    ref_count;                ///< Number of times referenced.
};

////////// extern functions ///////////////////////////////////////////////////

/**
 * Initializes the internal set of all symbols in the translation unit.
 */
void symbols_init( void );

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

/**
 * Calculates the hash of \a sym.
 *
 * @param sym The tidy_symbol to calculate the hash for.
 * @return Returns said hash.
 */
NODISCARD
ht_hash_val_t tidy_symbol_hash( tidy_symbol const *sym );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tidy_symbols_H */
/* vim:set et sw=2 ts=2: */
