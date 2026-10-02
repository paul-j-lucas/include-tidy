/*
**      include-tidy -- #include tidier
**      src/source.h
**
**      Copyright (C) 2026  Paul J. Lucas, et al.
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

#ifndef tidy_source_h
#define tidy_source_h

/**
 * @file
 * Declares a structure for a source file being tidied.
 */

// standard
#include <stdbool.h>

/**
 * @defgroup tidy-source-group Source File
 * A structure for a source file being tidied.
 * @{
 */

////////// typedefs ///////////////////////////////////////////////////////////

typedef struct tidy_source tidy_source;

////////// structs ////////////////////////////////////////////////////////////

/**
 * A source file being tidied.
 */
struct tidy_source {
  char const *path;                     ///< Source file path.
  bool        is_cxx;                   ///< Is \ref path C++?
  bool        is_ignored;               ///< Is \ref path ignored?
};

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* tidy_source_h */
/* vim:set et sw=2 ts=2: */
