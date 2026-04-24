/*
**      include-tidy -- #include tidier
**      src/util.h
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

#ifndef include_tidy_util_H
#define include_tidy_util_H

/**
 * @file
 * Declares utility constants, macros, and functions.
 */

// local
#include "pjl_config.h"
#include "type_traits.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <errno.h>
#include <limits.h>                     /* for CHAR_BIT, PATH_MAX */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>                      /* for FILE */
#include <stdlib.h>                     /* for exit(3) */
#include <string.h>                     /* for strspn(3) */
#include <sysexits.h>

/// @endcond

/**
 * @defgroup util-group Utility Macros & Functions
 * Utility macros, constants, and functions.
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * Gets whether the argument(s) contains a comma, that is there are 2 or more
 * arguments.
 *
 * @param ... Zero to 10 arguments, invariably `__VA_ARGS__`.
 * @return Returns `0` for 0 or 1 argument, or `1` for 2 or more arguments.
 */
#define ARGS_HAS_COMMA(...) \
  ARG_11( __VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0 )

/// @cond DOXYGEN_IGNORE
#define ARG_11(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,...) _11
/// @endcond

/**
 * Gets whether there are no arguments.
 *
 * @param ... Zero to 10 arguments, invariably `__VA_ARGS__`.
 * @return Returns `0` for 0 arguments or `1` otherwise.
 *
 * @sa https://stackoverflow.com/a/66556553/99089
 * @sa https://gustedt.wordpress.com/2010/06/08/detect-empty-macro-arguments/
 */
#define ARGS_IS_EMPTY(...)                                                  \
  ARGS_IS_EMPTY_CASES(                                                      \
    /*  Case 1: argument with a comma,                                      \
        e.g. "ARG1, ARG2", "ARG1, ...", or ",". */                          \
    ARGS_HAS_COMMA( __VA_ARGS__ ),                                          \
    /*  Case 2: argument within parentheses,                                \
        e.g., "(ARG)", "(...)", or "()". */                                 \
    ARGS_HAS_COMMA( ARGS_IS_EMPTY_COMMA __VA_ARGS__ ),                      \
    /*  Case 3: argument that is a macro that will expand the parentheses,  \
        possibly generating a comma. */                                     \
    ARGS_HAS_COMMA( __VA_ARGS__ () ),                                       \
    /*  Case 4: __VA_ARGS__ doesn't generate a comma by itself, nor with    \
        ARGS_IS_EMPTY_COMMA behind it, nor with () after it.  Therefore,    \
        "ARGS_IS_EMPTY_COMMA __VA_ARGS__ ()" generates a comma only if      \
        __VA_ARGS__ is empty.  So this is the empty __VA_ARGS__ case since  \
        the previous cases are false. */                                    \
    ARGS_HAS_COMMA( ARGS_IS_EMPTY_COMMA __VA_ARGS__ () )                    \
  )

/// @cond DOXYGEN_IGNORE
#define ARGS_IS_EMPTY_CASES(_1,_2,_3,_4) \
  ARGS_HAS_COMMA( NAME5( ARGS_IS_EMPTY_RESULT_, _1, _2, _3, _4 ) )
#define ARGS_IS_EMPTY_COMMA(...)  ,
#define ARGS_IS_EMPTY_RESULT_0001 ,
#define NAME5(A,B,C,D,E)          NAME5_HELPER( A, B, C, D, E )
#define NAME5_HELPER(A,B,C,D,E)   A ## B ## C ## D ## E
/// @endcond

/// @cond DOXYGEN_IGNORE

#define CHARIFY_0 '0'
#define CHARIFY_1 '1'
#define CHARIFY_2 '2'
#define CHARIFY_3 '3'
#define CHARIFY_4 '4'
#define CHARIFY_5 '5'
#define CHARIFY_6 '6'
#define CHARIFY_7 '7'
#define CHARIFY_8 '8'
#define CHARIFY_9 '9'
#define CHARIFY_A 'A'
#define CHARIFY_B 'B'
#define CHARIFY_C 'C'
#define CHARIFY_D 'D'
#define CHARIFY_E 'E'
#define CHARIFY_F 'F'
#define CHARIFY_G 'G'
#define CHARIFY_H 'H'
#define CHARIFY_I 'I'
#define CHARIFY_J 'J'
#define CHARIFY_K 'K'
#define CHARIFY_L 'L'
#define CHARIFY_M 'M'
#define CHARIFY_N 'N'
#define CHARIFY_O 'O'
#define CHARIFY_P 'P'
#define CHARIFY_Q 'Q'
#define CHARIFY_R 'R'
#define CHARIFY_S 'S'
#define CHARIFY_T 'T'
#define CHARIFY_U 'U'
#define CHARIFY_V 'V'
#define CHARIFY_W 'W'
#define CHARIFY_X 'X'
#define CHARIFY_Y 'Y'
#define CHARIFY_Z 'Z'
#define CHARIFY__ '_'
#define CHARIFY_a 'a'
#define CHARIFY_b 'b'
#define CHARIFY_c 'c'
#define CHARIFY_d 'd'
#define CHARIFY_e 'e'
#define CHARIFY_f 'f'
#define CHARIFY_g 'g'
#define CHARIFY_h 'h'
#define CHARIFY_i 'i'
#define CHARIFY_j 'j'
#define CHARIFY_k 'k'
#define CHARIFY_l 'l'
#define CHARIFY_m 'm'
#define CHARIFY_n 'n'
#define CHARIFY_o 'o'
#define CHARIFY_p 'p'
#define CHARIFY_q 'q'
#define CHARIFY_r 'r'
#define CHARIFY_s 's'
#define CHARIFY_t 't'
#define CHARIFY_u 'u'
#define CHARIFY_v 'v'
#define CHARIFY_w 'w'
#define CHARIFY_x 'x'
#define CHARIFY_y 'y'
#define CHARIFY_z 'z'

