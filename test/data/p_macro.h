/**
 * Test derived from cdecl.
 *
 * Copyright (C) 2023-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/cdecl
 */

#ifndef cdecl_p_macro_H
#define cdecl_p_macro_H

#include "red_black.h"
#include "types.h"

#include <stdbool.h>

typedef int (*p_macro_dyn_fn_t)();

typedef rb_iterator_t p_macro_iterator_t;

struct p_macro {
  char const         *name;
  bool                is_dynamic;

  union {
    p_macro_dyn_fn_t  dyn_fn;

    struct {
      p_param_list_t *param_list;
      p_token_list_t  replace_list;
    };
  };
};

#endif /* cdecl_p_macro_H */
/* vim:set et sw=2 ts=2: */
