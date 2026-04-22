# include-tidy

## Introduction

**include-tidy** is a command-line tool
that allows you to tidy-up the set of `#include` directives
used in either a C or C++ source file,
specifically:

+ For every symbol (type, variable, function, or macro) that you reference
  in a source file, you should directly `#include` the header
  that declares that symbol.

That means **include-tidy** will print the `#include` directives
for every header your source file:

1. Is missing (and so should be added).
2. Is unnecessary (and so should be deleted).

**include-tidy** has the same purpose as
[**include-what-you-use**](https://include-what-you-use.org) (IWYU),
but IWYU has a number of [issues](https://github.com/include-what-you-use/include-what-you-use/issues).

## Dependencies

**include-tidy** has the following dependencies:

+ [Libclang](https://clang.llvm.org/docs/LibClang.html)
  (typically installed as part of either a `clang`
  or `clang-devel`
  package,
  if not from source).

+ The [`llvm-config`](https://llvm.org/docs/CommandGuide/llvm-config.html)
  command
  (typically installed as part of either an `llvm`
  or `llvm-devel`
  package,
  if not from source).

+ The [Clang](https://clang.llvm.org) compiler.

Although Clang and LLVM are often packaged seperately,
their versions increment in lockstep,
so you _should_ install the same version for both.

**include-tidy** was written against version 21.x of Libclang.
(Hopefully, **include-tidy** will continue to work with future versions.)
**include-tidy** is known _not_ to work correctly
with version 18.
Nothing is known about either versions 19 or 20.

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
8 April 2026
