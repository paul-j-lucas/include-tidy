#include "derived_map.hpp"

derived_map const& derived_map::instance() {
  static derived_map const i;
  return i;
}

int main() {
  static auto const &m = derived_map::instance();
  auto const e = m.find( 42 );
  if ( e != m.end() )
    ;
}
