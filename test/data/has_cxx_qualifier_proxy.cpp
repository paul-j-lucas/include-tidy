#include "has_cxx_qualifier_proxy.hpp"

void S1::test() {
  // "E" is the qualifier (qual_csr). It is an EnumDecl, not a ClassDecl.  Its
  // parent (qual_parent) is "S1".  The current scope (scope_csr) is also "S1".
  auto x = E::VALUE_A;
}

void S2::test() {
  auto x = Global_E_Alias<int>::G_VALUE_A;
}

/* vim:set et sw=2 ts=2: */
