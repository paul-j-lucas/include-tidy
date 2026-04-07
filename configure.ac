# Process this file with autoconf to produce a configure script.

##
#       include-tidy -- #include tidier
#       configure.ac
#
#       Copyright (C) 2026  Paul J. Lucas
#
#       This program is free software: you can redistribute it and/or modify
#       it under the terms of the GNU General Public License as published by
#       the Free Software Foundation, either version 3 of the License, or
#       (at your option) any later version.
#
#       This program is distributed in the hope that it will be useful,
#       but WITHOUT ANY WARRANTY; without even the implied warranty of
#       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#       GNU General Public License for more details.
#
#       You should have received a copy of the GNU General Public License
#       along with this program.  If not, see <http://www.gnu.org/licenses/>.
##

AC_PREREQ([2.69])
AC_INIT([include-tidy], [0.1], [https://github.com/paul-j-lucas/include-tidy/issues],,[https://github.com/paul-j-lucas/include-tidy])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_LANG(C)
AC_PROG_CC
AS_IF([test "x$ac_cv_prog_cc_c11" = xno],
  [AC_MSG_ERROR([a C11 compiler is required to compile $PACKAGE_NAME])])
gl_EARLY
AC_PROG_INSTALL

# External dependency: libclang
AC_ARG_WITH([llvm-config],
  [AS_HELP_STRING([--with-llvm-config=PATH], [path to llvm-config program])],
  [
    AS_CASE(["x$with_llvm_config"],
      [xno],  [AC_MSG_ERROR([llvm-config is required])],
      [xyes], [:],
      [LLVM_CONFIG="$with_llvm_config"] # User provided a path
    )
  ]
)
AC_PATH_PROGS([LLVM_CONFIG], [llvm-config], [no])
AS_IF([test x$LLVM_CONFIG = xno || test ! -x "$LLVM_CONFIG"], [
  AC_MSG_ERROR([llvm-config not found; install LLVM development packages])
])

LIBCLANG_CPPFLAGS=`$LLVM_CONFIG --cppflags`
LIBCLANG_LIBDIR=`$LLVM_CONFIG --libdir`
LIBCLANG_LDFLAGS=`$LLVM_CONFIG --ldflags`
LIBCLANG_LIBS="-lclang"

# Check host type.
AC_CANONICAL_HOST
case "x$host_os" in
xdarwin*)
  AC_DEFINE([SHLIB_PATH_VAR], [DYLD_LIBRARY_PATH], [Define to platform shared library path variable.])
  ;;
*)
  AC_DEFINE([SHLIB_PATH_VAR], [LD_LIBRARY_PATH], [Define to platform shared library path variable.])
  ;;
esac

# Checks for headers.
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$LIBCLANG_CPPFLAGS $CPPFLAGS"
AC_CHECK_HEADER([clang-c/Index.h], [], [
  AC_MSG_ERROR([libclang headers not found; use --with-libclang=DIR])
])
CPPFLAGS="$save_CPPFLAGS"
AC_CHECK_HEADERS([getopt.h])
AC_CHECK_HEADERS([pwd.h])
AC_CHECK_HEADERS([sysexits.h])
AC_HEADER_ASSERT
AC_HEADER_STDBOOL
gl_INIT

# Checks for typedefs, structures, and compiler characteristics.
AC_C_TYPEOF
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_CHECK_MEMBERS([struct passwd.pw_dir],[],[],[[#include <pwd.h>]])
PJL_COMPILE([__builtin_expect],[], [(void)__builtin_expect(1,1);])
PJL_TYPE_UNSIGNED_INT128

# Checks for library functions.
AC_CHECK_FUNCS([geteuid getpwuid perror strerror strndup])
save_LDFLAGS="$LDFLAGS"
save_LIBS="$LIBS"
LDFLAGS="$LIBCLANG_LDFLAGS $LDFLAGS"
LIBS="$LIBCLANG_LIBS $LIBS"
AC_CHECK_LIB([clang], [clang_createIndex], [:], [
  AC_MSG_ERROR([libclang library not found; use --with-libclang=DIR])
])
LDFLAGS="$save_LDFLAGS"
LIBS="$save_LIBS"

# Makefile conditionals.

# Miscellaneous.
AX_C___ATTRIBUTE__

# Compiler warnings.
INCLUDE_TIDY_CFLAGS="-D_ISOC11_SOURCE"
AX_CFLAGS_WARN_ALL([INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wcast-align], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wcast-function-type], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wcomma], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wconditional-type-mismatch], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wconditional-uninitialized], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wconversion], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wduplicate-enum], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wembedded-directive], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wenum-enum-conversion], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wenum-float-conversion], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wextra], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wfloat-equal], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wfor-loop-analysis], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wformat-nonliteral], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wformat-signedness], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wformat-type-confusion], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wformat=2], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Widiomatic-parentheses], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wimplicit-fallthrough], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wlogical-op-parentheses], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wmisleading-indentation], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wnewline-eof], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wredundant-decls], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wreserved-identifier], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wshadow], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wshift-sign-overflow], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wshorten-64-to-32], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wsign-compare], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wsign-conversion], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wsometimes-uninitialized], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wstring-conversion], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wtautological-compare], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wtautological-type-limit-compare], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wundef], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wuninitialized], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wunreachable-code-break], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wunreachable-code-return], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wunreachable-code], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wunused], [INCLUDE_TIDY_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wwrite-strings], [INCLUDE_TIDY_CFLAGS])

# Generate files.
AC_SUBST([INCLUDE_TIDY_CFLAGS])
AC_SUBST([LIBCLANG_CPPFLAGS])
AC_SUBST([LIBCLANG_LDFLAGS])
AC_SUBST([LIBCLANG_LIBDIR])
AC_SUBST([LIBCLANG_LIBS])
AH_TOP([#ifndef include_tidy_config_H
#define include_tidy_config_H])
AH_BOTTOM([#endif /* include_tidy_config_H */])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_FILES([
  Makefile
  etc/Makefile
  lib/Makefile
  src/Makefile
  man/Makefile
  man/man1/Makefile
  test/Makefile
])
AC_OUTPUT

# vim:set et sw=2 ts=2:
