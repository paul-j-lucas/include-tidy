/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#ifndef swishxx_conf_var_hpp
#define swishxx_conf_var_hpp

class conf_var {
protected:
  conf_var( char const *name ) { }
  virtual ~conf_var() { }
};

template<typename T> class conf;

#endif /* swishxx_conf_var_hpp */
/* vim:set et sw=2 ts=2: */
