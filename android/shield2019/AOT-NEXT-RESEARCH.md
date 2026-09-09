# Native ARM64 CPU opportunities — Shield Pro 2019

Research checkpoint: September 9, 2026. This is a source/evidence audit, not a
performance result. No build or device operation was performed for this report.
Root must first install the actually applied kernel-dispatch TLS correction:
performance comparisons across a misdispatched/frozen game are invalid.

## What the existing measurements establish

`local/reports/shield2019/hub-ring-report.txt` contains 692 task-clock samples
from PID 14444 over eight seconds: memcmp 7.23%, memcpy 6.65%, dynamic TLS resolver
2.46%, native graphics wrapper 101BC0 2.46%, FFEA0 1.88%, shader_draw 1.30%.
These are sampled **CPU shares**, not frame wall-time shares or guaranteed savings.
The later freeze invalidated a separate target-cache A/B experiment; do not use
that experiment as evidence that the cache improved or harmed performance.

`bounds-current-report.txt` has 705 samples in a different scene: A1520 1.84%,
dynamic TLS 2.84%, memcmp 6.10%, memcpy 4.40%. Resolver samples also occur in the
other reports (roughly 1.6–3.6%). None establishes generated game arithmetic as the
single dominant bottleneck. Removing a 3% CPU cost cannot alone triple throughput.
Incomplete frame-pointer callgraphs are unsuitable for assigning every libc
sample to a particular source function.

CMake currently gives runtime code explicit `-O2`, generated game shards `-O1`,
and preserves `-fno-strict-aliasing -fwrapv`. The sampled SDK wrapper names can be
native replacements; do not optimize generated files merely because a symbol
resembles a guest address. The prepared graphics source has exact snapshot
memcmp checks in texture_snapshot_matches and target_return_snapshot_matches.
These are plausible contributors; per-site counters are needed to assign cost.

## Ranked experiments

1. **Selective optimization, narrow and cheap.** Try only generated
   `recomp_0016.c` at O2 first (contains A1520), keeping existing ABI and arithmetic
   flags. Check actual compile command and object disassembly; CMake option order
   matters. Measure identical hub and gameplay segments, APK text size, build time,
   median/p95/p99 frame time. This is likely incremental, not the main unlock.
   Add compiler optimization remarks to this shard to identify blocked transforms
   before escalating to O3, PGO or broad LTO. Clang documents remarks and selective
   instrumentation profiling; a tiny old simpleperf sample is not ready-made PGO
   training data. [Clang compiler manual](https://clang.llvm.org/docs/UsersManual.html)

2. **Consolidate guest TLS state, preserving its semantics.** The generated header
   exposes separate thread-local integer registers, x87 stack/top, FS base and SEH
   state. An Android-only prototype can use one TLS register-state structure and
   acquire its pointer once per generated function. Keep field reads/writes live
   through that pointer; do not copy all registers into locals across calls.
   Callbacks, SEH and longjmp must observe the same thread's latest state. Verify
   two threads with forced interleaving, nested callbacks, x87 argument passing,
   exception entry/return and nonlocal jumps against the current ABI. Inspect
   resolver-call counts in disassembly, then benchmark before widening scope.
   This is a generator/runtime ABI change and requires all consumers to agree;
   it is more invasive than the current dispatch-selector TLS fix.

   Do **not** force initial-exec TLS or use undocumented Android TLS slots.
   Bionic documents that dlopened libraries cannot assume static TLS surplus;
   normal ARM64 dynamic TLSDESC already has a fast path. Visibility/local-dynamic
   experiments need actual emitted-code evidence and cannot be assumed faster.
   [Bionic ELF TLS design](https://android.googlesource.com/platform/bionic/+/HEAD/docs/elf-tls.md)

3. **Reduce exact snapshot work before replacing libc.** Instrument bytes and CPU
   time at each snapshot match/capture site, with logging batched once per interval.
   If repeated checks occur during one serialized host operation with no guest
   callback or possible writer, reuse that exact equality result only within the
   proven interval. If not, keep the comparison. Retained guest pointers allow
   writes without a new Lock call; resource dirty flags or occasional checks are
   insufficient. Hashing adds a pass and collisions cannot prove equality. Avoid
   page-fault dirty tracking for now: guest/host aliases, unrelated page occupants
   and multithreaded faults greatly increase correctness risk. Reuse bounded
   allocations and audit ownership before changing original guest heap behavior.

4. **Specialize arithmetic only after its cost grows in a fresh profile.** A1520
   is an unrolled matrix-style operation with x87 values represented as double,
   ordered arithmetic and stores to guest scratch at 0x1A3B48 onward. A tempting
   float NEON matrix multiply changes intermediate precision, addition order and
   possibly alias behavior. A faithful specialized leaf can instead retain the
   exact double operations and scratch/store sequence while eliminating x87 stack
   bookkeeping, and must reproduce exposed registers, stack state and flags.
   Differential tests need overlapping inputs/output, all x87 starting tops,
   signed zero, subnormals, infinities, NaNs and representative finite matrices.
   Test final guest memory and ABI state, not only a visually plausible matrix.

   ARM64 Android already supports NEON; adding a NEON flag is not an unlock.
   [Android NDK NEON guide](https://developer.android.com/ndk/guides/cpu-arm-neon)
   LLVM warns that many floating reductions require reassociation permissions to
   vectorize. Independent lanes with preserved order are a safer target than
   blanket fast-math. [LLVM vectorizers](https://llvm.org/docs/Vectorizers.html)

5. **Immutable per-ordinal kernel trampolines, later.** The immediate TLS selector
   fix should remain the baseline. A generated distinct wrapper for each imported
   kernel ordinal could carry its ordinal as an immutable constant, eliminating
   selector lookup state altogether. This is principally a correctness simplifier;
   no profile establishes a large dispatch CPU cost. Preserve stack cleanup,
   diagnostic attribution and nested callback behavior. Reuse the forced
   interleaving fixture and compare all supported bridge argument counts.

## Measurement gate

Use one known stable APK and fixed saved position/input duration. Record thread
CPU and frame wall time separately, plus load completion, frame count and resource
bytes. Repeat comparable warm runs with the same diagnostics enabled; take a cold
loading run separately. Keep original frame pacing and audio clocks intact.
Successful component checks do not prove a level loads or 60 FPS: root should
verify the hub, Arctic Antics and a second first-realm level, and distinguish
source movie frame rate from the display's 60 Hz presentation cadence.
