#include "str_str_map.hpp"
#include <string>

string_string_map const& string_string_map::instance() {
  static string_string_map const i;
  return i;
}

int main() {
  static auto const &elements = string_string_map::instance();
  std::string const s{ "foo" };
  auto const e = elements.find( s );
  if ( e != elements.end() )
    ;
}
