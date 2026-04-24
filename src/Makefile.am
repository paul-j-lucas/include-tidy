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

# Suppress printing of "--- target ---" when -j is given to BSD's make.
.MAKE.JOB.PREFIX=

bin_PROGRAMS =		include-tidy
check_PROGRAMS =	red_black_test strbuf_test toml_test util_test

AM_CFLAGS =		$(INCLUDE_TIDY_CFLAGS) $(LIBCLANG_CPPFLAGS)
AM_CPPFLAGS =		-I$(top_srcdir)/lib -I$(top_builddir)/lib
AM_LDFLAGS =		$(LIBCLANG_LDFLAGS) -Wl,-rpath,$(LIBCLANG_LIBDIR)
LDADD =			$(LIBCLANG_LIBS) $(top_builddir)/lib/libgnu.a
TESTS =         	$(check_PROGRAMS)

INCLUDE_TIDY_FLAGS =	-Xtidy --verbose=f -Xtidy --error=never

include_tidy_SOURCES =	array.c array.h \
			clang_util.c clang_util.h \
			cli_options.c cli_options.h \
			color.c color.h \
			config_file.c config_file.h \
			include-tidy.c include-tidy.h \
			includes.c includes.h \
			options.c options.h \
			pjl_config.h config.h \
			print.c print.h \
			red_black.c red_black.h \
			strbuf.c strbuf.h \
			symbols.c symbols.h \
			toml_lite.c toml_lite.h \
			trans_unit.c trans_unit.h \
			type_traits.h \
			util.c util.h

BASE_TEST_SOURCES =	pjl_config.h config.h \
			array.c array.h \
			strbuf.c strbuf.h \
			type_traits.h \
			util.c util.h \
			unit_test.c unit_test.h

red_black_test_SOURCES=	$(BASE_TEST_SOURCES) \
			red_black.c red_black.h \
			red_black_test.c

strbuf_test_SOURCES = 	$(BASE_TEST_SOURCES) \
			strbuf_test.c

toml_test_SOURCES = 	$(BASE_TEST_SOURCES) \
			red_black.c red_black.h \
			toml_lite.c toml_lite.h \
			toml_test.c

util_test_SOURCES =	$(BASE_TEST_SOURCES) \
			util_test.c

tidy:
	@touch *.c
	@$(MAKE) -s CC="include-tidy" CFLAGS="$(INCLUDE_TIDY_FLAGS)"

# vim:set noet sw=8 ts=8:
