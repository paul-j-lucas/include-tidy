#ifndef include_tidy_test_str_str_map_h
#define include_tidy_test_str_str_map_h

#include <map>
#include <string>

class string_string_map : public std::map<std::string,std::string> {
public:
  static string_string_map const& instance();
};

#endif /* include_tidy_test_str_str_map_h */
