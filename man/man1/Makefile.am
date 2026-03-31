##
#	include-tidy -- #include tidier
#	man/man1/Makefile.am
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

dist_man1_MANS =	include-tidy.1

GROFF_FLAGS =		-man -Tps

PS2PDF_FLAGS =		-dEmbedAllFonts=true \
			-dHaveTrueTypes=true \
			-dPDFSETTINGS=/prepress \
			-dSubsetFonts=false

pdf-local: include-tidy-1.pdf

include-tidy-1.ps: include-tidy.1
	groff $(GROFF_FLAGS) $< > $@

include-tidy-1.pdf: include-tidy-1.ps
	ps2pdf $(PS2PDF_FLAGS) $<

clean-local:
	rm -f include-tidy-1.ps

distclean-local:
	rm -f include-tidy-1.pdf

# vim:set noet sw=8 ts=8:
