/**
 * Test derived from cdecl.
 *
 * Copyright (C) 2017-2026 Paul J. Lucas
 *
 * @sa https://github.com/paul-j-lucas/cdecl
 */

#ifndef cdecl_dump_H
#define cdecl_dump_H

#include "slist.h"

#include <stdio.h>                      // FILE

void c_sname_list_dump( slist_t const *list, FILE *fout );

#endif /* cdecl_dump_H */
/* vim:set et sw=2 ts=2: */
