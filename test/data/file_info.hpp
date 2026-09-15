/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#ifndef file_info_hpp
#define file_info_hpp

#include "hash.hpp"

#include <cstddef>
#include <vector>

class file_info {
public:
  using list_type = std::vector<file_info*>;
  using const_iterator = list_type::const_iterator;
  using size_type = size_t;
  using name_set_type = PJL::unordered_char_ptr_set;

  file_info( char const *path_name, unsigned dir_index, size_t file_size,
             char const *title, unsigned num_words = 0 );

  file_info( unsigned char const *ptr_into_index_file );

  unsigned dir_index() const {
    return dir_index_;
  }

  char const* file_name() const {
    return file_name_;
  }

  unsigned num_words() const {
    return num_words_;
  }

  size_type size() const {
    return file_size_;
  }

  char const* title() const {
    return title_;
  }

  static const_iterator begin() {
    return list_.begin();
  }

  static const_iterator end() {
    return list_.end();
  }

  static unsigned current_index() {
    return static_cast<unsigned>( list_.size() - 1 );
  }

  static void inc_words() {
    ++list_.back()->num_words_;
  }

  static file_info* ith_info( unsigned i ) {
    return list_[i];
  }

  static size_type num_files() {
    return list_.size();
  }

  static bool seen_file( char const *file_name ) {
    return name_set_.find( file_name ) != name_set_.end();
  }

private:
  unsigned const        dir_index_;
  char const *const     file_name_;
  size_type const       file_size_;
  unsigned              num_words_;
  char const *const     title_;

  static list_type      list_;
  static name_set_type  name_set_;
};

#endif /* file_info_hpp */
/* vim:set et sw=2 ts=2: */
