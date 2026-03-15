/*
**      include-tidy -- #include tidier
**      src/toml_lite.c
**
**      Copyright (C) 2026  Paul J. Lucas
**
**      This program is free software: you can redistribute it and/or modify
**      it under the terms of the GNU General Public License as published by
**      the Free Software Foundation, either version 3 of the License, or
**      (at your option) any later version.
**
**      This program is distributed in the hope that it will be useful,
**      but WITHOUT ANY WARRANTY; without even the implied warranty of
**      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**      GNU General Public License for more details.
**
**      You should have received a copy of the GNU General Public License
**      along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file
 * Defines types and functions for reading a TOML file.
 */

// local
#include "pjl_config.h"
#include "toml_lite.h"
#include "util.h"

// standard
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>                   /* for strtol */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////

// local functions
NODISCARD
static bool toml_space_skip( toml_file* ),
            toml_string_parse( toml_file*, char** ),
            toml_value_parse( toml_file*, toml_value* );

static void toml_value_cleanup( toml_value* );

////////// inline functions ///////////////////////////////////////////////////

/**
 * Checks whether \a c is a whitespace character according to TOML.
 *
 * @param c The character to check.
 * @return Returns `true` only if \a is a space.
 */
static inline bool is_toml_space( int c ) {
  return c == ' ' || c == '\t';
}

/**
 * Gets the next character, if any.
 *
 * @param toml The toml_file to get the next character from.
 * @return Returns the next character or `EOF`.
 */
NODISCARD
static int toml_getc( toml_file *toml ) {
  toml->col_prev = toml->col;
  int const c = fgetc( toml->file );
  if ( c == '\n' ) {
    ++toml->line;
    toml->col = 1;
  }
  else {
    ++toml->col;
  }
  return c;
}

/**
 * Ungets \a c.
 *
 * @param toml The toml_file to unget \a c.
 * @param c The character to unget.
 */
static void toml_ungetc( toml_file *toml, int c ) {
  ungetc( c, toml->file );
  if ( c == '\n' ) {
    assert( toml->line > 0 );
    --toml->line;
  }
  toml->col = toml->col_prev;
}

////////// local functions ////////////////////////////////////////////////////

/**
 * Peeks at the next character from \a file.
 *
 * @param file The `FILE` to peek from.
 * @return Returns the character peeked or `EOF`.
 */
NODISCARD
static int fpeekc( FILE *file ) {
  int const c = fgetc( file );
  if ( c != EOF )
    ungetc( c, file );
  return c;
}

/**
 * Cleans-up a toml_array.
 *
 * @param a The toml_array to clean up.  If NULL, does nothing.
 */
static void toml_array_cleanup( toml_array *a ) {
  if ( a == NULL )
    return;
  for ( unsigned i = 0; i < a->size; ++i )
    toml_value_cleanup( &a->values[i] );
  free( a->values );
}

/**
 * Parses a TOML array.
 *
 * @param toml The toml_file to use.
 * @param pa The toml_array to parse into.
 * @return Returns `true` only if all values were parsed successfully.
 */
NODISCARD
static bool toml_array_parse( toml_file *toml, toml_array *pa ) {
  assert( toml != NULL );
  assert( pa != NULL );

  ++toml->array_depth;

  toml_array a = { 0 };
  unsigned array_cap = 16;
  a.values = MALLOC( toml_value, array_cap );

  while ( true ) {
    PJL_DISCARD_RV( toml_space_skip( toml ) );
    int c = toml_getc( toml );
    if ( c == ']' )
      break;

    if ( c == EOF )

    toml_ungetc( toml, c );
    toml_value value;
    if ( !toml_value_parse( toml, &value ) )
      goto error;

    if ( a.size + 1 >= array_cap ) {
      array_cap *= 2;
      REALLOC( a.values, toml_value, array_cap );
    }
    a.values[ a.size++ ] = value;

    PJL_DISCARD_RV( toml_space_skip( toml ) );
    c = toml_getc( toml );
    if ( c == EOF ) {
      toml->error = "unexpected end of array";
      goto error;
    }
    if ( c == ',' )
      continue;
  } // while

  *pa = a;
  --toml->array_depth;
  return true;

error:
  toml_array_cleanup( &a );
  --toml->array_depth;
  return false;
}

