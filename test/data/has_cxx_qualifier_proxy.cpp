#include "has_cxx_qualifier_proxy.hpp"

void S::test() {
  // "E" is the qualifier (qual_csr). It is an EnumDecl, not a ClassDecl.  Its
  // parent (qual_parent) is "S".  The current scope (scope_csr) is also "S".
  auto x = E::VALUE_A;
}
