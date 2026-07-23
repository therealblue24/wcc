# `wcc` - the `Wonderful C compiler`
C Compiler, largely based on `chibicc` (used as a tutorial). The IR register allocation is also inspired off of `9cc`.

Curently the "version" of C it can compile is Turing Complete and has functions, types, arrays, etc.

Generates code for aarch64 & x86_64. I have not tested if the x86_64 backend works on the latest commit because I don't have an x86 machine.

You can also check out the IR backend, `bIRd` in `bird`, which can probably be ported to other projects.


## Usage

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

## Build

```sh
$ make
```

## Test

```sh
$ make test
```

## Help

```sh
$ make help  
```

## Calculate Swag Points

```sh
$ make count
```
Swag Point Counter: `9349`
