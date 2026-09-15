/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#ifndef swishxx_token_hpp
#define swishxx_token_hpp

#include <sstream>

class token_stream;

constexpr int Word_Hard_Max_Size = 25;

class token {
public:
  enum type {
    tt_none,
    tt_and,
    tt_equal,
    tt_lparen,
    tt_near,
    tt_not_near,
    tt_not,
    tt_or,
    tt_rparen,
    tt_word_star,
    tt_word
  };

  token() : type_{ tt_none }            { }
  explicit token( token_stream &in )    { in >> *this; }
  token( token const& ) = default;
  token& operator=( token const& ) = default;

  operator    type() const              { return type_; }
  int         length() const            { return len_; }
  char const* str() const               { return buf_; }
  char const* lower_str() const         { return lower_buf_; }

  friend token_stream& operator>>( token_stream&, token& );

private:
  type type_;
  char buf_[ Word_Hard_Max_Size + 1 ];
  char lower_buf_[ Word_Hard_Max_Size + 1 ];
  int  len_;
};

class token_stream : public std::istringstream {
public:
  token_stream( char const *s ) : std::istringstream{ s }, top_{ -1 } { }

  void put_back( token const &t ) {
    stack_[ ++top_ ] = t;
  }

private:
  token stack_[2];
  int   top_;
};

#endif /* swishxx_token_hpp */
/* vim:set et sw=2 ts=2: */
