#ifndef has_cxx_qualifier_proxy_hpp
#define has_cxx_qualifier_proxy_hpp

struct S1 {
  enum E {
    VALUE_A
  };

  void test();
};

enum Global_E {
  G_VALUE_A
};

template<typename T>
using Global_E_Alias = Global_E;

struct S2 {
  void test();
};

#endif /* has_cxx_qualifier_proxy_hpp */
/* vim:set et sw=2 ts=2: */
