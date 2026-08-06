# `wcc` - the `Wonderful C compiler`
C Compiler, largely based on `chibicc` (used as a tutorial).

It uses its own optimizing SSA IR backend, [`bird`](bird/).

The IR register allocation is also inspired off of `9cc`.

Generates code for aarch64 & x86_64 (hopefully).

## Usage

To build:

```sh
$ make
```

To test:
```sh
$ make test
```

The actual compiler itself:
```
wcc version 0.0.2 build Jul 23 2026
Usage: ./bin/wcc <input file> [-o <output asm file>] [-t <arch>-<abi>] [-d] [-?/--help]
  -o <output>:          file to output assembly to (stdout is default)
  -t <arch>-<abi>:      target architecture, abi
                        only aarch64-apple, x64-sysv are supported.
  -d:                   enable debug IR printing
  -p:                   enable profiling
  -a:                   print AST
  -O0/1/2/3:            optimization level (default: 0)
  -?, --help:           this page
```

## Credits

- `chibbicc`, `9cc`
- `godbolt.org` (to see how `clang` generated some non-trivial stuff)
- RandomProgrammerOnTheInternet - advice, x64-sysv backend tester
- epic-coder-64 - advice, x64-sysv backend tester
