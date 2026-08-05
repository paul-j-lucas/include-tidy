#include "Derived.hpp"
#include "util.hpp"

#include <cstring>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_set>

struct S {
  S() : _gid{ ::getegid() } { }

  bool has_it( gid_t ) const;

  gid_t _gid;
  std::unordered_set<gid_t> _s;
};

inline bool S::has_it( gid_t n ) const {
  return contains( _s, n );
}

void f( char const *s ) {
  char const *p = strpbrk( s, "," );

  Derived::iterator i, j;
  if ( i != j )
    ;
}
