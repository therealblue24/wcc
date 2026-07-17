# `bIRd` - `Best IR backenD`
IR backend for `wcc`. The IR is not 100% complete but almost everything is in place.

May need a refactor for floats.

The register allocation algorithm was inspired by 9cc's.

Depends on `zz` in [`../zz`](../zz).

## Want to integrate it in your own project?

I recommend you not. If you insist, look at `src/codegen.c` to see how it is used. You also need to define a variable named `debug` somewhere. Keep it at zero. You also need `zz`. Also see the documentation at [here](../docs/bird.md).
