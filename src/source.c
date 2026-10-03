/*
**      include-tidy -- #include tidier
**      src/source.c
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

/**
 * @file
 * Defines functions for a source file being tidied.
 */

// local
#include "pjl_config.h"
#include "source.h"
#include "util.h"

// libclang
#include <clang-c/Index.h>

// standard
#include <stddef.h>

/**
 * @addtogroup tidy-source-group
 * @{
 */

////////// extern functions ///////////////////////////////////////////////////

void tidy_source_cleanup( tidy_source *source ) {
  if ( source != NULL ) {
    FREE( source->assoc_header_rel_path );
    // source->path points to an argv so it doesn't need freeing
    if ( source->tu != NULL )
      clang_disposeTranslationUnit( source->tu );
  }
};

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