#define CHARIFY_IMPL(X)           CHARIFY_##X
#define STRINGIFY_IMPL(X)         #X

/// @endcond

/**
 * Gets a pointer to one element past the last of the given array.
 *
 * @param ARRAY The array to use.
 * @return Returns a pointer to one element past the last of \a ARRAY.
 *
 * @note \a ARRAY _must_ be a statically allocated array.
 *
 * @sa #ARRAY_SIZE()
 * @sa #FOREACH_ARRAY_ELEMENT()
 */
#define ARRAY_END(ARRAY)          ( (ARRAY) + ARRAY_SIZE( (ARRAY) ) )

/**
 * Gets the number of elements of the given array.
 *
 * @param A The array to get the number of elements of.
 *
 * @note \a A _must_ be a statically allocated array.
 *
 * @sa #ARRAY_END()
 * @sa #FOREACH_ARRAY_ELEMENT()
 */
#define ARRAY_SIZE(A) (     \
  sizeof(A) / sizeof(0[A])  \
  * STATIC_ASSERT_EXPR( IS_ARRAY_EXPR(A), #A " must be an array" ))

#ifndef NDEBUG
/**
 * Asserts that this line of code is run at most once --- useful in
 * initialization functions that must be called at most once.  For example:
 *
 *      void initialize() {
 *        ASSERT_RUN_ONCE();
 *        // ...
 *      }
 *
 * @sa #RUN_ONCE
 */
#define ASSERT_RUN_ONCE() BLOCK(    \
  static bool UNIQUE_NAME(called);  \
  assert( !UNIQUE_NAME(called) );   \
  UNIQUE_NAME(called) = true; )
#else
#define ASSERT_RUN_ONCE()         NO_OP
#endif /* NDEBUG */

/**
 * Calls **atexit**(3) and checks for failure.
 *
 * @param FN The pointer to the function to call **atexit**(3) with.
 */
#define ATEXIT(FN)                PERROR_EXIT_IF( atexit( FN ) != 0, EX_OSERR )

/**
 * Embeds the given statements into a compound statement block.
 *
 * @param ... The statement(s) to embed.
 */
#define BLOCK(...)                do { __VA_ARGS__ } while (0)

/**
 * Macro that "char-ifies" its argument, e.g., <code>%CHARIFY(x)</code> becomes
 * `'x'`.
 *
 * @param X The unquoted character to charify.  It can be only in the set
 * `[0-9_A-Za-z]`.
 *
 * @sa #STRINGIFY()
 */
#define CHARIFY(X)                CHARIFY_IMPL(X)

/**
 * C version of C++'s `const_cast`.
 *
 * @param T The type to cast to.
 * @param EXPR The expression to cast.
 *
 * @note This macro can't actually implement C++'s `const_cast` because there's
 * no way to do it in C.  It serves merely as a visual cue for the type of cast
 * meant.
 *
 * @sa #POINTER_CAST()
 * @sa #STATIC_CAST()
 */
#define CONST_CAST(T,EXPR)        ((T)(EXPR))

/**
 * Shorthand for printing to standard error.
 *
 * @param ... The `printf()` arguments.
 *
 * @sa #EPUTC()
 * @sa #EPUTS()
 * @sa #FPRINTF()
 * @sa #PRINTF()
 */
#define EPRINTF(...)              fprintf( stderr, __VA_ARGS__ )

/**
 * Shorthand for printing \a C to standard error.
 *
 * @param C The character to print.
 *
 * @sa #EPRINTF()
 * @sa #EPUTS()
 * @sa #FPUTC()
 * @sa #PUTC()
 */
