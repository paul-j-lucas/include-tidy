#! /usr/bin/env bash
##
#       include-tidy -- #include tidier
#       test/bin/no-include-end-cc.sh
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

# To test include-tidy's handling of a compiler's failure to emit include paths
# bracketed by:
#
#     #include <...> search starts here:
#     ...
#     End of search list.
#
# this script is used as an argument to --compiler to be a bogus "compiler"
# that emits the "start" line but no "end" line.

echo '#include <...> search starts here:'
exit 0

# vim:set et sw=2 ts=2:
