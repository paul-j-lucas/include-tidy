/**
 * Test derived from cdecl.
 *
 * Copyright (C) 2023-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/cdecl
 */

#include "p_macro.h"
#include "c_typedef.h"
#include "types.h"

#include <stdio.h>                      // FILE

void show_macros( cdecl_show_t show, FILE *fout ) {
  p_macro_iterator_t iter;
  // ...
}

void show_type_visitor( c_typedef_t const *tdef ) {
  (void)tdef->ast;
}

/* vim:set et sw=2 ts=2: */
