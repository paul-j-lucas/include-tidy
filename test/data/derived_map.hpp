#ifndef include_tidy_test_derived_map_hpp
#define include_tidy_test_derived_map_hpp

#include <map>

struct int_int_map : std::map<int,int> {
  void f();
};

#endif /* include_tidy_test_derived_map_hpp */
