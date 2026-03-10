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
#include "symbols.h"

// libclang
#include <clang-c/Index.h>

// standard
#include <stdbool.h>

////////// extern functions ///////////////////////////////////////////////////

/**
 * Adds \a sym to the set of symbols that are used in the file being tidied and
 * declared in \a include_file.
 *
 * @param include_file The file that declares \a sym.
 * @param sym The symbol that is used.
 * @return Returns `true` only if the symbol was added.
 */
bool include_add_symbol( CXFile include_file, tidy_symbol *sym );

/**
 * Initializes the set of files included in the given translation unit.
 *
 * @param tu The translation unit to use.
 */
void includes_init( CXTranslationUnit tu );

/**
 * Prints include files.
 */
void includes_print( void );

///////////////////////////////////////////////////////////////////////////////

#endif /* include_tidy_includes_H */
/* vim:set et sw=2 ts=2: */
