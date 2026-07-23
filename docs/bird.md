# Bird IR Documentation

## Basics

A program is a list of functions (`ir_func_t*`).

A function is a list of blocks (`ir_blk_t*`).

A block is a list of instructions (`ir_inst_t*`). A block ***always*** ends with a terminating instruction, which can be a branch, jump, or return. A block can have multiple (or no) predeccesors and can only have 0 (if it returns), 1, or 2 successors.

The IR which the frontend emits does ***not*** have to be SSA. (In fact, the IR cannot be SSA, as it cannot have phi functions in the frontend. See more later.) The IR is converted to and from SSA for you. Do note however that currently the mem2reg (or `varopt`) pass is not built into the IR and you have to implement that yourself. (I plan to fix this soon!)

The IR is also a three-address-code IR. Instructions have a maximum of 2 source arguments (with exceptions for `phi`, `call`, and `pmov`) and can only have 1 destination argument (or none). The IR also is flag-less, the only instructions that have side effects are the memory instructions and `call`.

The IR does have immediates, but the frontend cannot use them. Just generate `%r0 = imm #x` instructions and use the register instead. This approach is the best of both worlds for the frontend and optimizer.

You can create/delete functions/blocks/programs using `ir_*_make` and `ir_*_delete`.
Building the IR is trivial from there. (Use the IR builder interface.) In order to actually compile the IR into assembly, there is the following handy function to do all the steps for you:

```
/* generates code for an IR program */
/* handles all the function finalization stuff */
void ir_prog_compile(FILE *f, ir_prog_t *prog, enum ir_arch arch, int opt);
```

which emits assembly into the file `f`, given an IR program `prog`, for architecture `arch` and optimization level `opt`.

There are 4 optimization levels (0, 1, 2, and 3) and they determine how much optimization passes are done over the IR code (the amount of passes being 0, 1, 16, and 64, respectively). Note that the IR is always converted in and out of SSA regardless of optimization level.

There are 2 supported "architectures" (backends) as of now:
- `IR_ARCH_AARCH64_APPLE`: aarch64 assembly, apple ABI
- `IR_ARCH_X64_SYSV`: x86_64 assembly, System V ABI

I plan to add a `IR_ARCH_X64_WIN` (windows) backend after I am done with the actual ABI implementation (and compiler).

You also have a define a variable `int debug;` somewhere in your program. Set it to zero. You only want to set it to one if you want the debug IR printing to `stdout`.

## Instructions

### `IR_INST_NOP`: `nop`

Does nothing.

### `IR_INST_MOV`: `%r0 = %r1`

Moves register `%r1` to register `%r0`.

### `IR_INST_IMM`: `%r0 = imm #num`

Sets register `%r0` to the (64-bit) immediate specified.

### `IR_INST_ADD`: `%r0 = add %r1, %r2`

Adds `%r1` and `%r2` and stores the result in `%r0`.


### `IR_INST_SUB`: `%r0 = sub %r1, %r2`

Subtracts `%r1` and `%r2` and stores the result in `%r0`.


### `IR_INST_SMUL`: `%r0 = smul %r1, %r2`

(Signed) Multiplies `%r1` and `%r2` and stores the result in `%r0`.


### `IR_INST_UMUL`: `%r0 = umul %r1, %r2`

(Unsigned) Multiplies `%r1` and `%r2` and stores the result in `%r0`.

**NOTE**: on x86_64, this is the same as `IR_INST_SMUL`.

### `IR_INST_SDIV`: `%r0 = sdiv %r1, %r2`

(Signed) Divides `%r1` and `%r2` and stores the result in `%r0`.


### `IR_INST_UDIV`: `%r0 = udiv %r1, %r2`

(Unsigned) Divides `%r1` and `%r2` and stores the result in `%r0`.


### `IR_INST_SMOD`: `%r0 = smod %r1, %r2`

(Signed) Divides `%r1` and `%r2` and stores the remainder (modulo) in `%r0`.


### `IR_INST_UMOD`: `%r0 = umod %r1, %r2`

(Unsigned) Divides `%r1` and `%r2` and stores the remainder (modulo) in `%r0`.

### `IR_INST_AND`: `%r0 = and %r1, %r2`

Bitwise ANDs `%r1` and `%r2` and stores the result in `%r0`.


### `IR_INST_OR`: `%r0 = or %r1, %r2`

Bitwise ORs `%r1` and `%r2` and stores the result in `%r0`.


### `IR_INST_EOR`: `%r0 = eor %r1, %r2`

Bitwise XORs Adds `%r1` and `%r2` and stores the result in `%r0`.

### `IR_INST_SHL`: `%r0 = shl %r1, %r2`

Logically shifts `%r1` left by `%r2` and stores the result in `%r0`.

### `IR_INST_SHR`: `%r0 = shl %r1, %r2`

Logically shifts `%r1` right by `%r2` and stores the result in `%r0`.


### `IR_INST_ASHR`: `%r0 = ashr %r1, %r2`

Arithmetically (with sign extend) shifts `%r1` right by `%r2` and stores the result in `%r0`.

### `IR_INST_NEG`: `%r0 = neg %r1`

Negates `%r1` and stores the result in `%r0`.

### `IR_INST_NOT`: `%r0 = not %r1`

Bitwise NOTs `%r1` and stores the result in `%r0`.

