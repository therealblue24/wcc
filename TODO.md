# TODO list

welcome

If you want to contribute, do NOT implement the stuff under `Big stuff`. I want to handle that stuff.

## `wcc` (the compiler front-end)

### Big stuff

- Floats (`float` and `double`, `long double` will just be `double`)
    - might need a refactor

- Struct and union parameter passing and returning
    - might need a slight refactor

- Preprocessor
    - yeah this will be rough

- A way to find the include path

### Medium stuff

- Constant expressions
- Initalizers
- VLAs
- Varadics

### Small stuff

- `__builtin`s (i.e. abs, popcount, parity, clz, ctz, etc)
- atomics (optional)

## `bIRd` (middle/back-end IR)

### Big stuff

- Floats in IR
    - will need a refactor (lots of moving parts involved with floats: regalloc, register classes, parameter passing, IR dataflow, handling the type-less IR, etc.)

- Struct and union parameter passing and returning
    - might need a slight refactor

- Graph coloring register allocation
    - linear scan really sucks
    - possibly iterated register coalescing?

- Implement volatile load/stores

### Medium stuff

- IR lowering
- better way to describe side effects
    - like i.e. loads technically do not cause side effects[^1], but they can't be re-ordered trivially

- mem2reg pass
    - technically not really needed because the frontend can do so but nice-to-have

- alloca

[^1]: For memory mapped I/O, yes. They cause side effects without a doubt. But I believe that is up to the programmer to describe those loads as volatile.

### Small stuff

- builtins
- RISC-V?
    - Will be a pain because of the sign-extension for 32-bit operations on 64-bit registers. `bIRd` assumes zero-extension (which is the better extension!)
- (custom) Bytecode backend so we can run C code on some 8 bit computers (albeit **very** slowly)

## `zz`

### Big stuff

- fully type-generic hash maps?

### Medium stuff

- general purpose memory allocator?

### Small stuff

- bitsets?

## `wpas` (the Pascal compiler front-end)

### Big stuff

- Basically everything

### Medium stuff

- Decide: do we want manual or automatic memory mangement?

### Small stuff