#define EPUTC(C)                  FPUTC( C, stderr )

/**
 * Shorthand for printing a C string to standard error.
 *
 * @param S The C string to print.
 *
 * @sa #EPRINTF()
 * @sa #EPUTC()
 * @sa #FPUTS()
 * @sa #PUTS()
 */
#define EPUTS(S)                  fputs( (S), stderr )

/**
 * Convenience macro for iterating over the elements of a static array.
 *
 * @param TYPE The type of element.
 * @param VAR The element loop variable.
 * @param ARRAY The array to iterate over.
 *
 * @note \a ARRAY _must_ be a statically allocated array.
 *
 * @sa #ARRAY_END()
 * @sa #ARRAY_SIZE()
 */
#define FOREACH_ARRAY_ELEMENT(TYPE,VAR,ARRAY) \
  for ( TYPE const *VAR = (ARRAY); VAR < ARRAY_END( (ARRAY) ); ++VAR )

/**
 * Shorthand for printing to standard output.
 *
 * @param STREAM The `FILE` stream to print to.
 * @param ... The `printf()` arguments.
 *
 * @sa #EPRINTF()
 * @sa #FPUTC()
 * @sa #FPUTS()
 */
#define FPRINTF(STREAM,...) \
	PERROR_EXIT_IF( fprintf( (STREAM), __VA_ARGS__ ) < 0, EX_IOERR )

/**
 * Calls **putc**(3), checks for an error, and exits if there was one.
 *
 * @param C The character to print.
 * @param STREAM The `FILE` stream to print to.
 *
 * @sa #EPUTC()
 * @sa #FPRINTF()
 * @sa #FPUTS()
 * @sa #PUTC()
 */
#define FPUTC(C,STREAM) \
	PERROR_EXIT_IF( putc( (C), (STREAM) ) == EOF, EX_IOERR )

/**
 * Prints \a N spaces to \a STREAM.
 *
 * @param N The number of spaces to print.
 * @param STREAM The `FILE` stream to print to.
 *
 * @sa #FPUTS()
 */
#define FPUTNSP(N,STREAM) \
  FPRINTF( (STREAM), "%*s", STATIC_CAST( int, (N) ), "" )

/**
 * Calls **fputs**(3), checks for an error, and exits if there was one.
 *
 * @param S The C string to print.
 * @param STREAM The `FILE` stream to print to.
 *
 * @sa #EPUTS()
 * @sa #FPRINTF()
 * @sa #FPUTC()
 * @sa #PUTS()
 */
#define FPUTS(S,STREAM) \
	PERROR_EXIT_IF( fputs( (S), (STREAM) ) == EOF, EX_IOERR )

/**
 * Frees the given memory.
 *
 * @param PTR The pointer to the memory to free.
 *
 * @remarks
 * This macro exists since free'ing a pointer to `const` generates a warning.
 */
#define FREE(PTR)                 free( CONST_CAST( void*, (PTR) ) )

/**
 * A special-case of fatal_error() that additionally prints the file and line
 * where an internal error occurred.
 *
 * @param FORMAT The `printf()` format string literal to use.
 * @param ... The `printf()` arguments.
 *
 * @sa fatal_error()
 * @sa perror_exit()
 * @sa #PERROR_EXIT_IF()
 * @sa #UNEXPECTED_INT_VALUE()
 */
#define INTERNAL_ERROR(FORMAT,...)                            \
  fatal_error( EX_SOFTWARE,                                   \
    "%s:%d: internal error: " FORMAT,                         \
    __FILE__, __LINE__ VA_OPT( (,), __VA_ARGS__ ) __VA_ARGS__ \
  )

#ifdef HAVE___BUILTIN_EXPECT

/**
 * Specifies that \a EXPR is \e very likely (as in 99.99% of the time) to be
 * non-zero (true) allowing the compiler to better order code blocks for
 * magrinally better performance.
 *
 * @sa [Memory part 5: What programmers can do](http://lwn.net/Articles/255364/)
 */
#define likely(EXPR)              __builtin_expect( !!(EXPR), 1 )

/**
 * Specifies that \a EXPR is \e very unlikely (as in .01% of the time) to be
 * non-zero (true) allowing the compiler to better order code blocks for
 * magrinally better performance.
 *
 * @sa [Memory part 5: What programmers can do](http://lwn.net/Articles/255364/)
 */
#define unlikely(EXPR)            __builtin_expect( !!(EXPR), 0 )

#else
# define likely(EXPR)             (EXPR)
# define unlikely(EXPR)           (EXPR)
#endif /* HAVE___BUILTIN_EXPECT */

