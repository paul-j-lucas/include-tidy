/*
**      include-tidy -- #include tidier
**      src/config_file.h
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

#ifndef include_tidy_config_file_H
#define include_tidy_config_file_H

// local
#include "pjl_config.h"

/// @cond DOXYGEN_IGNORE

// libclang
#include <clang-c/Index.h>

/// @endcond

/**
 * @defgroup config-file-group Configuration File
 * Functions to read an **include-tidy**(1) configuration file and look-up
 * information specified therein.
 * @{
 */

////////// extern functions ///////////////////////////////////////////////////

/**
 * Gets the header file that \a symbol_name maps to, if any.
 *
 * @param symbol_name The symbol name.
 * @return Returns said header file or NULL if none.
 */
CXFile config_get_symbol_include( char const *symbol_name );

/**
 * Reads an **include-tidy**(5) configuration file, if any.
 *
 * @note This function must be called at most once.
 */
void config_init( void );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tidy_config_file_H */
/* vim:set et sw=2 ts=2: */
