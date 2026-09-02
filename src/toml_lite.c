/*
**      PJL Library
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
#include "fnv1a.h"
#include "hash_table.h"
#include "strbuf.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// @endcond

/**
 * @addtogroup toml-group
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

#define TOML_ARRAY_CAP_MIN        4     /**< Minimum array capacity. */
#define TOML_STRING_LEN_MAX       1024  /**< Maximum string length. */

/**
 * Rather than use a separate `bool is_newline_pending` in toml_file, we use a
 * special value in \ref toml_file::c_last "c_last".
 */
#define TOML_CHAR_PENDING_NEWLINE -2

////////// local constants ////////////////////////////////////////////////////

/**
 * TOML error messages.
 *
 * @note If \ref toml_file::error_msg is non-NULL, it overrides this message
 * with a more specific one.
 */
static char const *const TOML_ERROR_MSGS[] = {
  [ TOML_ERR_NONE             ] = "no error",
  [ TOML_ERR_DUPLICATE_KEY    ] = "duplicate key",
  [ TOML_ERR_DUPLICATE_TABLE  ] = "duplicate table",
  [ TOML_ERR_INVALID_CHAR     ] = "invalid character",
  [ TOML_ERR_INVALID_INT      ] = "invalid integer",
  [ TOML_ERR_INVALID_KEY      ] = "invalid key",
  [ TOML_ERR_INVALID_STRING   ] = "invalid string",
  [ TOML_ERR_UNEX_CHAR        ] = "unexpected character",
  [ TOML_ERR_UNEX_EOF         ] = "unexpected end of file",
  [ TOML_ERR_UNEX_VALUE       ] = "unexpected value",
};

/// @cond DOXYGEN_IGNORE

static char const TOML_ERR_BARE_KEY_NO_BEGIN_DOT[] =
  "bare key can not begin with '.'";
static char const TOML_ERR_BARE_KEY_NO_END_DOT[] =
  "bare key can not end with '.'";
static char const TOML_ERR_EMPTY_KEY[] = "empty key";
static char const TOML_ERR_INT_TOO_MANY_DIGITS[] =
  "integer has too many digits";
static char const TOML_ERR_INVALID_ESCAPE_SEQUENCE[] =
  "invalid escape sequence";
static char const TOML_ERR_UNEX_NEWLINE[] = "unexpected newline";
static char const TOML_ERR_UNTERMINATED_STRING[] = "unterminated string";

/// @endcond

////////// local functions ////////////////////////////////////////////////////

NODISCARD
static bool toml_space_skip( toml_file* ),
            toml_string_parse( toml_file*, strbuf_t* ),
            toml_value_parse( toml_file*, toml_value* );

NODISCARD
static int  toml_getc( toml_file* );

static void toml_comment_parse( toml_file* );
static void toml_space_comments_skip( toml_file* );
static void toml_value_cleanup( toml_value* );

////////// inline functions ///////////////////////////////////////////////////

/**
 * Gets whether \a c is a binary digit.
 *
 * @param c The character to check.
 * @return Returns `true` only if \a c is either `'0'` or `'1'`.
 *
 * @sa is_ident()
 * @sa isodigit()
 */
NODISCARD
static inline bool isbdigit( int c ) {
  return c == '0' || c == '1';
}

/**
 * Gets whether \a c is an octal digit.
 *
 * @param c The character to check.
 * @return Returns `true` only if \a c is one of `01234567`.
 *
 * @sa is_ident()
 * @sa isbdigit()
 */
NODISCARD
static inline bool isodigit( int c ) {
  return c >= '0' && c <= '7';
}

/**
 * Gets whether \a c is an identifier character, i.e., alphanumeric or `'_'`.
 *
 * @param c The character to check.
 * @return Returns `true` only if \a c is an identifier character.
 *
 * @sa isbdigit()
 * @sa isodigit()
 */
NODISCARD
static inline bool is_ident( int c ) {
  return isalnum( STATIC_CAST( unsigned char, c ) ) || c == '_';
}

/**
 * Increments the toml_file's column.
 *
 * @param toml The toml_file to use.
 */
static inline void toml_col_inc( toml_file *toml ) {
  toml->col_prev = toml->loc.col++;
}

