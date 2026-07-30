#ifndef include_tidy_test_util_hpp
#define include_tidy_test_util_hpp

#include <unordered_set>

template<typename T,class Compare,class Alloc,typename Key> inline
bool contains( std::unordered_set<T,Compare,Alloc> const &s, Key const &key ) {
  return s.find( key ) != s.end();
}

#endif /* include_tidy_test_util_hpp */
