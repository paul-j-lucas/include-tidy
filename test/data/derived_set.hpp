#ifndef include_tidy_test_derived_set_h
#define include_tidy_test_derived_set_h

#include <set>

struct int_set : std::set<int> {
  void f();
};

#endif /* include_tidy_test_derived_set_h */