/**
 * Gets whether \a c is an invalid TOML character.
 *
 * @param c The character to check.
 * @return Returns `true` only if \a c is invalid.
 */
NODISCARD
static bool toml_is_invalid_char( int c ) {
  return  (c >= 0x00 && c <= 0x08) || c == 0x0B || c == 0x0C ||
          (c >= 0x0E && c <= 0x1F) || c == 0x7F;
}

/**
 * Checks whether \a c is a whitespace character according to TOML.
 *
 * @param c The character to check.
 * @return Returns `true` only if \a c is a space.
 */
NODISCARD
static inline bool toml_is_space( int c ) {
  return c == ' ' || c == '\t';
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

/**
 * Ungets \a c.
 *
 * @remarks Storing the "ungotten" character ourselves is significantly faster
 * than calling **ungetc**(3).
 *
 * @param toml The toml_file to unget \a c.
 * @param c The character to unget.
 *
 * @sa toml_getc()
 * @sa toml_peekc()
 */
static inline void toml_ungetc( toml_file *toml, int c ) {
  toml->c_last = c;
  toml->loc.col = toml->col_prev;
}

/**
 * Peeks at the next character, if any.
 *
 * @param toml The toml_file to peek the next character from.
 * @return Returns the next character or `EOF`.
 *
 * @sa toml_getc()
 * @sa toml_ungetc()
 */
static inline int toml_peekc( toml_file *toml ) {
  int const c = toml_getc( toml );
  if ( c != EOF )
    toml_ungetc( toml, c );
  return c;
}

////////// local functions ////////////////////////////////////////////////////

/**
 * Cleans-up a toml_array.
 *
 * @param array The toml_array to clean up.  If NULL, does nothing.
 */
static void toml_array_cleanup( toml_array *array ) {
  if ( array != NULL ) {
    // Force hoist of array-> out of loop.
    size_t const size = array->size;
    toml_value *const values = array->values;

    for ( unsigned i = 0; i < size; ++i )
      toml_value_cleanup( &values[i] );
    free( values );
  }
}

/**
 * Parses a TOML array.
 *
 * @note Assumes the caller has already parsed the `[`.
 *
 * @param toml The toml_file to use.
 * @param rv_a The toml_array to parse into.
 * @return Returns `true` only if all values were parsed successfully.
 */
NODISCARD
static bool toml_array_parse( toml_file *toml, toml_array *rv_a ) {
  assert( toml != NULL );
  assert( rv_a != NULL );

  unsigned    array_cap = TOML_ARRAY_CAP_MIN;
  toml_array  array = { .values = MALLOC( toml_value, array_cap ) };
  int         c = '\0';
  bool        ok = false;
  bool        need_comma = false;

  ++toml->array_depth;

  for (;;) {
    PJL_DISCARD_RV( toml_space_skip( toml ) );
    c = toml_getc( toml );
    switch ( c ) {
      case EOF:
        toml->error = TOML_ERR_UNEX_EOF;
        goto done;
      case '#':
        toml_comment_parse( toml );
        continue;
      case ',':
        if ( !need_comma ) {
          toml->error = TOML_ERR_UNEX_CHAR;
          goto done;
        }
        need_comma = false;
        continue;
      case ']':
        ok = true;
        goto done;
      default:
        if ( need_comma ) {
          toml->error = TOML_ERR_UNEX_CHAR;
          goto done;
        }
        toml_ungetc( toml, c );
        break;
    } // switch

    toml_value value;
    if ( !toml_value_parse( toml, &value ) )
      break;
    if ( array.size + 1 >= array_cap ) {
      array_cap += array_cap >> 1;      // grow by ~1.5x
      REALLOC( array.values, array_cap );
    }
    array.values[ array.size++ ] = value;
    need_comma = true;
  } // for

done:
  --toml->array_depth;
  if ( ok )
    *rv_a = array;
  else
    toml_array_cleanup( &array );
  return ok;
}

/**
 * Parses a TOML Boolean value.
 *
 * @param toml The toml_file to use.
 * @param rv_b The `bool` to parse into.
 * @return Returns `true` only upon success.
 */
NODISCARD
static bool toml_bool_parse( toml_file *toml, bool *rv_b ) {
  assert( toml != NULL );
  assert( rv_b != NULL );

  toml_loc const start_loc = toml->loc;

  int         c = toml_getc( toml );
  bool const  is_t = c == 't';
  char const *want = is_t ? "rue" : "alse";

  for ( ; *want != '\0'; ++want ) {
    if ( toml_getc( toml ) != *want )
      goto error;
  } // for

  // Ensure it's not part of a longer identifier (e.g., "truest").
  c = toml_peekc( toml );
  if ( c != EOF && is_ident( c ) )
    goto error;

  *rv_b = is_t;
  return true;

error:
  toml->loc = start_loc;
  toml_col_inc( toml );
  toml->error = TOML_ERR_UNEX_VALUE;
  return false;
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
      toml->error_msg = TOML_ERR_UNEX_NEWLINE;
      FALLTHROUGH;
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
 *
 * @sa toml_peekc()
 * @sa toml_ungetc()
 */
NODISCARD
static int toml_getc( toml_file *toml ) {
  assert( toml != NULL );

  int c;
  bool const is_newline_pending = toml->c_last == TOML_CHAR_PENDING_NEWLINE;

  if ( !is_newline_pending && toml->c_last != EOF ) {
    c = toml->c_last;
  }
  else {
    c = fgetc( toml->file );
    if ( toml_is_invalid_char( c ) ) {
      toml->error = TOML_ERR_INVALID_CHAR;
      return EOF;
    }
  }

  toml->c_last = EOF;

  if ( c != EOF ) {
    if ( is_newline_pending )
      toml_newline( toml );
    if ( c == '\n' )
      toml->c_last = TOML_CHAR_PENDING_NEWLINE;
    toml_col_inc( toml );
  }

  return c;
}

/**
 * Parses a TOML integer.
 *
 * @param toml The toml_file to use.
 * @param rv_i The integer to parse into.
 * @return Returns `true` only if an integer was parsed successfully.
 */
NODISCARD
static bool toml_int_parse( toml_file *toml, long *rv_i ) {
  assert( toml != NULL );
  assert( rv_i != NULL );

  int     base = 10;
  char    buf[ MAX_DEC_INT_DIGITS( long ) + 1/*'\0'*/ ];
  size_t  buf_len = 0;
  int     c = toml_getc( toml );
  char    c_prev;

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
          *rv_i = 0;
          return true;

        default:
          toml->error = TOML_ERR_INVALID_INT;
          return false;
      } // switch
      break;

    default:
      toml_ungetc( toml, c );
  } // switch

  for (;;) {
    c_prev = STATIC_CAST( char, c );
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
        if ( c_prev == '_' )
          goto error;
        goto done;
      case '_':
        switch ( c_prev ) {
          case '+':
          case '-':
          case '_':
          case 'b':
          case 'o':
          case 'x':
            goto error;
        } // switch
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
      toml->error_msg = TOML_ERR_INT_TOO_MANY_DIGITS;
      goto error;
    }
    buf[ buf_len++ ] = STATIC_CAST( char, c );
  } // for

