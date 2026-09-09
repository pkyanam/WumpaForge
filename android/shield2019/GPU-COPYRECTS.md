# GPU CopyRects experiment

`title-zzzzzz-gpu-copyrects.patch` adds an Android-only direct framebuffer copy,
opt-in with `WRATH_GPU_COPYRECTS=1`. No host marker or default enablement is added.
The environment value is cached on the first CopyRects call.

Both surfaces must have authoritative GPU storage through `surface_gpu_storage`,
matching 32-bit format (06 or12), distinct complete framebuffers, and disjoint
resource storage/owners. CPU textures, inactive targets, aliases and unsupported
formats retain the original readback/upload path. Rectangles keep original
sequential validation; a later invalid rectangle preserves prior valid writes.
Guest bytes remain unchanged for GPU destinations, matching the existing path.

Validation: `python3 android/shield2019/tests/test_gpu_copyrects.py` passes with
ASan/UBSan against extracted actual helpers. It verifies opt-in gating, authority,
alias rejection, coordinate transformation, incomplete-FBO fallback and framebuffer,
buffer-selection and scissor restoration. It is a mock; it does not establish
physical driver pixel correctness or speed. All title patches apply to a fresh
source copy. Mac source remains unchanged.

Before enabling: add physical GPU checks for asymmetric top/bottom colors,
multiple rectangles including overlapping destinations and an invalid suffix,
front/back and active texture destinations, pending GL errors and CPU fallback.
Measure actual fast-path hit rate and copy cost. Prior hub logs show about4.7ms
per frame in CopyRects, including about3.7ms readback; those nested times must not
be summed and are not a predicted saving for this narrower GPU-only path.
