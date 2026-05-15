/**
 * Test derived from cdecl.
 *
 * Copyright (C) 2017-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/cdecl
 */

#ifndef cdecl_english_H
#define cdecl_english_H

#include "types.h"

#include <stdio.h>                      // FILE

void c_ast_english( c_ast_t const *ast, decl_flags_t eng_flags, FILE *fout );
void c_ast_list_english( c_ast_list_t const *ast_list, FILE *fout );

#endif /* cdecl_english_H */
/* vim:set et sw=2 ts=2: */
