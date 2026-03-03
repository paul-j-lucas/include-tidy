##
#	include-tidy -- #include tidier
#	src/Makefile.am
#
#	Copyright (C) 2026  Paul J. Lucas
#
#	This program is free software: you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation, either version 3 of the License, or
#	(at your option) any later version.
#
#	This program is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with this program.  If not, see <http://www.gnu.org/licenses/>.
##

bin_PROGRAMS =		include-tidy

AM_CFLAGS =		$(INCLUDE_TIDY_CFLAGS)
AM_CPPFLAGS =		-I$(top_srcdir)/lib -I$(top_builddir)/lib
LDADD =			-lclang $(top_builddir)/lib/libgnu.a

include_tidy_SOURCES =	include-tidy.c include-tidy.h \
			pjl_config.h config.h \
			type_traits.h \
			util.c util.h

# vim:set noet sw=8 ts=8:
