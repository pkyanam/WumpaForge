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

## Follow-up: whole shader draw seam audit

A deeper transitive source audit changes the earlier conservative recommendation:
**guest-memory reads alone do not require packetizing the entire draw.** The
whole `shader_draw` body appears suitable for synchronous owner execution with
the adaptations below. This is a source conclusion, pending focused tests.

| Path | Operations and ownership |
| --- | --- |
| `guest_ptr` / `read32` | Global `g_xbox_mem_offset` plus pointer arithmetic/memcpy; no guest-register TLS except when the supplied address explicitly comes from `g_esp`. |
| `shader_program` | Shared protected shader cache/age, native GLSL generation and GL calls. No guest execution. Static GLSL buffers remain serialized by the caller's graphics mutex. |
| `shader_adjust_texture_modes` / fog / viewport | Shared title state and ordinary guest-memory reads, no callbacks. |
| `resource` | Protected lookup cache and paged table scan. No SDK entry or resource allocation callback. |
| `gather_vertex_attribute` / retained variant / index lookup | Guest byte reads, bounds/generation validation, host malloc/memcpy. No SDK call, guest function or graphics acquisition. |
| `texture_gl_name` | Optional glGenTextures for palette view, or `xbox_D3D8GLTextureName`, which only returns a host object's field. |
| `xbox_D3D8GLApplyRenderStates` | Reads protected backend `g.rs`, invokes GL setters and pure enum conversion. No lock acquisition or event pump. |
| Native vertex/pixel generation and dead-input proof | Host C arithmetic, output formatting and explicit input arrays. No guest dispatch, thread-local guest register dependency or callback. |
| Shader binary cache | Host filesystem, malloc, GL; errno is consumed on the same executing thread. Directory lock is nonblocking, no callback to waiting guest. |
| GL wrappers | Existing owner branches in BeginCall/BeginStateCall return0 and avoid the caller-held graphics mutex; EnsureCurrent checks the owner's actual context. Scalar commands and nested RPC execute in order on the owner. |

The sole production caller is `draw_vertices_data_fetch`. It acquires through
`graphics_thread()` before entering this seam. `vertex_array_commit`, texture
uploads, title `wrath_profile_draw`, `probe_begin`, `probe_end`, converted-buffer
free and guest `finish` stay on the original caller. The synchronous wait keeps
its stack `VertexFetch`, indices and converted vertex array alive. Moving the
whole enclosing caller instead would cross these additional boundaries and is
outside this conclusion.

Concrete TLS dependencies inside the seam:

- `shader_error` reads `read32(g_esp)`; one rejected-secondary-fetch diagnostic
  also does so. Capture the caller's return value before submission and provide
  it via a scoped diagnostic override on the owner, restoring the previous
  override afterward. Other shader setters retain their normal caller lookup.
  Do not initialize or impersonate the entire guest register bank on the owner.
- Backend `xbox_D3D8GLProfileDrawBegin/End` uses thread-local enable cache and
  `g_profile_calls.draws/draw_time`. Capture the owner's before values, return the
  exact delta, restore those values, then add the delta on the caller, as for UP
  draw. A narrow backend callback/profile adapter can avoid exposing its struct.
- Backend context depth is TLS but the owner-aware wrapper guards already bypass
  it. Title `s_graphics_held` is TLS but is not read inside this seam.
- RPC `owner` TLS is intentionally true. Native errno/driver TLS remain local to
  the executing owner. No explicit floating-point-environment mutation was found
  in the audited shader/gather/generator helpers.

Minimal implementation: rename the body `shader_draw_impl`; retain a wrapper
with the original signature and opt-in marker. On the caller retain/acquire the
existing serialization token, flush preceding scalar state, capture diagnostic
return, submit arguments and profile delta storage synchronously; merge profile
results and return the original HRESULT. Already-owner or disabled mode executes
inline. Preserve every early return/error, cache mutation and final GL error
check. The callback must not expand to invoke guest/SDK/event-pump functions.
Validate actual wrapper arguments, borrowed lifetimes, early failure, diagnostic
override restoration and exact profile deltas with real pthread RPC, then run
the physical shader/secondary-stream fixtures before a matched scene measurement.
