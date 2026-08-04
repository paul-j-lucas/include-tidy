/**
 * Test derived from SWISH++.
 *
 * Copyright (C) 1998-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/swishxx
 */

#ifndef swishxx_query_node_hpp
#define swishxx_query_node_hpp

#include "auto_delete_pool.hpp"

struct query_node : auto_delete_obj<query_node> {
  query_node( pool_type &p ) : pool_object_type{ p } { }
};

struct and_node : query_node {
  and_node( pool_type& );
};

#endif /* swishxx_query_node_hpp */
/* vim:set et sw=2 ts=2: */
