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
#include "red_black.h"
#include "strbuf.h"
#include "util.h"

// standard
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @addtogroup toml-group
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

#define TOML_STRING_LEN_MAX       1024  /**< Maximum string length. */

////////// local constants ////////////////////////////////////////////////////

/**
 * TOML error messages.
 *
 * @note If \ref toml_file::error_msg is non-NULL, it overrides this message
 * with a more specific one.
 */
static char const *const TOML_ERROR_MSGS[] = {
  [ TOML_ERR_NONE            ] = "no error",
  [ TOML_ERR_INT_INVALID     ] = "invalid integer",
  [ TOML_ERR_INT_RANGE       ] = "integer out of range",
  [ TOML_ERR_KEY_DUPLICATE   ] = "duplicate key",
  [ TOML_ERR_KEY_INVALID     ] = "invalid key",
  [ TOML_ERR_STR_INVALID     ] = "invalid string",
  [ TOML_ERR_TABLE_DUPLICATE ] = "duplicate table",
  [ TOML_ERR_UNEX_CHAR       ] = "unexpected character",
  [ TOML_ERR_UNEX_EOF        ] = "unexpected end of file",
  [ TOML_ERR_UNEX_NEWLINE    ] = "unexpected newline",
  [ TOML_ERR_UNEX_VALUE      ] = "unexpected value",
};

////////// local functions ////////////////////////////////////////////////////

NODISCARD
static bool toml_space_skip( toml_file* ),
            toml_string_parse( toml_file*, char** ),
            toml_value_parse( toml_file*, toml_value* );

NODISCARD
static int  toml_getc( toml_file* );

static void toml_comment_parse( toml_file* );
static void toml_space_comments_skip( toml_file* );
static void toml_ungetc( toml_file*, int );
static void toml_value_cleanup( toml_value* );

////////// inline functions ///////////////////////////////////////////////////

/**
 * Gets whether \a c is a binary digit.
 *
 * @param c The character to check.
 * @return Returns `true` only if \c is either `'0'` or `'1'`.
 *
 * @sa is_ident()
 * @sa isodigit()
 */
static inline bool isbdigit( int c ) {
  return c == '0' || c == '1';
}

/**
 * Gets whether \a c is an octal digit.
 *
 * @param c The character to check.
 * @return Returns `true` only if \c is one of `01234567`.
 *
 * @sa is_ident()
 * @sa isbdigit()
 */
static inline bool isodigit( int c ) {
  return c >= '0' && c <= '7';
}

/**
 * Gets whether \a c is an identifier character, i.e., alphanumeric or `'_'`.
 *
 * @param c The character to check.
 * @return Returns `true` only if \c is an identifier character.
 *
 * @sa isbdigit()
 * @sa isodigit()
 */
static inline bool is_ident( int c ) {
  return isalnum( c ) || c == '_';
}

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
 * Increments the toml_file's column.
 *
 * @param toml The toml_file to use.
 * @param n How much to increment by.
 */
static inline void toml_col_inc( toml_file *toml, unsigned n ) {
  toml->col_prev = toml->loc.col;
  toml->loc.col += n;
}

/**
 * Performs a newline.
 *
 * @param toml The toml_file to use.
 */