done:
  buf[ buf_len ] = '\0';
  errno = 0;
  long const value = strtol( buf, /*endptr=*/NULL, base );
  if ( errno == 0 ) {
    *rv_i = value;
    return true;
  }

error:
  toml->error = TOML_ERR_INVALID_INT;
  return false;
}

/**
 * Cleans-up a toml_key.
 *
 * @param key The toml_key to clean-up. If NULL, does nothing.
 */
static void toml_key_cleanup( toml_key *key ) {
  if ( key != NULL ) {
    FREE( key->name );
    key->name = NULL;
  }
}

/**
 * Parses a TOML key.
 *
 * @param toml The toml_file to use.
 * @param rv_key The key to parse into.
 * @param rv_key_len If not NULL, receives the key's length.
 * @return Returns `true` only if a key was parsed successfully.
 */
NODISCARD
static bool toml_key_parse( toml_file *toml, toml_key *rv_key,
                            size_t *rv_key_len ) {
  assert( toml != NULL );
  assert( rv_key != NULL );

  static char const BARE_KEY_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "-._";

  int             c = toml_getc( toml );
  unsigned const  first_col = toml->loc.col;
  strbuf_t        key_buf;

  strbuf_init( &key_buf );

  switch ( c ) {
    case '"':
      if ( !toml_string_parse( toml, &key_buf ) )
        return false;
      goto done;
    case '.':
      toml->error = TOML_ERR_INVALID_KEY;
      toml->error_msg = TOML_ERR_BARE_KEY_NO_BEGIN_DOT;
      return false;
    case EOF:
      return false;
  } // switch

  char c_prev = '\0';

  do {
    if ( toml_is_space( c ) ) {
      PJL_DISCARD_RV( toml_space_skip( toml ) );
      c = toml_getc( toml );
      if ( c_prev != '.' && c != '.' ) {
        toml_ungetc( toml, c );
        break;
      }
    }

    if ( strchr( BARE_KEY_CHARS, c ) == NULL ) {
      toml_ungetc( toml, c );
      break;
    }

    c_prev = STATIC_CAST( char, c );
    strbuf_putc( &key_buf, c_prev );
    c = toml_getc( toml );
  } while ( c != EOF );

  if ( key_buf.len == 0 ) {
    toml->error = TOML_ERR_INVALID_KEY;
    toml->error_msg = TOML_ERR_EMPTY_KEY;
    goto error;
  }

  if ( key_buf.str[ key_buf.len - 1 ] == '.' ) {
    toml->loc.col = first_col + STATIC_CAST( unsigned, key_buf.len ) - 1;
    toml->error = TOML_ERR_INVALID_KEY;
    toml->error_msg = TOML_ERR_BARE_KEY_NO_END_DOT;
    goto error;
  }

done:
  if ( rv_key_len != NULL )
    *rv_key_len = key_buf.len;
  rv_key->name = strbuf_take( &key_buf );
  rv_key->loc.col = first_col;
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
  if ( kv != NULL ) {
    toml_key_cleanup( &kv->key );
    toml_value_cleanup( &kv->value );
  }
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
  return strcmp( i_kv->key.name, j_kv->key.name );
}

