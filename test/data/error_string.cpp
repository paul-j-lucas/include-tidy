#include "error_string.hpp"
#include <ostream>

void f( std::ostream &o ) {
  o << 'x' << error_string( 2 ) << '\n';
}