/**
 * Convenience macro for calling check_realloc().
 *
 * @param TYPE The type to allocate.
 * @param N The number of objects of \a TYPE to allocate.  It _must_ be &gt; 0.
 * @return Returns a pointer to \a N uninitialized objects of \a TYPE.
 *
 * @sa check_realloc()
 * @sa #REALLOC()
 */
#define MALLOC(TYPE,N) \
  check_realloc( NULL, sizeof(TYPE) * STATIC_CAST( size_t, (N) ) )

/**
 * Gets the number of characters needed to represent the largest magnitide
 * value of the integral \a TYPE in decimal.
 *
 * @remarks
 * @parblock
 * The number of decimal digits _d_ required to represent a binary number with
 * _b_ bits is:
 *
 *      d = ceil(b * log10(2))
 *
 * where _log10(2)_ &asymp; .30102999; hence multiply _b_ by .30102999.  Since
 * the compiler can't do floating-point math at compile-time, that has to be
 * simulated using only integer math.
 *
 * The expression 1233 / 4096 = .30102539 is a close approximation of
 * .30102999.  Integer division by 4096 is the same as right-shifting by 12.
 * The number of bits _b_ = <code>sizeof(</code><i>TYPE</i><code>)</code> *
 * `CHAR_BIT`.  Therefore, multiply that by 1233, then right-shift by 12.
 *
 * The `STATIC_ASSERT_EXPR` (if true) adds 1 that rounds up since shifting
 * truncates.  The `IS_SIGNED_TYPE` (if true) adds another 1 to accomodate the
 * possible `-` (minus sign) for a negative number if \a TYPE is signed.
 * @endparblock
 *
 * @param TYPE The integral type.
 *
 * @sa https://stackoverflow.com/a/13546502/99089
 */