/**
 * Parses a TOML Boolean value.
 *
 * @param toml The toml_file to use.
 * @param pb The Boolean to parse into.
 * @return Returns `true` only upon success.
 */
NODISCARD
static bool toml_bool_parse( toml_file *toml, bool *pb ) {
  assert( toml != NULL );
  assert( pb != NULL );

  char        buf[ STRLITLEN( "false" ) ];
  int         c = fpeekc( toml->file );
  bool const  is_f = c == 'f';

  size_t const bytes_want = is_f ? STRLITLEN( "false" ) : STRLITLEN( "true" );
  size_t const bytes_read = fread( buf, 1, bytes_want, toml->file );

  c = fpeekc( toml->file );             // ensure not falsex or truex
  bool const next_c_is_ok = c == EOF || !isalnum( c );

  if ( bytes_read < bytes_want ||
       !next_c_is_ok ||
       ( is_f && strncmp( buf, "false", STRLITLEN( "false" ) ) != 0) ||
       (!is_f && strncmp( buf, "true" , STRLITLEN( "true"  ) ) != 0) ) {
    toml->error = "unexpected value";
    return false;
  }

  toml->col += bytes_read;
  *pb = !is_f;
  return true;
}

/**
 * Parses \a c.
 *
 * @param toml The toml_file to use.
 * @param c The character to parse.
 * @return Returns `true` only if \a c was parsed successfully.
 */
NODISCARD
static bool toml_char_parse( toml_file *toml, char c ) {
  assert( toml != NULL );

  if ( toml_getc( toml ) != c ) {
    toml->error = "unexpected character";
    return false;
  }
  return true;
}

/**
 * Parses a TOML integer.
 *
 * @param pi A pointer to receive the integer.
 * @return Returns `true` only if an integer was parsed successfully.
 */
NODISCARD
static bool toml_integer_parse( toml_file *toml, long *pi ) {
  assert( toml != NULL );
  assert( pi != NULL );

  char    buf[ MAX_DEC_INT_DIGITS( long ) + 1 ];
  size_t  buf_len = 0;

  int base = 10;
  int c = toml_getc( toml );

  switch ( c ) {
    case EOF:
      return false;
    case '+':
      break;
    case '-':
      buf[ buf_len++ ] = '-';
      break;
    case '0':
      c = toml_getc( toml );
      switch ( c ) {
        case 'b':
          base = 2;
          break;
        case 'o':
          base = 8;
          break;
        case 'x':
          base = 16;
          break;
        case EOF:
          *pi = 0;
          return true;
        default:
          toml->error = "unknown integer prefix";
          return false;
      } // switch
      break;
  } // switch

  while ( (c = toml_getc( toml )) != EOF ) {
    if ( c == '_' )
      continue;
    if ( buf_len + 1 == MAX_DEC_INT_DIGITS( long ) ) {
      toml->error = "integer too long";
      return false;
    }
    buf[ buf_len++ ] = STATIC_CAST( char, c );
  } // while

  if ( buf[ buf_len - 1 ] == '_' ) {
    toml->error = "invalid integer";
    return false;
  }

  buf[ buf_len ] = '\0';
  errno = 0;
  long const rv = strtol( buf, /*endptr=*/NULL, base );
  if ( errno != 0 ) {
    toml->error = "invalid integer";
    return false;
  }

  *pi = rv;
  return true;
}

/**
 * Parses a TOML key.
 *
 * @param toml The toml_file to use.
 * @param pkey The string to receive the key.  The caller is responsible for
 * freeing it.
 * @return Returns `true` only if a key was parsed successfully.
 */
