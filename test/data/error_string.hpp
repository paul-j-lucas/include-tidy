#ifndef error_string_hpp
#define error_string_hpp

#include "omanip.hpp"
#include <ostream>

inline std::ostream& error_string( std::ostream &o, int err_code ) {
  return o << err_code;
}

inline omanip<int> error_string( int err_code ) {
  return omanip<int>( error_string, err_code );
}

#endif /* error_string_hpp */
/* vim:set et sw=2 ts=2: */