/**
 * Calculates the hash of \a kv.
 *
 * @param kv The toml_key_value to hash.
 * @return Returns said hash.
 */
NODISCARD
static ht_hash_val_t toml_key_value_hash( toml_key_value const *kv ) {
  return fnv1a_s( kv->key.name );
}

/**
 * Parses a TOML _key_ `=` _value_.
 *
 * @param toml The toml_file to use.
 * @param rv_kv The toml_key_value to parse into.
 * @return Returns `true` only if both a key and value were parsed
 * successfully.
 */
NODISCARD
static bool toml_key_value_parse( toml_file *toml, toml_key_value *rv_kv ) {
  assert( toml != NULL );
  assert( rv_kv != NULL );

  toml_key    key = { .loc = toml->loc };
  toml_value  value = { 0 };

  if ( !toml_key_parse( toml, &key, /*rv_key_len=*/NULL ) )
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
    *rv_kv = (toml_key_value){ .key = key, .value = value };
  else
    toml_key_cleanup( &key );

  return ok;
}

/**
 * Skips all whitespace and comments.
 *
 * @param toml The toml_file to use.
 *
 * @sa toml_space_skip()
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
 *
 * @sa toml_space_comments_skip()
 */
NODISCARD
static bool toml_space_skip( toml_file *toml ) {
  assert( toml != NULL );

  for ( int c; (c = toml_getc( toml )) != EOF; ) {
    if ( c == '\n' ) {
      if ( toml->in_key_value && toml->array_depth == 0 ) {
        toml->error = TOML_ERR_UNEX_CHAR;
        toml->error_msg = TOML_ERR_UNEX_NEWLINE;
        return false;
      }
    }
    else if ( !toml_is_space( c ) ) {
      toml_ungetc( toml, c );
      break;
    }
  } // for

  return true;
}

/**
 * Parses a TOML string.
 *
 * @note Assumes the caller has already parsed the `"`.
 *
 * @param toml The toml_file to use.
 * @param rv_sbuf The strbuf_t to parse the string into.
 * @return Returns `true` only if a string was parsed successfully.
 */
