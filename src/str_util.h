/*
**      include-tidy -- #include tidier
**      src/str_util.h
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

#ifndef pjl_str_util_h
#define pjl_str_util_h

/**
 * @file
 * Declares string utility functions.
 */

// local
#include "pjl_config.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <stdbool.h>
#include <string.h>

/// @endcond

/**
 * @defgroup str-util-group String Utility Functions
 * String utility functions.
 * @{
 */

////////// extern functions ///////////////////////////////////////////////////

/**
 * Checks whether \a s is null: if so, returns the empty string.
 *
 * @param s The pointer to check.
 * @return If \a s is null, returns the empty string; otherwise returns \a s.
 *
 * @sa null_if_empty()
 */
NODISCARD
inline char const* empty_if_null( char const *s ) {
  return s == NULL ? "" : s;
}

/// @cond DOXYGEN_IGNORE
// LCOV_EXCL_START
NODISCARD
inline char* nonconst_empty_if_null( char *s ) {
  return CONST_CAST( char*, empty_if_null( s ) );
}
// LCOV_EXCL_STOP

#define empty_if_null(S)          NONCONST_OVERLOAD( empty_if_null, (S) )
/// @endcond

/**
 * Checks whether \a s is null, an empty string, or consists only of
 * whitespace.
 *
 * @param s The null-terminated string to check.
 * @return If \a s is either null or the empty string, returns NULL; otherwise
 * returns a pointer to the first non-whitespace character in \a s.
 *
 * @sa empty_if_null()
 */
NODISCARD
inline char const* null_if_empty( char const *s ) {
  return s != NULL && *SKIP_CHARS( s, WS_CHARS ) == '\0' ? NULL : s;
}

/// @cond DOXYGEN_IGNORE
// LCOV_EXCL_START
NODISCARD
inline char* nonconst_null_if_empty( char *s ) {
  return CONST_CAST( char*, null_if_empty( s ) );
}
// LCOV_EXCL_STOP

#define null_if_empty(S)          NONCONST_OVERLOAD( null_if_empty, (S) )
/// @endcond

/**
 * Calls **strdup**(3) and checks for failure.
 *
 * @remarks If memory allocation fails, prints an error message and exits.
 *
 * @param s The null-terminated string to duplicate.
 * @return Returns a copy of \a s.
 */
NODISCARD
char* strdup_or_exit( char const *s );

/**
 * Gets whether \a s ends with \a end.
 *
 * @param s The string to check.
 * @param end The end string.
 * @param end_len The length of \a end.
 * @return Returns `true` only if \a s ends with \a end.
 */
NODISCARD
inline bool str_ends_with( char const *s, char const *end, size_t end_len ) {
  size_t const s_len = strlen( s );
  return s_len >= end_len && strcmp( s + s_len - end_len, end ) == 0;
}

/**
 * Gets whether \a s is among \a strings.
 *
 * @param s The string to look for.
 * @param strings The NULL terminated array of strings to look at.
 * @return Returns `true` only if \a s is among \a strings.
 */
NODISCARD
bool str_is_any( char const *s, char const *const strings[static 1] );

/**
 * Convenience macro for calling str_is_any() constructing a compound array of
 * string literals ending with NULL.
 *
 * @param S The string to look for.
 * @param ... The strings to look at.
 * @return Returns `true` only if \a S is among ...
 */
#define str_is_any_list(S,...)                                        \
  str_is_any( (S),                                                    \
              (char const*[]){ __VA_ARGS__ VA_OPT( (,), __VA_ARGS__ ) \
                               (void*)0 } )

/**
 * Trims both leading and trailing whitespace from a string.
 *
 * @param s The string to trim whitespace from.
 * @param n The length of \a s.
 * @return Returns a pointer to within \a s having all whitespace trimmed.
 *
 * @warning If \a s starts with whitespace, the pointer returned is within \a
 * s. If \a s was dynamically allocated, \a s --- and not a pointer with \a s
 * --- must be freed.
 */
NODISCARD
char* str_trim( char *s, size_t n );

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* pjl_str_util_h */
/* vim:set et sw=2 ts=2: */
