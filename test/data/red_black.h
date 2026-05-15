/**
 * Test derived from PJL Libarary.
 *
 * Copyright (C) 2017-2026 Paul J. Lucas, et al.
 *
 * @sa https://github.com/paul-j-lucas/cdecl
 * @sa https://github.com/paul-j-lucas/include-tidy
 */

#ifndef pjl_red_black_H
#define pjl_red_black_H

#include <stdalign.h>                   // alignas
#include <stddef.h>                     // max_align_t

#define RB_ITERATOR_DEPTH_MAX     64

typedef struct rb_iterator  rb_iterator_t;
typedef struct rb_node      rb_node_t;
typedef struct rb_tree      rb_tree_t;

struct rb_iterator {
  rb_tree_t const  *tree;
  rb_node_t        *curr;
  rb_node_t        *stack[ RB_ITERATOR_DEPTH_MAX ];
  unsigned          stack_top;
};

struct rb_node {
  rb_node_t  *child[2];
  rb_node_t  *parent;
  // ...
  alignas( max_align_t ) char data[];
};

struct rb_tree {
  rb_node_t  *root;
  // ...
  rb_node_t   nil;
};

#endif /* pjl_red_black_H */
/* vim:set et sw=2 ts=2: */
