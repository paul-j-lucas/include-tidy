/*
**      include-tidy -- #include tidier
**      src/proxies.h
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

#ifndef tidy_proxies_h
#define tidy_proxies_h

/**
 * @file
 * Declares functions for include proxies.
 */

// local
#include "pjl_config.h"
#include "include.h"

// standard
#include <stdbool.h>

/**
 * @defgroup tidy-proxies-group Include Proxies
 * Functions for include proxies.
 * @{
 */

////////// extern functions ///////////////////////////////////////////////////

/**
 * Initializes the implicit include proxies for the translation unit.
 *
 * @sa includes_init()
 */
void implicit_proxies_init( void );

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

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* tidy_proxies_h */
/* vim:set et sw=2 ts=2: */
