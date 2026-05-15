/**
 * Test derived from cdecl.
 *
 * Copyright (C) 2017-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/cdecl
 */

#ifndef cdecl_types_H
#define cdecl_types_H

#include "slist.h"

enum cdecl_show {
  CDECL_SHOW_PREDEFINED       = 1 << 0,
  CDECL_SHOW_USER_DEFINED     = 1 << 1,
  CDECL_SHOW_OPT_IGNORE_LANG  = 1 << 2
};

enum decl_flags {
  C_ENG_DECL              = 1 << 0,
  C_ENG_OPT_OMIT_DECLARE  = 1 << 1,
  C_GIB_PRINT_CAST        = 1 << 8,
  C_GIB_PRINT_DECL        = 1 << 9,
  C_GIB_OPT_MULTI_DECL    = 1 << 10,
  C_GIB_OPT_OMIT_TYPE     = 1 << 11,
  C_GIB_OPT_SEMICOLON     = 1 << 12,
  C_GIB_TYPEDEF           = 1 << 13,
  C_GIB_USING             = 1 << 14,
};

typedef struct c_ast        c_ast_t;
typedef slist_t             c_ast_list_t;
typedef enum   decl_flags   decl_flags_t;
typedef unsigned long long  c_tid_t;
typedef struct c_type       c_type_t;
typedef struct c_typedef    c_typedef_t;
typedef enum   cdecl_show   cdecl_show_t;
typedef struct p_macro      p_macro_t;
typedef slist_t             p_param_list_t;
typedef slist_t             p_token_list_t;

#endif /* cdecl_types_H */
/* vim:set et sw=2 ts=2: */