#define MAX_DEC_INT_DIGITS(TYPE)                              \
  (((sizeof(TYPE) * CHAR_BIT * 1233) >> 12)                   \
    + STATIC_ASSERT_EXPR( IS_INTEGRAL_TYPE(TYPE),             \
                          #TYPE " must be an integral type" ) \
    + IS_SIGNED_TYPE(TYPE))

/**
 * Zeros the memory pointed to by \a PTR.  The number of bytes to zero is given
 * by `sizeof *(PTR)`.
 *
 * @param PTR The pointer to the start of memory to zero.  \a PTR must be a
 * pointer.  If it's an array, it'll generate a compile-time error.
 */
#define MEM_ZERO(PTR) BLOCK(                                        \
  static_assert( IS_POINTER_EXPR(PTR), #PTR " must be a pointer" ); \
  memset( (PTR), 0, sizeof *(PTR) ); )

/// @cond DOXYGEN_IGNORE
#define NAME2_HELPER(A,B)         A##B
/// @endcond

/**
 * Concatenate \a A and \a B together to form a single token.
 *
 * @remarks This macro is needed instead of simply using `##` when either
 * argument needs to be expanded first, e.g., `__LINE__`.
 *
 * @param A The first name.
 * @param B The second name.
 */
#define NAME2(A,B)                NAME2_HELPER(A,B)

/**
 * No-operation statement.  (Useful for a `goto` target.)
 */
#define NO_OP                     ((void)0)

/**
 * Overloads \a FN such that if \a PTR is a pointer to:
 *  + `const`, \a FN is called; or:
 *  + Non-`const`, `nonconst_`\a FN is called.
 *
 * @remarks
 * @parblock
 * Sometimes for a given function `f`, you want `f` to return:
 *
 *  + `R const*` when passed a `T const*`; or:
 *  + `R*` when passed a `T*`.
 *
 * That is you want the `const`-ness of `R` to match that of `T`.  In C++,
 * you'd simply overload `f`:
 *
 *      R const*  f( T const* );        // C++ only
 *      inline R* f( T *t ) {
 *        return const_cast<R*>( f( const_cast<T const*>( t ) ) );
 *      }
 *
 * In C, you'd need two differently named functions:
 *
 *      R const*  f( T const* );        // C
 *      inline R* nonconst_f( T *t ) {
 *        return (R*)f( t );
 *      }
 *
 * This macro allows `f` to be "overloaded" in C such that only `f` ever needs
 * to be called explicitly and either `f` or `nonconst_f` is actually called
 * depending on the `const`-ness of \a PTR.
 *
 * To use this macro:
 *
 *  1. Declare `f` as a function that takes a `T const*` and returns an `R
 *     const*`.
 *
 *  2. Declare `nonconst_f` as an `inline` function that takes a `T*` and
 *     returns an `R*` by calling `f` and casting the result to `R*`.
 *
 *  3. Define a macro also named `f` that expands into `NONCONST_OVERLOAD`
 *     like:
 *
 *          #define f(P,A2,A3)    NONCONST_OVERLOAD( f, (P), (A2), (A3) )
 *
 *     where <code>A</code><i>n</i> are additional arguments for `f`.
 *
 *  4. Define the function `f` with an extra set of `()` to prevent the macro
 *     `f` from expanding:
 *
 *          R const* (f)( T const *t, A2 a2, A3 a3 ) {
 *            // ...
 *          }
 * @endparblock
 *
 * @param FN The name of the function to overload.
 * @param PTR A pointer. If it's a pointer to:
 *  + `const`, \a FN is called; or:
 *  + Non-`const`, `nonconst_`\a FN is called.
 *
 * @param ... Additional arguments passed to \a FN.
 */
#define NONCONST_OVERLOAD(FN,PTR,...)       \
  STATIC_IF( IS_PTR_TO_CONST_EXPR( (PTR) ), \
    FN,                                     \
    NAME2(nonconst_,FN)                     \
  )( (PTR) VA_OPT( (,), __VA_ARGS__ ) __VA_ARGS__ )

/**
 * If \a EXPR is `true`, prints an error message for `errno` to standard error
 * and exits with status \a STATUS.
 *
 * @param EXPR The expression.
 * @param STATUS The exit status code.
 *
 * @sa fatal_error()
 * @sa #INTERNAL_ERROR()
 * @sa perror_exit()
 * @sa #UNEXPECTED_INT_VALUE()
 */
#define PERROR_EXIT_IF( EXPR, STATUS ) \
  BLOCK( if ( unlikely( EXPR ) ) perror_exit( STATUS ); )

/**
 * Cast either from or to a pointer type --- similar to C++'s
 * `reinterpret_cast`, but for pointers only.
 *
 * @param T The type to cast to.
 * @param EXPR The expression to cast.
 *
 * @note This macro silences a "cast to pointer from integer of different size"
 * warning.  In C++, this would be done via `reinterpret_cast`, but it's not
 * possible to implement that in C that works for both pointers and integers.
 *
 * @sa #CONST_CAST()
 * @sa #STATIC_CAST()
 */
#define POINTER_CAST(T,EXPR)      ((T)(uintptr_t)(EXPR))

/**
 * Calls #FPRINTF() with `stdout`.
 *
 * @param ... The `fprintf()` arguments.
 *
 * @sa #EPRINTF()
 * @sa #FPRINTF()
 * @sa #PUTC()
 * @sa #PUTS()
 */
#define PRINTF(...)               FPRINTF( stdout, __VA_ARGS__ )

/**
 * Calls #FPUTC() with `stdout`.
 *
 * @param C The character to print.
 *
 * @sa #EPUTC()
 * @sa #FPUTC()
 * @sa #PRINTF()
 * @sa #PUTS()
 */
#define PUTC(C)                   FPUTC( (C), stdout )

/**
 * Calls #FPUTS() with `stdout`.
 *
 * @param S The C string to print.
 *
 * @note Unlike **puts**(3), does _not_ print a newline.
 *
 * @sa #FPUTS()
 * @sa #PRINTF()
 * @sa #PUTC()
 */
#define PUTS(S)                   FPUTS( (S), stdout )

/**
 * Convenience macro for calling check_realloc().
 *
 * @param PTR The pointer to memory to reallocate.  It is set to the newly
 * reallocated memory.
 * @param N The number of objects of \a TYPE to reallocate.
 *
 * @sa check_realloc()
 * @sa #MALLOC()
 */
#define REALLOC(PTR,N) \
  ((PTR) = check_realloc( (PTR), sizeof *(PTR) * STATIC_CAST( size_t, (N) ) ))

/**
 * Runs a statement at most once even if control passes through it more than
 * once.  For example:
 *
 *      RUN_ONCE initialize();
 *
 * or:
 *
 *      RUN_ONCE {
 *        // ...
 *      }
 *
 * @sa #ASSERT_RUN_ONCE()
 */
#define RUN_ONCE                      \
  static bool UNIQUE_NAME(run_once);  \
  if ( likely( true_or_set( &UNIQUE_NAME(run_once) ) ) ) ; else

/**
 * Advances \a S over all \a CHARS.
 *
 * @param S The string pointer to advance.
 * @param CHARS A string containing the characters to skip over.
 * @return Returns the updated \a S.
 *
 * @sa #SKIP_WS()
 */
#define SKIP_CHARS(S,CHARS)       ((S) += strspn( (S), (CHARS) ))

/**
 * Advances \a S over all whitespace.
 *
 * @param S The string pointer to advance.
 * @return Returns the updated \a S.
 *
 * @sa #SKIP_CHARS()
 */
#define SKIP_WS(S)                SKIP_CHARS( (S), WS_CHARS )

/**
 * C version of C++'s `static_cast`.
 *
 * @param T The type to cast to.
 * @param EXPR The expression to cast.
 *
 * @note This macro can't actually implement C++'s `static_cast` because
 * there's no way to do it in C.  It serves merely as a visual cue for the type
 * of cast meant.
 *
 * @sa #CONST_CAST()
 * @sa #POINTER_CAST()
 */
#define STATIC_CAST(T,EXPR)       ((T)(EXPR))

/**
 * Shorthand for calling **strerror**(3) with `errno`.
 */
#define STRERROR()                strerror( errno )

/**
 * Macro that "string-ifies" its argument, e.g., <code>%STRINGIFY(x)</code>
 * becomes `"x"`.
 *
 * @param X The unquoted string to stringify.
 *
 * @note This macro is sometimes necessary in cases where it's mixed with uses
 * of `##` by forcing re-scanning for token substitution.
 *
 * @sa #CHARIFY()
 */
#define STRINGIFY(X)              STRINGIFY_IMPL(X)

/**
 * Strips the enclosing parentheses from \a ARG.
 *
 * @param ARG The argument.  It _must_ be enclosed within parentheses.
 * @return Returns \a ARG without enclosing parentheses.
 */
#define STRIP_PARENS(ARG)         STRIP_PARENS_HELPER ARG

/// @cond DOXYGEN_IGNORE
#define STRIP_PARENS_HELPER(...)  __VA_ARGS__
/// @endcond

/**
 * Gets the length of \a S.
 *
 * @param S The C string literal to get the length of.
 * @return Returns said length.
 */
#define STRLITLEN(S) \
  (ARRAY_SIZE( (S) ) \
   - STATIC_ASSERT_EXPR( IS_C_STR_EXPR( (S) ), #S " must be a C string literal" ))

/**
 * Calls **strncmp**(3) with #STRLITLEN(\a LIT) for the third argument.
 *
 * @param S The string to compare.
 * @param LIT The string literal to compare against.
 * @return Returns a number less than 0, 0, or greater than 0 if \a S is
 * less than, equal to, or greater than \a LIT, respectively.
 */
#define STRNCMPLIT(S,LIT)         strncmp( (S), (LIT), STRLITLEN( (LIT) ) )

/**
 * A special-case of #INTERNAL_ERROR() that prints an unexpected integer value.
 *
 * @param EXPR The expression having the unexpected value.
 *
 * @sa fatal_error()
 * @sa #INTERNAL_ERROR()
 * @sa perror_exit()
 * @sa #PERROR_EXIT_IF()
 */
#define UNEXPECTED_INT_VALUE(EXPR)                      \
  INTERNAL_ERROR(                                       \
    "%lld (0x%llX): unexpected value for " #EXPR "\n",  \
    STATIC_CAST( long long, (EXPR) ),                   \
    STATIC_CAST( unsigned long long, (EXPR) )           \
  )

/**
 * Synthesises a name prefixed by \a PREFIX unique to the line on which it's
 * used.
 *
 * @param PREFIX The prefix of the synthesized name.
 *
 * @warning All uses for a given \a PREFIX that refer to the same name _must_
 * be on the same line.  This is not a problem within macro definitions, but
 * won't work outside of them since there's no way to refer to a previously
 * used unique name.
 */
#define UNIQUE_NAME(PREFIX)       NAME2(NAME2(PREFIX,_),__LINE__)

/**
 * Pre-C23/C++20 substitution for `__VA_OPT__`, that is returns \a TOKENS only
 * if 1 or more additional arguments are passed.
 *
 * @remarks
 * @parblock
 * For compilers that don't yet support `__VA_OPT__`, instead of doing
 * something like:
 *
 *      ARG __VA_OPT__(,) __VA_ARGS__               // C23/C++20 way
 *
 * do this instead:
 *
 *      ARG VA_OPT( (,), __VA_ARGS__ ) __VA_ARGS__  // substitute way
 *
 * (It's unfortunately necessary to specify `__VA_ARGS__` twice.)
 * @endparblock
 *
 * @param TOKENS The token(s) possibly to be returned.  They _must_ be enclosed
 * within parentheses.
 * @param ... Zero to 10 arguments, invariably `__VA_ARGS__`.
 * @return Returns \a TOKENS (with enclosing parentheses stripped) followed by
 * `__VA_ARGS__` only if 1 or more additional arguments are passed; returns
 * nothing otherwise.
 */
#ifdef HAVE___VA_OPT__
# define VA_OPT(TOKENS,...) \
    __VA_OPT__( STRIP_PARENS( TOKENS ) )
#else
# define VA_OPT(TOKENS,...) \
    NAME2( VA_OPT_EMPTY_, ARGS_IS_EMPTY( __VA_ARGS__ ) )( TOKENS, __VA_ARGS__ )

  /// @cond DOXYGEN_IGNORE
# define VA_OPT_EMPTY_0(TOKENS,...) STRIP_PARENS(TOKENS)
# define VA_OPT_EMPTY_1(TOKENS,...) /* nothing */
  /// @endcond
#endif /* HAVE___VA_OPT__ */

////////// extern variables ///////////////////////////////////////////////////

/**
 * Whitespace characters.
 */
extern char const WS_CHARS[];

////////// extern functions ///////////////////////////////////////////////////

/**
 * Extracts the base portion of a path-name.
 * Unlike **basename**(3):
 *  + Trailing `/` characters are not deleted.
 *  + \a path_name is never modified (hence can therefore be `const`).
 *  + Returns a pointer within \a path_name (hence is multi-call safe).
 *
 * @param path_name The path-name to extract the base portion of.
 * @return Returns a pointer to the last component of \a path_name.
 * If \a path_name consists entirely of '/' characters, a pointer to the string
 * "/" is returned.
 */
NODISCARD
char const* base_name( char const *path_name );

/**
 * Calls **asprintf**(3) and checks for failure.
 *
 * @param ps A pointer to the string to receive the printed result.  The caller
 * is responsible for freeing it.
 * @param format The `printf()` style format string.
 * @return Returns the number of characters printed.
 */
PJL_DISCARD
PJL_PRINTF_LIKE_FUNC(2)
unsigned check_asprintf( char **ps, char const *format, ... );

/**
 * Calls **realloc**(3) and checks for failure.
 * If reallocation fails, prints an error message and exits.
 *
 * @param p The pointer to reallocate.  If NULL, new memory is allocated.
 * @param size The number of bytes to allocate.
 * @return Returns a pointer to the allocated memory.
 */
NODISCARD
void* check_realloc( void *p, size_t size );

/**
 * Calls **snprintf**(3) and checks for failure.
 *
 * @param buf The destination buffer to print into.
 * @param buf_size The size of \a buf.
 * @param format The `printf()` style format string.
 * @param ... The `printf()` arguments.
 */
PJL_PRINTF_LIKE_FUNC(3)
void check_snprintf( char *buf, size_t buf_size, char const *format, ... );

/**
 * Calls **strdup**(3) and checks for failure.
 * If memory allocation fails, prints an error message and exits.
 *
 * @param s The null-terminated string to duplicate.
 * @return Returns a copy of \a s.
 */
NODISCARD
char* check_strdup( char const *s );

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
NODISCARD
inline char* nonconst_empty_if_null( char *s ) {
  return CONST_CAST( char*, empty_if_null( s ) );
}

#define empty_if_null(S)          NONCONST_OVERLOAD( empty_if_null, (S) )
/// @endcond

/**
 * Checks \a flag: if `false`, sets it to `true`.
 *
 * @param flag A pointer to the Boolean flag to be tested and, if `false`,
 * sets it to `true`.
 * @return Returns `true` only if \a flag was `false` initially.
 *
 * @sa true_clear()
 * @sa true_or_set()
 */
NODISCARD
inline bool false_set( bool *flag ) {
  return !*flag && (*flag = true);
}

/**
 * Prints an error message to standard error and exits with \a status code.
 *
 * @param status The status code to exit with.
 * @param format The `printf()` format string literal to use.
 * @param ... The `printf()` arguments.
 *
 * @sa #INTERNAL_ERROR()
 * @sa perror_exit()
 * @sa #PERROR_EXIT_IF()
 * @sa #UNEXPECTED_INT_VALUE()
 */
PJL_PRINTF_LIKE_FUNC(2)
_Noreturn void fatal_error( int status, char const *format, ... );

/**
 * Like **free**(3) except frees the pointer pointed to by pptr.
 *
 * @param pptr The pointer to pointer to free.
 */
void free_pptr( void *pptr );

/**
 * Gets the absolute path of the current working directory.
 *
 * @param plen If not NULL, the length of the path is put here.
 * @return Returns the absolute path of the current working directory.
 */
NODISCARD
char const* get_cwd( size_t *plen );

/**
 * Checks whether \a s is null, an empty string, or consists only of
 * whitespace.
 *
 * @param s The null-terminated string to check.
 * @return If \a s is either null or the empty string, returns NULL; otherwise
 * returns a pointer to the first non-whitespace character in \a s.
 */
NODISCARD
inline char const* null_if_empty( char const *s ) {
  return s != NULL && *SKIP_CHARS( s, WS_CHARS ) == '\0' ? NULL : s;
}

/// @cond DOXYGEN_IGNORE
NODISCARD
inline char* nonconst_null_if_empty( char *s ) {
  return CONST_CAST( char*, null_if_empty( s ) );
}

#define null_if_empty(S)          NONCONST_OVERLOAD( null_if_empty, (S) )
/// @endcond

/**
 * Gets the filename extension of \a path, if any.
 *
 * @param path The path to get the filename extension of.
 * @return Returns a pointer into \a path pointing at the first character of
 * the extension (not the dot) or NULL if \a path has no extension.
 *
 * @sa path_no_ext()
 */
NODISCARD
char const* path_ext( char const *path );

/**
 * Gets whether \a path is an absolute path.
 *
 * @param path the path to check.
 * @return Returns `true` only if \a path is absolute.
 *
 * @sa path_is_relative()
 */
NODISCARD
inline bool path_is_absolute( char const *path ) {
  return path[0] == '/';
}

/**
 * Gets whether \a path is a relative path.
 *
 * @param path the path to check.
 * @return Returns `true` only if \a path is relative.
 *
 * @sa path_is_absolute()
 */
NODISCARD
inline bool path_is_relative( char const *path ) {
  return path[0] != '/';
}

/**
 * Strips leading dot-slashes, if any, from \a path.
 *
 * @param path The path to strip `./` from.
 * @return Returns \a path without leading `./`.
 */
char const* path_no_dot_slash( char const *path );

/**
 * Gets the pathname without the filename extension of \a path, if any.
 *
 * @param path The path.
 * @param path_buf A path buffer to use only if \a path has an extension.
 * @return If \a path has no extension, returns \a path as-is; otherwise copies
 * \a path into \a path_buf without the extension and returns \a path_buf.
 *
 * @sa path_ext()
 */
NODISCARD
char const* path_no_ext( char const *path, char path_buf[static PATH_MAX] );

/**
 * Normalizes a path by:
 *
 *  + Making a relative path absolute.
 *  + Eliminating all occurrences of `./` or `../`.
 *
 * @param path The path to normalize.
 * @return Returns a normalized path.  The caller is responsible for freeing
 * it.
 */
char* path_normalize( char const *path );

/**
 * Prints an error message for `errno` to standard error and exits.
 *
 * @param status The exit status code.
 *
 * @sa fatal_error()
 * @sa #INTERNAL_ERROR()
 * @sa #PERROR_EXIT_IF()
 * @sa #UNEXPECTED_INT_VALUE()
 */
_Noreturn void perror_exit( int status );

#ifndef NDEBUG
/**
 * Checks whether \a s is an affirmative value.  An affirmative value is one of
 * 1, t, true, y, or yes, case-insensitive.
 *
 * @param s The null-terminated string to check or null.
 * @return Returns `true` only if \a s is affirmative.
 */
NODISCARD
bool str_is_affirmative( char const *s );
#endif /* NDEBUG */

/**
 * A variant of **strncpy**(3) that always null-terminates \a dst.
 *
 * @param dst A pointer to receive the copy of \a src.  It _must_ be at least
 * \a n + 1 bytes long.
 * @param src The null-terminated string to copy.
 * @param n The number of bytes at most to copy.
 * @return Returns \a dst.
 */
PJL_DISCARD
inline char* strncpy_0( char *restrict dst, char const *restrict src,
                        size_t n ) {
  snprintf( dst, n + 1, "%s", src );
  return dst;
}

/**
 * Compares two string pointers by comparing the string pointed to.
 *
 * @param i_ps The first string pointer to compare.
 * @param j_ps The first string pointer to compare.
 * @return Returns a number less than 0, 0, or greater than 0 if \a *i_ps is
 * less than, equal to, or greater than \a *j_ps, respectively.
 */
NODISCARD
int str_ptr_cmp( char const **i_ps, char const **j_ps );

/**
 * Trims both leading and trailing whitespace from a string.
 *
 * @param s The string to trim whitespace from.
 * @return Returns a pointer to within \a s having all whitespace trimmed.
 */
NODISCARD
char* str_trim( char *s );

/**
 * Checks \a flag: if `false`, sets it to `true`.
 *
 * @param flag A pointer to the Boolean flag to be tested and, if `false`, sets
 * it to `true`.
 * @return Returns `true` only if \a flag was `true` initially.
 *
 * @sa false_set()
 * @sa true_clear()
 */
NODISCARD
inline bool true_or_set( bool *flag ) {
  return *flag || !(*flag = true);
}

/**
 * Checks the flag: if `true`, resets the flag to `false`.
 *
 * @param flag A pointer to the Boolean flag to be tested and, if `true`, set
 * to \c false.
 * @return Returns `true` only if \a *flag is `true`.
 *
 * @sa false_set()
 * @sa true_or_set()
 */
NODISCARD
inline bool true_clear( bool *flag ) {
  return *flag && !(*flag = false);
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* include_tidy_util_H */
/* vim:set et sw=2 ts=2: */
