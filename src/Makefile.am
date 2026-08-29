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
check_PROGRAMS =	hash_table_test \
			path_util_test \
			red_black_test \
			strbuf_test \
			toml_test

AM_CFLAGS =		$(INCLUDE_TIDY_CFLAGS) $(LIBCLANG_CPPFLAGS)

if ENABLE_ASAN
AM_CFLAGS +=            -fsanitize=address -fno-sanitize-recover=all
endif

if ENABLE_MSAN
AM_CFLAGS +=            -fsanitize=memory -fno-sanitize-recover=all
endif

if ENABLE_UBSAN
AM_CFLAGS +=            -fsanitize=undefined -fno-sanitize-recover=all
endif

AM_CPPFLAGS =		-I$(top_srcdir)/lib -I$(top_builddir)/lib
AM_LDFLAGS =		$(LIBCLANG_LDFLAGS) -Wl,-rpath,$(LIBCLANG_LIBDIR)
LDADD =			$(LIBCLANG_LIBS) $(top_builddir)/lib/libgnu.a
TESTS =         	$(check_PROGRAMS)

TIDY_FLAGS =		-Xtidy --verbose=f -Xtidy --error=never
TIDY_CFLAGS =		$(CFLAGS) $(AM_CFLAGS) $(CPPFLAGS) $(AM_CPPFLAGS)

include_tidy_SOURCES =	array.c array.h \
			bit_util.c bit_util.h \
			clang_util.c clang_util.h \
			cli_options.c cli_options.h \
			color.c color.h \
			config_file.c config_file.h \
			cxx.c cxx.h \
			file_ext.c file_ext.h \
			fnv1a.c fnv1a.h \
			hash_table.c hash_table.h \
			include.c include.h \
			include-tidy.c include-tidy.h \
			ipath.c ipath.h \
			options.c options.h \
			path_util.c path_util.h \
			pjl_config.h config.h \
			print.c print.h \
			proxies.c proxies.h \
			red_black.c red_black.h \
			strbuf.c strbuf.h \
			symbol.c symbol.h \
			toml_lite.c toml_lite.h \
			trans_unit.c trans_unit.h \
			typedef.c typedef.h \
			type_traits.h \
			util.c util.h

BASE_TEST_SOURCES =	pjl_config.h config.h \
			array.c array.h \
			fnv1a.c fnv1a.h \
			path_util.c path_util.h \
			strbuf.c strbuf.h \
			type_traits.h \
			util.c util.h \
			unit_test.c unit_test.h

hash_table_test_SOURCES=$(BASE_TEST_SOURCES) \
			hash_table.c hash_table.h \
			hash_table_test.c

path_util_test_SOURCES=	$(BASE_TEST_SOURCES) \
			path_util_test.c

red_black_test_SOURCES=	$(BASE_TEST_SOURCES) \
			red_black.c red_black.h \
			red_black_test.c

strbuf_test_SOURCES = 	$(BASE_TEST_SOURCES) \
			strbuf_test.c

toml_test_SOURCES = 	$(BASE_TEST_SOURCES) \
			hash_table.c hash_table.h \
			toml_lite.c toml_lite.h \
			toml_test.c

tidy: include-tidy
	@for file in *.h *.c; do \
	  [ ! -f "$$file" ] || \
	    include-tidy $(TIDY_FLAGS) $(TIDY_CFLAGS) "$$file" || exit $$?; \
	done

# vim:set noet sw=8 ts=8:
