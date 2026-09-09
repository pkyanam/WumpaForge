# Pixel combiner numerical audit

2026-09-08. This check uses generated native GLSL and synthetic inputs, without
launching the game or embedding retail assets. The scalar oracle is independent
of the shader emitter and follows the input mapping, bias/scale, stage ordering
and final-stage equations in NVIDIA's
[NV_register_combiners specification](https://registry.khronos.org/OpenGL/extensions/NV/NV_register_combiners.txt).
Xbox-specific selection fields remain the existing documented wire contract.

`tools/test_combiner_differential.c` constructs96 deterministic multistage
programs and feeds32 input sets to each:3072 GPU/scalar comparisons. It covers
all eight signed/unsigned input mappings, six output mappings, both mux modes,
dot outputs, shared/independent constant banks, cross-channel reads, simultaneous
RGB/alpha writes, and default final fog/specular combinations. Values are finite
binary fractions, including endpoints; every output channel is compared after
8-bit conversion. The test reuses only GL setup/compile helpers from the existing
pixel test. No production emitter evaluation is used for the scalar result.

On this M3 Mac's native OpenGL driver, all3072 cases passed with maximum output
error0/255. Report: `local/reports/combiner-differential-test.log`. The strict
warning build also passed. Reproduce from the repository root:

```sh
clang -std=c11 -O2 -Wall -Wextra -Werror -DGL_SILENCE_DEPRECATION $(pkg-config --cflags sdl2 epoxy) tools/test_combiner_differential.c src/nv2a_pixel.c $(pkg-config --libs sdl2 epoxy) -o build/input/test_combiner_differential
build/input/test_combiner_differential
```

This is strong evidence for these arithmetic paths, not a probability of whole-
game correctness or proof of bit-exact Xbox fixed-point precision. It does not
cover every texture sampling mode, all final-combiner definitions, nonfinite
inputs, asset decoding, geometry, live draw state, or driver/device variants.
No production pixel-generator change was justified by this passing audit.
