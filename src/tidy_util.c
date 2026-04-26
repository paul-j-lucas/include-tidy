/*
**      include-tidy -- #include tidier
**      src/tidy_util.c
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
**      along with this program.  If not, see <http
*/

/**
 * @file
 * Defines miscellanous stuff.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "tidy_util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <stddef.h>

/// @endcond

/**
 * @addtogroup tidy-util-group
 * @{
 */

////////// extern constants ///////////////////////////////////////////////////

ext_lang_map const EXT_LANG_MAP[] = {
  { "c",   "c"   },
  { "c++", "c++" },
  { "cc",  "c++" },
  { "cp",  "c++" },
  { "cpp", "c++" },
  { "cxx", "c++" },
  { "h",   "c"   },
  { "h++", "c++" },
  { "hh",  "c++" },
  { "hp",  "c++" },
  { "hpp", "c++" },
  { "hxx", "c++" },
  { NULL,  NULL  }
};

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
