#ifndef include_tidy_test_derived_map_h
#define include_tidy_test_derived_map_h

#include <map>

struct derived_map : std::map<int,int> {
  static derived_map const& instance();
};

#endif /* include_tidy_test_derived_map_h */
