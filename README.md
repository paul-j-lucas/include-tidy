# include-tidy

## Introduction

**include-tidy** is a command-line tool
that allows you to tidy-up the st of `#include` directives
used in either a C or C++ source file, specifically:

+ For every symbol (type, variable, function, or macro) that you reference
  in a source file, you should directly `#include` the header
  that declares that symbol.

That means **include-tidy** will print the `#include` directives
for every header your source file:

1. Is missing (and so should be added).
2. Is present for no reason (and so should be deleted).

**include-tidy** has the same purpose as
[**include-what-you-use**](https://include-what-you-use.org) (IWYU),
but IWYU has a number of [issues](https://github.com/include-what-you-use/include-what-you-use/issues).

## Dependencies

**include-tidy** depends on both
[Libclang](https://clang.llvm.org/docs/LibClang.html)
(typically installed as part of either a `clang`
or `clang-devel`
package,
if not from source)
and the
[Clang](https://clang.llvm.org)
compiler.

## Installation

The git repository contains only the necessary source code.
Things like `configure` are _derived_ sources and
[should not be included in repositories](http://stackoverflow.com/a/18732931).
If you have
[`autoconf`](https://www.gnu.org/software/autoconf/),
[`automake`](https://www.gnu.org/software/automake/),
and
[`m4`](https://www.gnu.org/software/m4/)
installed,
you can generate `configure` yourself by doing:

    ./bootstrap

Then follow the generic installation instructions
given in `INSTALL`.

If you would like to generate the developer documentation,
you will also need
[Doxygen](http://www.doxygen.org/);
then do:

    make doc                            # or: make docs

**Paul J. Lucas**  
San Francisco Bay Area, California, USA  
2 March 2026
