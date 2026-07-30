#ifndef include_tidy_test_Base_hpp
#define include_tidy_test_Base_hpp

struct Base {
  struct iterator {
    // ...
  };
  // ...
};

inline bool operator!=( Base::iterator const &i, Base::iterator const &j ) {
  // ...
  return true;
}

#endif /* include_tidy_test_Base_hpp */
