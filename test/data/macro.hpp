#ifndef macro_hpp
#define macro_hpp

#include <iostream>

#define internal_error \
  std::cerr << (me) << ", \"" \
  << __FILE__ << "\", line " << __LINE__ << ": internal error: "

#define ZERO(/**/TYPE,SIZE)       (TYPE){ .count = 0, .size = (SIZE) }

#endif /* macro_hpp */
