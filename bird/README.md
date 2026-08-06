# `bIRd` - `Best IR backenD`
IR backend for `wcc`. The IR is not 100% complete but almost everything is in place.

The register allocation algorithm was inspired by 9cc's, with improvements from [Improvements to Linear Scan register allocation](https://llvm.org/ProjectsWithLLVM/2004-Fall-CS426-LS.pdf).

Depends on `zz` in [`../zz`](../zz).

## Want to integrate it in your own project?

Look at `src/codegen.c` to see how it is used. You also need to define a variable named `debug` somewhere. Keep it at zero. (Except if you want debug IR printing, set it to one.) You also need [`zz`](../zz). Also see the documentation at [here](../docs/bird.md).