static inline void toml_newline( toml_file *toml ) {
  ++toml->loc.line;
  toml->col_prev = toml->loc.col;
  toml->loc.col = 0;
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
 * @remarks Assumes the `'['` has already been parsed and is _not_ in the input
 * stream.
 *
 * @param toml The toml_file to use.
 * @param pa The toml_array to parse into.
 * @return Returns `true` only if all values were parsed successfully.
 */
NODISCARD
static bool toml_array_parse( toml_file *toml, toml_array *pa ) {
  assert( toml != NULL );
  assert( pa != NULL );

  unsigned    array_cap = 16;
  toml_array  a = { .values = MALLOC( toml_value, array_cap ) };
  int         c = '\0';
  bool        ok = false;
  char        prev_c;

  ++toml->array_depth;

  for (;;) {
    PJL_DISCARD_RV( toml_space_skip( toml ) );
    prev_c = STATIC_CAST( char, c );
    c = toml_getc( toml );
    switch ( c ) {
      case EOF:
        toml->error = TOML_ERR_UNEX_EOF;
        goto done;
      case '#':
        toml_comment_parse( toml );
        continue;
      case ',':
        if ( a.size == 0 || prev_c == ',' ) {
          toml->error = TOML_ERR_UNEX_CHAR;
          goto done;
        }
        continue;
      case ']':
        ok = true;
        goto done;
      default:
        toml_ungetc( toml, c );
        break;
    } // switch

    toml_value value;
    if ( !toml_value_parse( toml, &value ) )
      break;
    if ( a.size + 1 >= array_cap )
      REALLOC( a.values, array_cap *= 2 );
    a.values[ a.size++ ] = value;
  } // for

done:
  --toml->array_depth;
  if ( ok )
    *pa = a;
  else
    toml_array_cleanup( &a );
  return ok;
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
  bool const next_c_is_ok = c == EOF || !is_ident( c );

  if ( bytes_read < bytes_want ||
       !next_c_is_ok ||
       ( is_f && strncmp( buf, "false", STRLITLEN( "false" ) ) != 0) ||
       (!is_f && strncmp( buf, "true" , STRLITLEN( "true"  ) ) != 0) ) {
    toml_col_inc( toml, 1 );            // so col is at first char of value
    toml->error = TOML_ERR_UNEX_VALUE;
    return false;
  }

  toml_col_inc( toml, STATIC_CAST( unsigned, bytes_read ) );
  *pb = !is_f;
  return true;
}

/**
 * Parses a character.
 *
 * @param toml The toml_file to use.
 * @param want_c The character wanted.
 * @return Returns `true` only if \a want_c was parsed successfully.
 */
NODISCARD
static bool toml_char_parse( toml_file *toml, char want_c ) {
  assert( toml != NULL );

  int const got_c = toml_getc( toml );
  if ( got_c == want_c )
    return true;

  switch ( got_c ) {
    case EOF:
      toml->error = TOML_ERR_UNEX_EOF;
      break;
    case '\n':
    case '\r':
      toml->error = TOML_ERR_UNEX_NEWLINE;
      break;
    default:
      toml->error = TOML_ERR_UNEX_CHAR;
      break;
  } // switch
  return false;
}

/**
 * Parses a TOML comment.
 *
 * @param toml The toml_file to use.
 */
static void toml_comment_parse( toml_file *toml ) {
  assert( toml != NULL );

  for ( int c; (c = fgetc( toml->file )) != EOF; ) {
    if ( c == '\n' ) {
      toml_newline( toml );
      break;
    }
  } // for
}

/**
 * Gets the next character, if any.
 *
 * @param toml The toml_file to get the next character from.
 * @return Returns the next character or `EOF`.
 */
NODISCARD
static int toml_getc( toml_file *toml ) {
  int const c = fgetc( toml->file );

  if ( c != EOF ) {
    if ( toml->c_last == '\n' )
      toml_newline( toml );
    toml_col_inc( toml, 1 );
    toml->c_last = c;
  }

  return c;
}

/**
 * Parses a TOML integer.
 *
 * @param toml The toml_file to use.
 * @param pi A pointer to receive the integer.
 * @return Returns `true` only if an integer was parsed successfully.
 */
NODISCARD
static bool toml_int_parse( toml_file *toml, long *pi ) {
  assert( toml != NULL );
  assert( pi != NULL );

  char    buf[ MAX_DEC_INT_DIGITS( long ) + 1/*'\0'*/ ];
  size_t  buf_len = 0;

  int   base = 10;
  int   c = toml_getc( toml );
  char  prev_c;

  switch ( c ) {                        // can't be EOF
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

        case '#':
        case ',':
        case ']':
        case ' ':
        case '\n':
        case '\r':
        case '\t':
          toml_ungetc( toml, c );
          FALLTHROUGH;
        case EOF:
          *pi = 0;
          return true;

        default:
          toml->error = TOML_ERR_INT_INVALID;
          return false;
      } // switch
      break;

    default:
      toml_ungetc( toml, c );
  } // switch

  for (;;) {
    prev_c = STATIC_CAST( char, c );
    c = toml_getc( toml );
    switch ( c ) {
      case '#':
      case ',':
      case ']':
      case ' ':
      case '\n':
      case '\r':
      case '\t':
        toml_ungetc( toml, c );
        FALLTHROUGH;
      case EOF:
        if ( prev_c == '_' )
          goto error;
        goto done;
      case '_':
        continue;
    } // switch

    switch ( base ) {
      case 2:
        if ( !isbdigit( c ) )
          goto error;
        break;
      case 8:
        if ( !isodigit( c ) )
          goto error;
        break;
      case 10:
        if ( !isdigit( c ) )
          goto error;
        break;
      case 16:
        if ( !isxdigit( c ) )
          goto error;
        break;
    } // switch

    if ( buf_len + 1 == sizeof buf - 1 ) {
      toml->error = TOML_ERR_INT_RANGE;
      return false;
    }
    buf[ buf_len++ ] = STATIC_CAST( char, c );
  } // for

done:
  buf[ buf_len ] = '\0';
  errno = 0;
  long const value = strtol( buf, /*endptr=*/NULL, base );
  if ( errno == 0 ) {
    *pi = value;
    return true;
  }

error:
  toml->error = TOML_ERR_INT_INVALID;
  return false;
}

