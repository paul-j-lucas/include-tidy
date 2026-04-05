/*
**      include-tidy -- #include tidier
**      src/trans_unit.h
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

#ifndef include_tidy_trans_unit_H
#define include_tidy_trans_unit_H

/**
 * @file
 * Declares variables and functions for the translation unit.
 */

/// @cond DOXYGEN_IGNORE

// libclang
#include <clang-c/Index.h>

/// @endcond

/**
 * @defgroup tidy-trans-unit-group Translation Unit
 * Translation unit Variable and function declarations.
 * @{
 */

// extern variables
extern enum CXLanguageKind tidy_lang;   ///< Source file language.

////////// extern functions ///////////////////////////////////////////////////

/**
 * Initializes the translation unit by parsing the source file.
 *
 * @param argc The command-line argument count.
 * @param argv The command-line argument values.
 * @return Returns the initialized translation unit.
 */
CXTranslationUnit trans_unit_init( int argc, char const *const argv[] );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tidy_trans_unit_H */
/* vim:set et sw=2 ts=2: */