NODISCARD
static bool toml_key_parse( toml_file *toml, char **pkey ) {
  assert( toml != NULL );
  assert( pkey != NULL );

  static char const BARE_KEY_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "-._";

  int c = toml_getc( toml );
  switch ( c ) {
    case '"':
      return toml_string_parse( toml, pkey );
    case '.':
      toml->error = "bare key can not begin with '.'";
      return false;
    case EOF:
      return false;
  } // switch

  size_t  key_cap = 16;
  char   *key = MALLOC( char, key_cap + 1 );
  size_t  key_len = 0;

  do {
    if ( !is_toml_space( c ) ) {
      if ( strchr( BARE_KEY_CHARS, c ) == NULL ) {
        toml_ungetc( toml, c );
        break;
      }
      if ( key_len + 1 >= key_cap ) {
        key_cap *= 2;
        REALLOC( key, char, key_cap + 1 );
      }
      key[ key_len++ ] = STATIC_CAST( char, c );
    }
    c = toml_getc( toml );
  } while ( c != EOF );

  if ( key_len == 0 ) {
    toml->error = "empty key";
    goto error;
  }

  if ( key[ key_len - 1 ] == '.' ) {
    toml->error = "invalid key";
    goto error;
  }

  key[ key_len ] = '\0';
  *pkey = key;
  return true;

error:
  free( key );
  return false;
}

/**
 * Cleans-up a toml_key_value.
 *
 * @param kv The toml_key_value to clean-up. If NULL, does nothing.
 */
static void toml_key_value_cleanup( toml_key_value *kv ) {
  if ( kv == NULL )
    return;
  FREE( kv->key );
  toml_value_cleanup( &kv->value );
}

/**
 * Compares two toml_key_value objects.
 *
 * @param i_kv The first toml_key_value.
 * @param j_kv The second toml_key_value.
 * @return Returns a number less than 0, 0, or greater than 0 if the key of \a
 * i_kv is less than, equal to, or greater than the key of \a j_kv,
 * respectively.
 */
NODISCARD
static int toml_key_value_cmp( toml_key_value *i_kv, toml_key_value *j_kv ) {
  assert( i_kv != NULL );
  assert( j_kv != NULL );
  return strcmp( i_kv->key, j_kv->key );
}

/**
 * Parses a TOML _key_ `=` _value_.
 *
 * @param toml The toml_file to use.
 * @param kv The toml_key_value to receive the key and value.
 * @return Returns `true` only if both a key and value were successfully
 * parsed.
 */
NODISCARD
static bool toml_key_value_parse( toml_file *toml, toml_key_value *kv ) {
  assert( toml != NULL );
  assert( kv != NULL );

  char       *key = NULL;
  toml_value  value = { 0 };

  if ( !toml_key_parse( toml, &key ) )
    return false;

  assert( !toml->in_key_value );
  toml->in_key_value = true;

  bool const ok =
    toml_space_skip( toml ) &&
    toml_char_parse( toml, '=' ) &&
    toml_space_skip( toml ) &&
    toml_value_parse( toml, &value );

  toml->in_key_value = false;
  if ( ok )
    *kv = (toml_key_value){ .key = key, .value = value };
  else
    free( key );

  return ok;
}

/**
 * Skips all whitespace.
 *
 * @param toml The toml_file to use.
 * @return Returns `true` only upon success.
 */
NODISCARD
static bool toml_space_skip( toml_file *toml ) {
  assert( toml != NULL );

  for ( int c; (c = toml_getc( toml )) != EOF; ) {
    if ( c == '\n' ) {
      if ( toml->in_key_value && toml->array_depth == 0 ) {
        toml->error = "unexpected newline";
        return false;
      }
    }
    else if ( !is_toml_space( c ) ) {
      toml_ungetc( toml, c );
      break;
    }
  } // for

  return true;
}

/**
 * Parses a TOML string.
 *
 * @param toml The toml_file to use.
 * @param ps A pointer to receive the string.
 * @return Returns `true` only if a string was parsed successfully.
 */
NODISCARD
static bool toml_string_parse( toml_file *toml, char **ps ) {
  assert( toml != NULL );
  assert( ps != NULL );

  char      buf[ 1024 ];
  int       c;
  unsigned  i = 0;

  while ( (c = toml_getc( toml )) != EOF ) {
    ++toml->col;
    if ( i >= sizeof buf ) {
      // error
      return false;
    }
    if ( c == '"' )
      break;
    if ( c == '\\' ) {
      if ( (c = toml_getc( toml )) == EOF ) {
        /* error */;
        return false;
      }
    }
    buf[ i++ ] = STATIC_CAST( char, c );
  } // while

  buf[i] = '\0';

  *ps = check_strdup( buf );
  return true;
}

