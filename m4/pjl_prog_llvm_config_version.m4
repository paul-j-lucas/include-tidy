##
# SYNOPSIS
#
#     AX_PROG_LLVM_CONFIG_VERSION([version],[action-if-true],[action-if-false])
#
# DESCRIPTION
#
#     Ensures that the llvm-config version is greater or equal to the version
#     specified. If true, the shell commands in action-if-true are executed;
#     otherwise, the shell commands in commands in action-if-false are.
#
#     Note: the variable LLVM_CONFIG must be set (for example, by running
#     AC_CHECK_PROG or AC_PATH_PROG), this macro will fail.
#
# PARAMETERS
#
#     $1  Minimum llvm-config version
#     $2  Action-if-true
#     $3  Action-if-false
#
# EXAMPLE:
#
#     AC_PATH_PROG([LLVM_CONFIG],[llvm-config])
#     PJL_PROG_LLVM_CONFIG_VERSION([21.0.0],[ ... ],[ ... ])
#
# LICENSE
#
#     Copyright (C) 2026  Paul J. Lucas
#
#     This program is free software: you can redistribute it and/or modify it
#     under the terms of the GNU General Public License as published by the
#     Free Software Foundation, either version 3 of the License, or (at your
#     option) any later version.
#
#     This program is distributed in the hope that it will be useful, but
#     WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
#     Public License for more details.
#
#     You should have received a copy of the GNU General Public License along
#     with this program.  If not, see <http://www.gnu.org/licenses/>.
##

#serial 1

AC_DEFUN([PJL_PROG_LLVM_CONFIG_VERSION],[
  AC_REQUIRE([AC_PROG_SED])
  AC_REQUIRE([AC_PROG_GREP])

  AS_IF([test -n "$LLVM_CONFIG"],[
    ax_llvm_version="$1"

    AC_MSG_CHECKING([for llvm-config version])
    llvm_version=`$LLVM_CONFIG --version 2>&1`
    AC_MSG_RESULT($llvm_version)
    AC_SUBST([LLVM_VERSION],[$llvm_version])

    AX_COMPARE_VERSION([$llvm_version],[ge],[$ax_llvm_version],[
        :
        $2
    ],[
        :
        $3
    ])
  ],[
    AC_MSG_WARN([could not find llvm-config])
    $3
  ])
])

dnl vim:set et sw=2 ts=2:
