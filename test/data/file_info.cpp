/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#include "file_info.hpp"

#include <cstring>

inline char* new_strdup( char const *s ) {
  return std::strcpy( new char[ std::strlen( s ) + 1 ], s );
}

inline char const* pjl_basename( char const *file_name ) {
  char const *const slash = ::strrchr( file_name, '/' );
  return slash ? slash + 1 : file_name;
}

file_info::file_info( char const *path_name, unsigned dir_index,
                      size_t file_size, char const *title,
                      unsigned num_words ) :
  dir_index_{ dir_index },
  file_name_{
    pjl_basename( *name_set_.insert( new_strdup( path_name ) ).first )
  },
  file_size_{ file_size }, num_words_{ num_words },
  title_{ title ? new_strdup( title ) : file_name_ }
{
  // ...
}

/* vim:set et sw=2 ts=2: */
