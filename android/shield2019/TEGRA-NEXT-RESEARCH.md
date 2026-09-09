# Tegra rendering candidates after the RPC profile

Research checkpoint: September 9, 2026. Target: SHIELD TV Pro 2019 only.
This is a source audit and primary-source research, not an implementation or
hardware speedup claim. Root owns builds and device tests.

## What the current measurement actually says

Ignored `local/reports/shield2019/tls-current-tail.log`, a profiled RPC run,
contains a 60-swap window with 113732 submissions, 62.988 ms/frame total RPC
latency, 22.246 ms/frame owner execution and 40.742 ms/frame handoff overhead.
Its nearby main frame1350 reports93.696 ms/frame (10.67FPS),55.355ms caller CPU,
zero texture uploads/readbacks and54.705MiB draw input over60frames. Profiling
adds overhead, and caller/render windows need not align exactly.

This evidence identifies CPU/submission work, not texture transfer or output
resolution, as the immediate target. Removing every measured handoff would still
leave roughly53ms/frame in this illustrative subtraction. It is not a60FPS
prediction. Owner CPU alone is16.109ms/frame, already nearly the entire16.667ms
budget before game CPU or presentation. A robust60FPS result requires reducing
actual work as well as changing where it runs.

The source has already changed since this trace: `prepare.py` now queues scalar
sampler/uniform setters. Measure that build before crediting another change with
the old sampler/uniform RPC cost.

## Ranked candidates

1. **One title draw packet, retaining guest reads on the caller.**
   `build/shield2019/title/shader_bridge.inc:shader_draw` remains outside the UP
   draw batching experiment. It uploads vertices, resets16attributes, gathers
   secondary streams, uploads uniforms, saves4samplers, applies samplers, draws
   and restores them. Materialize a host packet after all guest reads and
   resource validation; synchronously execute only its GL operations on the
   owner. Borrowed arrays remain alive until return. Start with uniform/sampler
   apply/draw/restore, preserving its exact ordering and HRESULT/error boundary.
   Full-function migration is unsafe without further separation: it calls
   `read32`, resource/gather helpers and diagnostic guest-stack reads. Keep
   caller-held resource serialization and profile attribution. This can remove
   multiple wakeups per programmable draw, but cannot remove the22ms owner cost.

2. **Copy small vector payloads into the existing scalar queue.**
   `glUniform2fv`, `3fv`, `4fv` and sampler border-color uploads still flush it.
   Own a bounded inline payload for small counts; preserve program-at-queue-time
   ordering, overflow flush and synchronous fallback for large counts. The
   192-vec4 vertex constants are3072bytes and already have value caching; do not
   allocate3072bytes in every scalar command. Also batch the four
   ActiveTexture/GetIntegerv sampler-save pairs in one callback. They cannot be
   replaced by the existing adjacent-getter helper because active texture
   changes between queries. Array/canary, changed-program and queued-error tests
   are required. This is narrower than a whole draw packet.

3. **Cache sampler state and non-vertex uniforms at their actual owner.**
   Each shader draw repeats min/mag/wrap/border parameters on the same four
   sampler objects. Cache exact bits per sampler object, and pixel/fog/alpha/
   viewport values per linked program; initialize invalid and invalidate on
   object recreation. Existing vertex-uniform caching is the model. Do not use
   one global uniform-value cache across programs. Preserve external binding
   restoration until all backend ownership is known. This removes raw driver
   operations, unlike queue batching alone. No numeric saving is established.

4. **Stream vertex data into a bounded ring instead of respecifying per draw.**
   The trace has3420`glBufferData`calls/60swaps,4.834ms/frame including1.715ms
   handoff. Primary plus secondary buffers are respecified in`shader_draw`.
   Compare a preallocated rotating buffer with a capability-gated persistently
   mapped ring. Use GPU fences before reusing ranges; coherent mapping handles
   visibility, not permission to overwrite in-flight vertices. Keep a bounded
   fallback for oversized draws. This can reduce allocation and upload overhead,
   but the measured BufferData group alone cannot produce a60FPS result.
   Actual GL version/extension support must be checked; Tegra branding is not
   evidence that this process advertises ARB_buffer_storage.

5. **Preserve indices and residency after submission overhead is reduced.**
   Programmable drawing ultimately uses`glDrawArrays`on expanded vertices.
   An indexed path could reduce expansion copies, GPU fetch and redundant vertex
   shader work. It must preserve Xbox primitive conversion, base vertex, retained
   secondary-stream addressing, NaN/color semantics and resource generations.
   Start with a single proven triangle-list case and an exact fallback. Do not
   cache guest data solely by address: retained CPU writes are real. This is
   higher effort and needs a post-batching CPU/GPU profile to justify priority.

## Primary sources and applicability

NVIDIA recommends avoiding redundant state changes and draw-loop getters,
retaining object state, and using reusable geometry buffers. Its Tegra guide
warns that updating buffers still in GPU use can stall. These are portable
principles, not measured SHIELD Android driver guarantees. We cannot reorder
transparent/ordered Xbox draws merely to batch state.
[NVIDIA Tegra OpenGL guidance](https://docs.nvidia.com/jetson/l4t/Tegra%20Linux%20Driver%20Package%20Development%20Guide/graphics_opengl.html).

ARB_buffer_storage defines immutable storage and persistent/coherent mappings.
Explicit lifetime synchronization remains necessary when reusing GPU-consumed
ranges; keep a fallback and query support.
[Khronos ARB_buffer_storage specification](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_buffer_storage.txt).

Before choosing a GPU rewrite, add delayed timer-query sampling around rendering
passes and read only results reported available, avoiding a new synchronous
GPU wait. This separates GPU execution from CPU submission/wakeup costs.
[Khronos ARB_timer_query specification](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_timer_query.txt).

Vulkan, bindless extensions, CUDA conversion and desktop driver environment
switches have no measured advantage here. A Vulkan rewrite would still need
packet construction, guest-resource coherency and submission batching. Fix those
measured costs first. Lowering already640x480 internal resolution or merely
upscaling output cannot remove these CPU/wakeup costs.

## Acceptance measurements

Use the same build, markers and comparable scene/animation windows; retain
p50/p95/p99 frame time, longest loading gap, submissions/draw, caller and owner
CPU, optional delayed GPU time, and allocations/upload bytes. Keep profiled and
unprofiled runs separate. Test sampler restoration, queued pointer lifetime,
multiple programs, retained writes and lifecycle handling before game checks.
One or two completed levels plus hub return are useful gates; they do not prove
all levels or constant60FPS.
