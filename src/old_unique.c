void array_unique( array_t *restrict array, array_cmp_fn_t cmp_fn,
                   array_free_fn_t free_fn ) {
  assert( array != NULL );
  assert( cmp_fn != NULL );

  if ( array->len < 2 )
    return;

  char             *dst = array_at_nc( array, 1 );
  void const *const end = array_at_nc( array, array->len );
  size_t const      esize = array->esize;
  void const       *unique_last = array_front_nc( array );

  for ( char *src = dst; src < end; src += esize ) {
    if ( (*cmp_fn)( last_unique, src ) != 0 ) {
      last_unique = src;
      if ( dst != src )
        memcpy( dst, src, esize );
      dst += esize;
    }
    else if ( free_fn != NULL ) {
      (*free_fn)( src );
    }
  } // for

  array->len = (size_t)(dst - (char*)array_front_nc( array )) / esize;
}