/**
 * Parses a TOML key.
 *
 * @param toml The toml_file to use.
 * @param pkey The string to receive the key.  The caller is responsible for
 * freeing it.
 * @param pkey_col If not NULL, a pointer to receive the key's column.
 * @param pkey_len If not NULL, a pointer to receive the key's length.
 * @return Returns `true` only if a key was parsed successfully.
 */
NODISCARD
static bool toml_key_parse( toml_file *toml, char **pkey, unsigned *pkey_col,
                            size_t *pkey_len ) {
  assert( toml != NULL );
  assert( pkey != NULL );

  static char const BARE_KEY_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "-._";

  int             c = toml_getc( toml );
  unsigned const  first_col = toml->loc.col;

  switch ( c ) {
    case '"':
      return toml_string_parse( toml, pkey );
    case '.':
      toml->error = TOML_ERR_KEY_INVALID;
      toml->error_msg = "bare key can not begin with '.'";
      return false;
    case EOF:
      return false;
  } // switch

  strbuf_t  key_buf;
  char      prev_c = '\0';

  strbuf_init( &key_buf );

  do {
    if ( is_toml_space( c ) ) {
      PJL_DISCARD_RV( toml_space_skip( toml ) );
      c = toml_getc( toml );
      if ( prev_c != '.' && c != '.' ) {
        toml_ungetc( toml, c );
        break;
      }
    }

    if ( strchr( BARE_KEY_CHARS, c ) == NULL ) {
      toml_ungetc( toml, c );
      break;
    }

    prev_c = STATIC_CAST( char, c );
    strbuf_putc( &key_buf, prev_c );
    c = toml_getc( toml );
  } while ( c != EOF );

  if ( key_buf.len == 0 ) {
    toml->error = TOML_ERR_KEY_INVALID;
    toml->error_msg = "empty key";
    goto error;
  }

  if ( key_buf.str[ key_buf.len - 1 ] == '.' ) {
    toml->loc.col = first_col + STATIC_CAST( unsigned, key_buf.len ) - 1;
    toml->error = TOML_ERR_KEY_INVALID;
    toml->error_msg = "bare key can not end with '.'";
    goto error;
  }

  if ( pkey_col != NULL )
    *pkey_col = first_col;
  if ( pkey_len != NULL )
    *pkey_len = key_buf.len;
  *pkey = strbuf_take( &key_buf );
  return true;

error:
  strbuf_cleanup( &key_buf );
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
  toml_loc    key_loc = toml->loc;
  toml_value  value = { 0 };

  if ( !toml_key_parse( toml, &key, /*pkey_col=*/NULL, /*pkey_len=*/NULL ) )
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
    *kv = (toml_key_value){ .key = key, .key_loc = key_loc, .value = value };
  else
    free( key );

  return ok;
}

