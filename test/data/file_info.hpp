/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#ifndef swishxx+file_info_hpp
#define swishxx+file_info_hpp

#include <vector>

struct file_info {
  using list_type = std::vector<file_info*>;

  static file_info* ith_info( unsigned i ) {
    return list_[i];
  }

  static list_type      list_;
};

#endif /* swishxx+file_info_hpp */
/* vim:set et sw=2 ts=2: */
