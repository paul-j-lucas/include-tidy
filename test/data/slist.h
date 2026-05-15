/**
 * Test derived from PJL Library.
 *
 * Copyright (C) 2017-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/cdecl
 */

#ifndef pjl_slist_H
#define pjl_slist_H

#include <stddef.h>                     // size_t

typedef struct slist      slist_t;
typedef struct slist_node slist_node_t;

struct slist {
  slist_node_t *head;                   ///< Pointer to list head.
  slist_node_t *tail;                   ///< Pointer to list tail.
  size_t        len;                    ///< Length of list.
};

struct slist_node {
  slist_node_t *next;                   ///< Pointer to next node or NULL.
  void         *data;                   ///< Pointer to node's data.
};

#endif /* pjl_slist_H */
/* vim:set et sw=2 ts=2: */
