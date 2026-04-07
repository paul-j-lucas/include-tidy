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

# Command-line option tests
TESTS=	tests/cli_options.test \
	tests/cli_options-error.test

TESTS+=	tests/hello.test

###############################################################################

BUILD_SRC = $(abs_top_builddir)/src
AM_TESTS_ENVIRONMENT = BUILD_SRC=$(BUILD_SRC) ; export BUILD_SRC ;
TEST_EXTENSIONS = .test

TEST_LOG_DRIVER = $(srcdir)/run_test.sh

EXTRA_DIST = run_test.sh tests data expected
dist-hook:
	cd $(distdir)/tests && rm -f *.log *.trs

UPDATE_TEST = $(srcdir)/update_test.sh
update:
	@ls $(TESTS) | xargs -n255 -r $(UPDATE_TEST) -s $(BUILD_SRC)

# vim:set noet sw=8 ts=8:
