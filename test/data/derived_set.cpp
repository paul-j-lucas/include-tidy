#include "derived_set.hpp"

void int_set::f() {
  auto v = value_type{ 0 };
}

int erase_even( int_set &s ) {
  std::erase_if( s, []( auto x ) { return x % 2 == 0; } );
}

bool find_42( int_set const &s ) {
  auto const e = s.find( 42 );
  return e != s.end();
}
