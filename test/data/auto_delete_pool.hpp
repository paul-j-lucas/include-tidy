/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#ifndef swishxx_auto_delete_pool_hpp
#define swishxx_auto_delete_pool_hpp

template<class T> struct auto_delete_obj;

template<typename T>
struct auto_delete_pool {
};

template<typename Derived>
struct auto_delete_obj {
  using pool_object_type = auto_delete_obj<Derived>;
  using pool_type = auto_delete_pool<Derived>;

  auto_delete_obj( pool_type& ) { }
};

#endif /* swishxx_auto_delete_pool_hpp */
/* vim:set et sw=2 ts=2: */
