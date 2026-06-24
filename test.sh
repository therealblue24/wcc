#!/bin/sh

passing=1

# ./bin/wcc test/one.c -o test/one.s -O3
cc -c -o one.o test/one_impl.c

./bin/wcc --internal-test-strmap

assert() {
  
  expect="$1"
  actual=$2

  ./bin/wcc test/$actual -o prog.s -O3
  compilerstatus="$?"

  if [ "$compilerstatus" = "1" ]; then
    echo "compile error $actual"
    passing=0
    return
  fi
  
  cc -o prog prog.s one.o && ./prog
  status="$?"
  rm prog prog.s
  
  if [ "$status" = "$expect" ]; then
    printf "\e[0;32mok $actual => $status\n"
  else
    printf "\e[0;31mnot ok $actual => $status\n"
    passing=0
  fi
}
assert 0 "helloworld.c"
assert 0 "null_stmt.c"
assert 0 "ret0.c"
assert 1 "sub.c"
assert 46 "add.c"
assert 108 "crazy.c"
assert 108 "whitespace.c"
assert 1 "parse.c"
assert 41 "unary.c"
assert 1 "comparisons.c"
assert 5 "prog_stmts.c"
assert 1 "vars.c"
assert 2 "early_ret.c"
assert 4 "blocks.c"
assert 42 "if.c"
assert 16 "regalloc_test.c"
assert 3 "for.c"
assert 3 "converge.c"
assert 42 "while.c"
assert 123 "do_while.c"
assert 7 "ref_deref.c"
assert 1 "call.c"
assert 156 "call_many.c"
assert 2 "types.c"
assert 3 "funs.c"
assert 1 "div.c"
assert 251 "triangle.c"
assert 229 "primes.c"
assert 229 "primes2.c"
assert 229 "primes_short.c"
assert 1 "arrays.c"
assert 5 "mov_elim.c"
#todo proper alignment support (need to switch to sp-rel addressing)
#assert 6 "align.c"
assert 164 "sizeof_alignof.c"
assert 4 "globals.c"
assert 1 "strings.c"
assert 0 "fold.c"
assert 1 "stmt_expr.c"
assert 5 "cond_expr.c"

if [ "$passing" = "1" ]; then
  echo "all tests passed"
else
  printf "\e[0;33msome tests failed\n"
fi
