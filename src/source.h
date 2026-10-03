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
 * Declares a structure and functions for a source file being tidied.
 */

// libclang
#include <clang-c/Index.h>

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
  char const       *path;               ///< Source file path.
  CXTranslationUnit tu;                 ///< Translation unit for \ref path.
  CXFile            file;               ///< File for \ref path.

  /**
   * The associated header for \ref path only if set explicitly via the
   * `associated-header` configuration key.
   */
  char const *assoc_header_rel_path;

  bool            is_cxx;               ///< Is \ref path C++?
  bool            is_ignored;           ///< Is \ref path ignored?

  unsigned        includes_missing;     ///< Number of missing includes.
  unsigned        includes_unnecessary; ///< Number of unnecessry includes.
};

////////// extern functions ///////////////////////////////////////////////////

/**
 * Checks the source's translation unit for errors and prints them, if any.
 *
 * @note If there are errors, this function does not return.
 *
 * @param source The source file being tidied.
 */
void source_check_for_errors( tidy_source const *source );

/**
 * Cleans-up all memory associated with \a source but does _not_ free \a source
 * itself.
 *
 * @param source the tidy_source to clean up.  If NULL, does nothing.
 */
void source_cleanup( tidy_source *source );

/**
 * Further initializes \a source.
 *
 * @param source The source to initialize.
 * @param argc The command-line argument count.
 * @param argv The command-line argument values.
 */
void source_init( tidy_source *source, int argc, char const *const argv[] );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* tidy_source_h */
/* vim:set et sw=2 ts=2: */
