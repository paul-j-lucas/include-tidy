#ifndef omanip_hpp
#define omanip_hpp

#include <ostream>

template<typename Arg>
class omanip {
public:
  using func_type = std::ostream& (*)( std::ostream&, Arg );

  omanip( func_type f, Arg const &arg ) : f_{ f }, arg_{ arg } { }

  friend std::ostream& operator<<( std::ostream &o, omanip<Arg> const &om ) {
    return (*om.f_)( o, om.arg_ );
  }

private:
  func_type const f_;
  Arg const arg_;
};

#endif /* omanip_hpp */
/* vim:set et sw=2 ts=2: */