/**
 * Parses a table name.
 *
 * @param pname A pointer to receive the table name.
 * @return Returns `true` only if a table name was parsed successfully.
 *
 * @note Assumes the caller has already parsed the `[`.
 */
NODISCARD
static bool toml_table_name_parse( toml_file *toml, char **pname ) {
  assert( toml != NULL );
  assert( pname != NULL );

  char *key = NULL;

  bool const ok =
    toml_key_parse( toml, &key ) &&
    toml_char_parse( toml, ']' );

  if ( ok )
    *pname = key;
  else
    free( key );

  return ok;
}

/**
 * Cleans-up a toml_value.
 *
 * @param v The toml_value to clean-up.  If NULL, does nothing.
 */
static void toml_value_cleanup( toml_value *v ) {
  if ( v == NULL )
    return;
  switch ( v->type ) {
    case TOML_ARRAY:
      toml_array_cleanup( &v->a );
      break;
    case TOML_BOOL:
    case TOML_INT:
      // nothing to do
      break;
    case TOML_STRING:
      free( v->s );
      break;
  } // switch
}

NODISCARD
static bool toml_value_parse( toml_file *toml, toml_value *v ) {
  assert( toml != NULL );
  assert( v != NULL );

  int const c = toml_getc( toml );
  switch ( c ) {
    case '"':;
      char *s;
      if ( !toml_string_parse( toml, &s ) )
        return false;
      *v = (toml_value){ .type = TOML_STRING, .s = s };
      return true;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      toml_ungetc( toml, c );
      long i;
      if ( !toml_integer_parse( toml, &i ) )
        return false;
      *v = (toml_value){ .type = TOML_INT, .i = i };
      return true;

    case 'f':
    case 't':
      toml_ungetc( toml, c );
      bool b;
      if ( !toml_bool_parse( toml, &b ) )
        return false;
      *v = (toml_value){ .type = TOML_BOOL, .b = b };
      return true;

    case '[':;
      toml_array a;
      if ( !toml_array_parse( toml, &a ) )
        return false;
      *v = (toml_value){ .type = TOML_ARRAY, .a = a };
      return true;

    default:
      toml->error = "unexpected character";
      return false;
  } // switch
}

////////// extern functions ///////////////////////////////////////////////////

void toml_close( toml_file *toml ) {
  assert( toml != NULL );
  if ( toml->file != NULL )
    fclose( toml->file );
  *toml = (toml_file){ 0 };
}

void toml_init( toml_file *toml, FILE *file ) {
  assert( toml != NULL );
  *toml = (toml_file){
    .file = file,
    .line = 1
  };
}

void toml_table_cleanup( toml_table *table ) {
  if ( table == NULL )
    return;
  FREE( table->name );
  table->name = NULL;
  rb_tree_cleanup(
    &table->keys_values,
    POINTER_CAST( rb_free_fn_t, &toml_key_value_cleanup )
  );
}

void toml_table_init( toml_table *table ) {
  assert( table != NULL );
  table->name = NULL;
  rb_tree_init(
    &table->keys_values, RB_DINT,
    POINTER_CAST( rb_cmp_fn_t, toml_key_value_cmp )
  );
}

NODISCARD
bool toml_table_next( toml_file *toml, toml_table *table ) {
  assert( toml != NULL );
  assert( table != NULL );

  if ( !toml_space_skip( toml ) )
    return false;

  toml_table_cleanup( table );

  while ( true ) {
    int c = toml_getc( toml );
    if ( c == '[' ) {
      if ( !toml_space_skip( toml ) )
        return false;
      if ( !toml_table_name_parse( toml, (char**)&table->name ) )
        return false;
    }
    if ( !toml_space_skip( toml ) )
      return false;
    toml_key_value kv;
    if ( !toml_key_value_parse( toml, &kv ) )
      return false;

    rb_insert_rv_t const rb_rbi =
      rb_tree_insert( &table->keys_values, &kv, sizeof kv );
    if ( !rb_rbi.inserted ) {
      toml_key_value_cleanup( &kv );
      toml->error = "duplicate key";
      return false;
    }

    if ( !toml_space_skip( toml ) )
      return false;
    c = fpeekc( toml->file );
    if ( c == EOF || c == '[' )
      return true;
  } // while

  return false;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