/**
 * Skips all whitespace and comments.
 *
 * @param toml The toml_file to use.
 */
static void toml_space_comments_skip( toml_file *toml ) {
  assert( toml != NULL );
  for (;;) {
    PJL_DISCARD_RV( toml_space_skip( toml ) );
    int const c = toml_getc( toml );
    if ( c == EOF )
      break;
    if ( c != '#' ) {
      toml_ungetc( toml, c );
      break;
    }
    toml_comment_parse( toml );
  } // for
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
        toml->error = TOML_ERR_UNEX_NEWLINE;
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
 *
 * @note Assumes the caller has already parsed the `"`.
 */
NODISCARD
static bool toml_string_parse( toml_file *toml, char **ps ) {
  assert( toml != NULL );
  assert( ps != NULL );

  strbuf_t sbuf;
  strbuf_init( &sbuf );

  for (;;) {
    int c = toml_getc( toml );
    switch ( c ) {
      case EOF:
        goto eof;
      case '\r':
      case '\n':
        toml->error = TOML_ERR_STR_INVALID;
        toml->error_msg = "unterminated string";
        goto error;
      case '"':
        goto done;
      case '\\':
        c = toml_getc( toml );
        switch ( c ) {
          case EOF  : goto eof;
          case '"'  : c = '"';  break;
          case 'b'  : c = '\b'; break;
          case 'e'  : c = 0x1B; break;
          case 'f'  : c = '\f'; break;
          case 'n'  : c = '\n'; break;
          case 'r'  : c = '\r'; break;
          case 't'  : c = '\t'; break;
          case '\\' : c = '\\'; break;
          default:
            toml->error = TOML_ERR_STR_INVALID;
            toml->error_msg = "invalid escape sequence";
            goto error;
        } // switch
        break;
    } // switch

    strbuf_putc( &sbuf, STATIC_CAST( char, c ) );
  } // for

done:
  *ps = strbuf_take( &sbuf );
  return true;

eof:
  toml->error = TOML_ERR_UNEX_EOF;
error:
  strbuf_cleanup( &sbuf );
  return false;
}

/**
 * Parses a table name.
 *
 * @param toml The toml_file to use.
 * @param pname A pointer to receive the table name.
 * @param pname_len If not NULL, a pointer to receive the name's length.
 * @return Returns `true` only if a table name was parsed successfully.
 *
 * @note Assumes the caller has already parsed the `[`.
 */
NODISCARD
static bool toml_table_name_parse( toml_file *toml, char **pname,
                                   unsigned *pname_col, size_t *pname_len ) {
  assert( toml != NULL );
  assert( pname != NULL );

  char *key = NULL;

  bool const ok =
    toml_space_skip( toml ) &&
    toml_key_parse( toml, &key, pname_col, pname_len ) &&
    toml_space_skip( toml ) &&
    toml_char_parse( toml, ']' );

  if ( ok )
    *pname = key;
  else
    free( key );

  return ok;
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
    assert( toml->loc.line > 0 );
    --toml->loc.line;
  }
  toml->loc.col = toml->col_prev;
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

/**
 * Parses a TOML value.
 *
 * @param toml The toml_file to use.
 * @param v The toml_value to receive into.
 * @return Returns `true` only if the value parsed successfully.
 */
NODISCARD
static bool toml_value_parse( toml_file *toml, toml_value *v ) {
  assert( toml != NULL );
  assert( v != NULL );

  for (;;) {
    toml_loc const value_loc = toml->loc;
    int const c = toml_getc( toml );
    switch ( c ) {
      case '"':;
        char *s;
        if ( !toml_string_parse( toml, &s ) )
          return false;
        *v = (toml_value){ .type = TOML_STRING, .loc = value_loc, .s = s };
        return true;

      case '#':
        toml_comment_parse( toml );
        continue;

      case '+':
      case '-':
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
        if ( !toml_int_parse( toml, &i ) )
          return false;
        *v = (toml_value){ .type = TOML_INT, .loc = value_loc, .i = i };
        return true;

      case 'f':
      case 't':
        toml_ungetc( toml, c );
        bool b;
        if ( !toml_bool_parse( toml, &b ) )
          return false;
        *v = (toml_value){ .type = TOML_BOOL, .loc = value_loc, .b = b };
        return true;

      case '[':;
        toml_array a;
        if ( !toml_array_parse( toml, &a ) )
          return false;
        *v = (toml_value){ .type = TOML_ARRAY, .loc = value_loc, .a = a };
        return true;

      default:
        toml->error = TOML_ERR_UNEX_CHAR;
        return false;
    } // switch
  } // for
}

////////// extern functions ///////////////////////////////////////////////////

void toml_cleanup( toml_file *toml ) {
  if ( toml == NULL )
    return;
  // Table names are copied into the nodes, so nothing to free.
  rb_tree_cleanup( &toml->table_names, /*free_fn=*/NULL );
  *toml = (toml_file){ 0 };
}

char const* toml_error_msg( toml_file const *toml ) {
  assert( toml != NULL );
  if ( toml->error_msg != NULL )
    return toml->error_msg;
  assert( toml->error < ARRAY_SIZE( TOML_ERROR_MSGS ) );
  char const *const msg = TOML_ERROR_MSGS[ toml->error ];
  assert( msg != NULL );
  return msg;
}

void toml_init( toml_file *toml, FILE *file ) {
  assert( toml != NULL );
  assert( file != NULL );

  *toml = (toml_file){
    .file = file,
    .loc = { .line = 1, .col = 0 }
  };

  rb_tree_init(
    &toml->table_names, RB_DINT,
    POINTER_CAST( rb_cmp_fn_t, &strcmp )
  );
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

toml_value const* toml_table_find( toml_table const *table, char const *key ) {
  assert( table != NULL );
  assert( key != NULL );

  toml_key_value const kv = { .key = key };
  rb_node_t const *const found_rb = rb_tree_find( &table->keys_values, &kv );
  if ( found_rb == NULL )
    return NULL;
  toml_key_value const *const found_kv = RB_DINT( found_rb );
  return &found_kv->value;
}

void toml_table_init( toml_table *table ) {
  assert( table != NULL );
  table->name = NULL;
  rb_tree_init(
    &table->keys_values, RB_DINT,
    POINTER_CAST( rb_cmp_fn_t, toml_key_value_cmp )
  );
}

bool toml_table_next( toml_file *toml, toml_table *table ) {
  assert( toml != NULL );
  assert( table != NULL );

  toml_space_comments_skip( toml );
  toml_loc const table_loc = toml->loc;
  int c = toml_getc( toml );
  if ( c != '[' )
    return false;

  char     *table_name;
  unsigned  table_name_col;
  size_t    table_name_len;

  if ( !toml_table_name_parse( toml, &table_name, &table_name_col,
                               &table_name_len ) ) {
    return false;
  }

  toml_table_cleanup( table );

  rb_insert_rv_t rb_rbi =
    rb_tree_insert( &toml->table_names, table_name, table_name_len + 1 );
  if ( !rb_rbi.inserted ) {
    toml->error = TOML_ERR_TABLE_DUPLICATE;
    toml->loc.col = table_name_col;
    return false;
  }

  toml_table_init( table );
  table->name = table_name;
  table->loc = table_loc;

  for (;;) {
    toml_space_comments_skip( toml );
    c = fpeekc( toml->file );
    if ( c == EOF || c == '[' )
      return true;

    toml_key_value kv;
    if ( !toml_key_value_parse( toml, &kv ) )
      break;

    rb_rbi = rb_tree_insert( &table->keys_values, &kv, sizeof kv );
    if ( !rb_rbi.inserted ) {
      toml_key_value_cleanup( &kv );
      toml->error = TOML_ERR_KEY_DUPLICATE;
      break;
    }
  } // for

  toml_table_cleanup( table );
  return false;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/// @cond DOXYGEN_IGNORE

extern inline void toml_iterator_init( toml_table*, toml_iterator* );
extern inline toml_key_value* toml_iterator_next( toml_iterator* );
extern inline bool toml_table_empty( toml_table const* );

/// @endcond

/* vim:set et sw=2 ts=2: */
