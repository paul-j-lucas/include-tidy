#include "using_set.hpp"

using_set const& using_set_instance() {
  static using_set const i;
  return i;
}

int main() {
  static auto const &m = using_set_instance();
  auto const e = m.find( 42 );
  if ( e != m.end() )
    ;
}