### `IR_INST_MKBOOL`: `%r0 = mkbool %r1`

Equivalent to:

```
    %zero = imm #0
    %r0 = cmp.ne %r1, %zero
```


### `IR_INST_NOTBOOL`: `%r0 = notbool %r1`

Equivalent to:

```
    %zero = imm #0
    %r0 = cmp.eq %r1, %zero
```

### `IR_INST_EQ`: `%r0 = cmp.eq %r1, %r2`

Compares `%r1` and `%r2`. If they are equal, `%r0` is set to 1, else 0.


### `IR_INST_NE`: `%r0 = cmp.ne %r1, %r2`

Compares `%r1` and `%r2`. If they are equal, `%r0` is set to 0, else 1.

### `IR_INST_SLT`: `%r0 = cmp.slt %r1, %r2`
### `IR_INST_SLE`: `%r0 = cmp.sle %r1, %r2`
### `IR_INST_SGT`: `%r0 = cmp.sgt %r1, %r2`
### `IR_INST_SGE`: `%r0 = cmp.sge %r1, %r2`
### `IR_INST_ULT`: `%r0 = cmp.ult %r1, %r2`
### `IR_INST_ULE`: `%r0 = cmp.ule %r1, %r2`
### `IR_INST_UGT`: `%r0 = cmp.ugt %r1, %r2`
### `IR_INST_UGE`: `%r0 = cmp.uge %r1, %r2`

I don't want to write almost the same thing 8 times again so you can infer what any of these instructions does by the name.

If it starts with `S` (Signed), it is a signed comparison. If not, it is an unsigned comparsion.

`LT` is less-than, `LE` is less-than-or-equal, `GT` is greater-than, `GE` is greather-than-or-equal.

### `IR_INST_LOAD`: `%r0 = load %r1`

Loads memory at `%r1` and stores it in `%r0`. The `size` member of the instruction determines the memory size, and the `sign_ext` member determines if the load shall be sign extended.

### `IR_INST_LOADS`: `%r0 = loads #disp`

Loads memory at the stack base pointer, indexed by `#disp` and stores it in `%r0`. The `size` member of the instruction determines the memory size, and the `sign_ext` member determines if the load shall be sign extended.

### `IR_INST_STORE`: `store %r1, %r2`
Stores the register `%r2` at memory location `%r1`. The `size` member of the instruction determines the memory size.

### `IR_INST_STORES`: `stores #disp, %r1`
Stores the register `%r1` at the memory location of the stack base pointer indexed by `#disp`. The `size` member of the instruction determines the memory size.

### `IR_INST_LEAS`: `%r0 = leas #disp`

Loads the effective address of the stack base pointer indexed by `#disp`.

In x86 asm, is basically equivalent to:
```
    lea %r0, [rbp + #disp]
```

### `IR_INST_LEA`: `%r0 = lea Label`

Loads the effective address of a global variable labeled by `Label`.

In x86 asm, is basically equivalent to:

```
   lea %r0, [rip + Label] 
```

### `IR_INST_ZXT`: `%r0 = zxt %r1`

Zero extends `%r1` and stores the result in `%r0`. The size of the zero extension is determined by the `size` member in the instruction.


### `IR_INST_SXT`: `%r0 = sxt %r1`

Sign extends `%r1` and stores the result in `%r0`. The size of the sign extension is determined by the `size` member in the instruction.

### `IR_INST_BR`: `br %r1, true-blk, false-blk`

If `%r1` is not zero, a jump to `true-blk` is performed. If not, a jump to `false-blk` is performed.

### `IR_INST_JMP`: `jmp blk`

A uncondtional jump to `blk` is performed.

### `IR_INST_RET`: `ret (%r1)`

Returns the function, with a possible value to return. If there is no value to return, set `r1` to `NULL`.

### `IR_INST_CALL`: `(%r0) = call Function, %a1, %a2, ...`

Note that this instruction is not 100% finished because the whole ABI of both x86_64 and aarch64 is not implemented.

Calls a function with the arguments specified, and stores the return value in `%r0` if not `NULL`.

## SSA instructions (not recommended to use)

Do note that your frontend should NOT emit SSA instructions if you are using `ir_prog_compile`! The SSA entering pass assumes the IR is a typical 3AC without phis and parallel moves. Use `ir_ssa_exit` then compile the program.

### `IR_INST_PHI`: `%r0 = phi [pred1, %a1], [pred2, %a2], ...`

The phi instruction of SSA. Chooses a value from the predeccesor list based on which predeccesor jumped to the current block and stores it in `%r0`.

***IMPORTANT NOTE***: Phis are executed _in parallel_, and must be treated as such. See `IR_INST_PMOV` for more.

### `IR_INST_PMOV`: `{ %d0 = %s0, %d1 = %s1, ... }`

Parallel move/copy instruction. Executes a series of parallel moves.

***NOTE***: The moves are executed _in parallel_. This means that a parallel move like this:

```
    {
        %r0 = %r1
        %r1 = %r0
    } 
```

would sequentialize (generate) into the following:

```
    %tmp = %r0
    %r0 = %r1
    %r1 = %tmp
```

This is because the moves are executed in parallel, and as such the assigns to `%r0` and `%r1` are not processed until after the parallel move.

This is a bad explanation and you should seek a better one. (I can't find one.)
