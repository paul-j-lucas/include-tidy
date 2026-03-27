##
# SYNOPSIS
#
#     PJL_TYPE_UNSIGNED_INT128
#
# DESCRIPTION
#
#     Check for unsigned __int128 support.
#
# LICENSE
#
#     Copyright (C) 2026  Paul J. Lucas
#
#     This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
#
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
#
#     You should have received a copy of the GNU General Public License
#     along with this program.  If not, see <http://www.gnu.org/licenses/>.
##

#serial 1

AC_DEFUN([PJL_TYPE_UNSIGNED_INT128], [
  AC_CACHE_CHECK([for unsigned __int128 support], [pjl_cv_type_uint128],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[]], [[
        unsigned __int128 h = 1234, p = 5678;
        unsigned char c = 'c';
        h = p * (h ^ c);
        return h == 6819278;
      ]])],
      [pjl_cv_type_uint128=yes],
      [pjl_cv_type_uint128=no]
    )]
  )

  AS_IF([test "x$pjl_cv_type_uint128" = xyes], [
    AC_DEFINE([HAVE_UNSIGNED_INT128], [1], [Define if the compiler supports unsigned __int128])
  ], [
    AC_DEFINE([HAVE_UNSIGNED_INT128], [0], [Define if the compiler supports unsigned __int128])
  ])
])

dnl vim:set et sw=2 ts=2:
