#include "using_map.hpp"

using_map const& using_map_instance() {
  static using_map const i;
  return i;
}

int main() {
  static auto const &m = using_map_instance();
  auto const e = m.find( 42 );
  if ( e != m.end() )
    ;
}