NODISCARD
static bool toml_string_parse( toml_file *toml, strbuf_t *rv_sbuf ) {
  assert( toml != NULL );
  assert( rv_sbuf != NULL );

  strbuf_t sbuf;
  strbuf_init( &sbuf );

  for (;;) {
    int c = toml_getc( toml );
    switch ( c ) {
      case EOF:
        goto eof;
      case '\r':
      case '\n':
        toml->error = TOML_ERR_INVALID_STRING;
        toml->error_msg = TOML_ERR_UNTERMINATED_STRING;
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
            toml->error = TOML_ERR_INVALID_STRING;
            toml->error_msg = TOML_ERR_INVALID_ESCAPE_SEQUENCE;
            goto error;
        } // switch
        break;
    } // switch

    strbuf_putc( &sbuf, STATIC_CAST( char, c ) );
  } // for

done:
  *rv_sbuf = sbuf;
  return true;

eof:
  toml->error = TOML_ERR_UNEX_EOF;
error:
  strbuf_cleanup( &sbuf );
  return false;
}

/**
 * Parses a table header.
 *
 * @note Assumes the caller has already parsed the `[`.
 *
 * @param toml The toml_file to use.
 * @param rv_key The toml_key to parse into.
 * @param rv_name_len Receives the table name's length.
 * @return Returns `true` only if a table name was parsed successfully.
 */
NODISCARD
static bool toml_table_header_parse( toml_file *toml, toml_key *rv_key,
                                     size_t *rv_name_len ) {
  assert( toml != NULL );
  assert( rv_key != NULL );
  assert( rv_name_len != NULL );

  toml_key key = { 0 };

  bool const ok =
    toml_space_skip( toml ) &&
    toml_key_parse( toml, &key, rv_name_len ) &&
    toml_space_skip( toml ) &&
    toml_char_parse( toml, ']' );

  if ( ok )
    *rv_key = key;
  else
    toml_key_cleanup( &key );

  return ok;
}

/**
 * Cleans-up a toml_value.
 *
 * @param value The toml_value to clean-up.  If NULL, does nothing.
 */
static void toml_value_cleanup( toml_value *value ) {
  if ( value != NULL ) {
    switch ( value->type ) {
      case TOML_ARRAY:
        toml_array_cleanup( &value->a );
        break;
      case TOML_BOOL:
      case TOML_INT:
        // nothing to do
        break;
      case TOML_STRING:
        free( value->s );
        break;
    } // switch
  }
}

/**
 * Parses a TOML value.
 *
 * @param toml The toml_file to use.
 * @param rv_value The toml_value to parse into.
 * @return Returns `true` only if the value parsed successfully.
 */
NODISCARD
static bool toml_value_parse( toml_file *toml, toml_value *rv_value ) {
  assert( toml != NULL );
  assert( rv_value != NULL );

  for (;;) {
    int const c = toml_getc( toml );
    toml_loc const value_loc = toml->loc;
    switch ( c ) {
      case '"':;
        strbuf_t sbuf;
        strbuf_init( &sbuf );
        if ( !toml_string_parse( toml, &sbuf ) )
          return false;
        *rv_value = (toml_value){
          .type = TOML_STRING,
          .loc = value_loc,
          .s = strbuf_take( &sbuf )
        };
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
        *rv_value = (toml_value){
          .type = TOML_INT,
          .loc = value_loc,
          .i = i
        };
        return true;

      case 'f':
      case 't':
        toml_ungetc( toml, c );
        bool b;
        if ( !toml_bool_parse( toml, &b ) )
          return false;
        *rv_value = (toml_value){
          .type = TOML_BOOL,
          .loc = value_loc,
          .b = b
        };
        return true;

      case '[':;
        toml_array a;
        if ( !toml_array_parse( toml, &a ) )
          return false;
        *rv_value = (toml_value){
          .type = TOML_ARRAY,
          .loc = value_loc,
          .a = a
        };
        return true;

      default:
        toml->error = TOML_ERR_UNEX_CHAR;
        return false;
    } // switch
  } // for
}

////////// extern functions ///////////////////////////////////////////////////

