##
#	include-tidy -- #include tidier
#	test/Makefile.am
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

AUTOMAKE_OPTIONS = 1.12			# needed for TEST_LOG_DRIVER

###############################################################################

# Basic sanity tests
TESTS=	tests/hello.test

# Command-line option tests
TESTS+=	tests/cli-compiler-no-exists.test \
	tests/cli-compiler-no-include-end.test \
	tests/cli-compiler-no-include-start.test \
	tests/cli-directory-error.test \
	tests/cli-error-always.test \
	tests/cli-help.test \
	tests/cli-options.test \
	tests/cli-options-error.test \
	tests/cli-usage.test \
	tests/libclang-error.test

# Configuration file tests
TESTS+=	tests/config.test \
	tests/config-associated-header.test \
	tests/config-bad-semantics-1.test \
	tests/config-bad-semantics-2.test \
	tests/config-bad-toml.test \
	tests/config-default.test \
	tests/config-proxy-cycle.test \
	tests/config-elide-includes.test \
	tests/config-enoent.test \
	tests/config-first.test \
	tests/config-handling-error.test \
	tests/config-ignore.test \
	tests/config-ignore-symbols.test \
	tests/config-keep.test

# Other tests
TESTS+=	tests/home.test \
	tests/ipaths.test \
	tests/no-config.test \
	tests/symlink.test \
	tests/xdg_config_home.test \
	tests/xdg_config_dirs-1.test \
	tests/xdg_config_dirs-2.test

# Error tests
TESTS+=	tests/duplicate.test \
	tests/enoent.test \
	tests/error.test \
	tests/include-enoent.test \
	tests/missing.test \
	tests/unknown_extension.test \
	tests/unnecessary.test

# Real-world C tests.
TESTS+=	tests/c_ast.test \
	tests/c_ast_util.test \
	tests/c_type.test \
	tests/c_typedef.test \
	tests/cdecl_dym.test \
	tests/dump.test \
	tests/english.test \
	tests/p_macro.test \
	tests/red_black.test \
	tests/show.test \
	tests/slist.test \
	tests/types.test

# C++ tests.
TESTS+=	tests/cxx.test \
	tests/conf_filter.test \
	tests/conf_string.test \
	tests/conf_var_init.test \
	tests/Derived-02.test \
	tests/derived_conf_string_cpp.test \
	tests/derived_conf_string_hpp.test \
	tests/derived_map.test \
	tests/Derived-MI.test \
	tests/derived_set-2.test \
	tests/derived_set.test \
	tests/elements_cpp.test \
	tests/entities_hpp.test \
	tests/error_string.test \
	tests/exit_codes.test \
	tests/file_info_cpp.test \
	tests/file_info_hpp.test \
	tests/has_cxx_qualifier_proxy.test \
	tests/hash.test \
	tests/macro.test \
	tests/namespace_type.test \
	tests/query_node.test \
	tests/timespec_t.test \
	tests/using_set.test

###############################################################################

ABS_TOP_BUILDDIR =	$(abs_top_builddir)
EXTRA_DIST =		bin data expected tests
TEST_EXTENSIONS =	.test
TEST_LOG_DRIVER =	$(srcdir)/bin/run_test.sh
TOP_SRCDIR =		$(top_srcdir)
UPDATE_TEST =		$(srcdir)/bin/update_test.sh

AM_TESTS_ENVIRONMENT = TOP_SRCDIR=$(TOP_SRCDIR) ABS_TOP_BUILDDIR=$(ABS_TOP_BUILDDIR) ; export TOP_SRCDIR ABS_TOP_BUILDDIR ;

.PHONY: top-clean-coverage

top-clean-coverage:
if ENABLE_COVERAGE
	cd $(top_builddir) && $(MAKE) $(AM_MAKEFLAGS) clean-coverage
endif

dist-hook:
	cd $(distdir)/tests && rm -f *.log *.trs

update: top-clean-coverage
	@ls $(TESTS) | xargs -n255 -r \
	$(UPDATE_TEST) -B "$(ABS_TOP_BUILDDIR)" -s "$(TOP_SRCDIR)"

# vim:set noet sw=8 ts=8:
