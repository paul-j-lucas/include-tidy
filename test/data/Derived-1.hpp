#ifndef include_tidy_Derived_1_hpp
#define include_tidy_Derived_1_hpp

#include "Base-1.hpp"
#include <map>

struct Derived : Base, std::map<int,int> {
};

#endif /* include_tidy_Derived_1_hpp */
/* vim:set et sw=2 ts=2: */