char const* toml_error_msg( toml_file const *toml ) {
  assert( toml != NULL );
  if ( toml->error_msg != NULL )
    return toml->error_msg;
  assert( toml->error < ARRAY_SIZE( TOML_ERROR_MSGS ) );
  char const *const msg = TOML_ERROR_MSGS[ toml->error ];
  assert( msg != NULL );
  return msg;
}

void toml_file_cleanup( toml_file *toml ) {
  if ( toml != NULL ) {
    // Table names are copied into the entries, so nothing to free.
    ht_cleanup( &toml->table_names, /*free_fn=*/NULL );
    *toml = (toml_file){ 0 };
  }
}

void toml_file_init( toml_file *toml, FILE *file ) {
  assert( toml != NULL );
  assert( file != NULL );

  *toml = (toml_file){
    .c_last = EOF,
    .file = file,
    .loc = {
      .line = 1,
      // Explicitly initialize col to 0 so it doesn't look like an omission.
      // Upon reading the first character, this will be incremented to 1.
      .col = 0
    }
  };

  ht_init(
    &toml->table_names, HT_DINT, 2.0, 32,
    POINTER_CAST( ht_cmp_fn_t, &strcmp ),
    POINTER_CAST( ht_hash_fn_t, &fnv1a_s )
  );
}

void toml_table_cleanup( toml_table *table ) {
  if ( table != NULL ) {
    toml_key_cleanup( &table->key );
    ht_cleanup(
      &table->keys_values,
      POINTER_CAST( ht_free_fn_t, &toml_key_value_cleanup )
    );
  }
}

toml_value const* toml_table_find( toml_table const *table, char const *key ) {
  assert( table != NULL );
  assert( key != NULL );

  toml_key_value const kv = { .key = { .name = key } };
  ht_entry_t const *const found_ht = ht_find( &table->keys_values, &kv );
  if ( found_ht == NULL )
    return NULL;
  toml_key_value const *const found_kv = HT_DINT( found_ht );
  return &found_kv->value;
}

void toml_table_init( toml_table *table ) {
  assert( table != NULL );
  table->key = (toml_key){ 0 };
  ht_init(
    &table->keys_values, HT_DINT, 2.0, 64,
    POINTER_CAST( ht_cmp_fn_t, &toml_key_value_cmp ),
    POINTER_CAST( ht_hash_fn_t, &toml_key_value_hash )
  );
}

bool toml_table_next( toml_file *toml, toml_table *table ) {
  assert( toml != NULL );
  assert( table != NULL );

  toml_space_comments_skip( toml );
  toml_loc const header_loc = toml->loc;
  int c = toml_getc( toml );
  if ( c != '[' ) {
    toml_ungetc( toml, c );
    return false;
  }

  toml_key  table_key;
  size_t    table_name_len;

  if ( !toml_table_header_parse( toml, &table_key, &table_name_len ) )
    return false;

  toml_table_cleanup( table );

  ht_insert_rv_t hti = ht_insert(
    &toml->table_names, CONST_CAST( char*, table_key.name ), table_name_len + 1
  );
  if ( !hti.inserted ) {
    toml->error = TOML_ERR_DUPLICATE_TABLE;
    toml->loc.col = table_key.loc.col;
    toml_key_cleanup( &table_key );
    return false;
  }

  toml_table_init( table );
  table->key = (toml_key){ .name = table_key.name, .loc = header_loc };

  for (;;) {
    toml_space_comments_skip( toml );
    c = toml_peekc( toml );
    if ( c == EOF || c == '[' )
      return true;

    toml_key_value kv;
    if ( !toml_key_value_parse( toml, &kv ) )
      break;

    hti = ht_insert( &table->keys_values, &kv, sizeof kv );
    if ( !hti.inserted ) {
      toml->loc = kv.key.loc;
      toml->error = TOML_ERR_DUPLICATE_KEY;
      toml_key_value_cleanup( &kv );
      break;
    }
  } // for

  toml_table_cleanup( table );
  return false;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/// @cond DOXYGEN_IGNORE

extern inline void toml_iterator_init( toml_iterator*, toml_table* );
extern inline toml_key_value const* toml_iterator_next( toml_iterator* );
extern inline bool toml_table_empty( toml_table const* );

/// @endcond

/* vim:set et sw=2 ts=2: */
