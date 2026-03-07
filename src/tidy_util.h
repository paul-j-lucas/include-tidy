/*
**      include-tidy -- #include tidier
**      src/tidy_util.h
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

#ifndef include_tidy_tidy_util_H
#define include_tidy_tidy_util_H

////////// extern functions ///////////////////////////////////////////////////

/**
 * Gets the `#include` path delimiters that should be used for \a full_path.
 *
 * @param full_path The full path of a file that's included.
 * @param delims A 2-element array to receive the opening and closing
 * delimiters.
 */
void include_get_delims( char const *full_path, char delims[static 2] );

///////////////////////////////////////////////////////////////////////////////

#endif /* include_tidy_tidy_util_H */
/* vim:set et sw=2 ts=2: */
