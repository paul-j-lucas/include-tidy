/*
**      include-tidy -- #include tidier
**      src/includes.c
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

// local
#include "pjl_config.h"
#include "clang_util.h"

// libclang
#include <clang-c/Index.h>

////////// extern functions ///////////////////////////////////////////////////

/**
 * Gets the real path of \a file.
 *
 * @param file The file to get the real path of.
 * @return Returns the string containing the real path of \a file.  The caller
 * _must_ call `clang_disposeString()` on it.
 *
 */
NODISCARD
CXString tidy_File_getRealPathName( CXFile file ) {
  CXString          file_str  = clang_File_tryGetRealPathName( file );
  char const *const file_cstr = clang_getCString( file_str );

  if ( file_cstr == NULL || file_cstr[0] == '\0' ) {
    clang_disposeString( file_str );
    file_str = clang_getFileName( file );
  }

  return file_str;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
