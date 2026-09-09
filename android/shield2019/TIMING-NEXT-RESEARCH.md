# Shield timing and loading: next experiments

Source audit, September 9, 2026. This is the third new research workstream requested
by the user. No build, device interaction, source modification, or new FPS
measurement was performed. Target: SHIELD TV Pro 2019 only. The root task reports
PID 22299 now contains the actual kernel dispatch TLS fix; the older observations
below precede that verified deployment and cannot assess its success.

## Highest-value conclusions

The old hub is predominantly caller CPU work, not a presentation wait. In ignored
`local/reports/shield2019/tls-live.log`, main frame 4931 reports 3.93 FPS,
254.309 ms interval, 240.527 ms work CPU, and 1.020 ms Present. Frame 5111 has
230.846 ms work CPU, 1.036 ms Present, and just 1.305 ms upload CPU. These are
individual historic windows, not a controlled comparison. Eliminating all upload
or swap overhead in that latter window cannot produce 60 FPS. CPU time includes
native graphics and driver work; it does not isolate translated game logic.

The 60 FPS target requires 16.667 ms per new game frame, including the hub. A
60 Hz TV refresh or 60 Hz callback does not establish that throughput. Profile
actual game progress separately from the loading worker and repeated display frames.

## Three independent clocks already exist

| Domain | Actual source behavior | Consequence and experiment |
| --- | --- | --- |
| Game update/animation | The original intro fade `85D40` changes once per loop; audited initialization 255 with step -8 takes 32 updates. `2C330` can hold animation rate zero based on audio status and the original gate at `1BAA2C`. See `docs/research/STARTUP-RUNTIME-AUDIT.md` and `docs/AUDIO-INTEGRATION.md`. | At 4 rather than 60 updates/s, 32 updates take 8 rather than 0.533 seconds. This arithmetic illustrates a possible slowdown; it is not a measured cutscene offset. Capture scene ID, animation position/step, audio gate and guest loop count against one monotonic timestamp. |
| Vblank callback | `src/timing_bridge.c:timing_run` schedules an absolute 60 Hz phase, records elapsed blanks, and delivers one callback after lateness instead of bursting stale calls. Original `39910` increments guest `1BABB0` once per delivered call. | Elapsed blank count and guest callback count can diverge under scheduling delay. Record delivered callback count, elapsed blank count and maximum callback lateness. Do not patch the guest counter to elapsed time or replay arbitrary callbacks without proving original semantics. |
| Audio production/output | Runtime `apu_core.c:throttle` advances independent 256-frame production deadlines; `apu_xaudio2.c` requests 48 kHz stereo, 512-frame callback, 1024-frame preroll, and a 3072-frame ring. | The application ring holds at most 64 ms; it cannot alone explain multi-second visual drift. The device/TV has additional latency that is not measured here. Correlate submitted, consumed, underrun/overflow and voice cursor values with scene timestamps. |

SDL defines callback invocation by audio demand and its buffer size in sample
frames. This supports keeping output independent from the draw loop; changing
sample rate to conceal slow rendering would alter playback and completion timing.
Sources: [SDL AudioSpec](https://wiki.libsdl.org/SDL2/SDL_AudioSpec),
[SDL audio callback](https://wiki.libsdl.org/SDL2/SDL_AudioCallback).

A small constant offset with normal animation progression points toward output
latency. A growing offset proportional to missing game updates points toward
slow scene progress. Rising underruns with sound breaking up points toward the
producer/scheduler. These hypotheses need paired measurements, not listening alone.

## Loading: separate four mechanisms

1. **Wrong kernel service / damaged guest stack.** The proven global-selector
   race could turn Enter/LeaveCriticalSection into another service with different
   stack cleanup. Re-test transitions only with the verified ELF TLS selector.
   Do not call the previous missed-patch runs evidence against the TLS correction.
2. **Graphics serialization starvation.** Loading animation and asset construction
   share the native graphics mutex. Record per-thread wait and hold time, loader
   completion state, bytes read and texture bytes processed in the same interval.
   A continuously animated portal with zero loader progress may be lock starvation
   or another guest wait; it does not establish storage throughput failure. Keep
   the mutex off arbitrary guest code and retain original lock ordering.
3. **File access latency.** Add fixed-size aggregated read counts/bytes/duration
   and a maximum, without per-read stderr output. Existing logs budget successful
   read dumps, but do not measure syscall latency. Only if read time dominates,
   compare a bounded read-ahead hint or small verified immutable-file cache;
   preserve failures, short reads, cursor behavior and memory limits.
4. **Potential shared file-position race.** Android's copied runtime
   `src/kernel/kernel_file.c:xbox_NtReadFile` performs `lseek(fd, offset)` followed
   by `read(fd,...)` without per-handle serialization. Concurrent callers sharing
   a descriptor can read another caller's offset. The diagnostic prints requested
   offset, so it cannot exclude this failure. Title overlap is unproven. First
   force the seek/read interleaving in a host fixture and record same-fd overlap
   on-device. Do not replace it blindly with `pread`: current-position updates,
   sequential calls, file aliases and Windows synchronous semantics need review.

A second level test should include successful return to the hub, not merely its
loading screen disappearing. Log requested level, actual loaded level, loader
completion and any allocation/read failures. No fixed asset-delay sleep or forced
completion flag is justified.

## Presentation research and bounded follow-up

Android's compositor repeats an old buffer when a new one misses a refresh. Its
Frame Pacing library uses presentation timestamps and fences to reduce irregular
cadence and queue buildup. This can improve delivery once workload is fast enough;
it cannot execute 240 ms of CPU work in 16.667 ms. The current port uses desktop
OpenGL through EGL, so compatibility with the documented GLES integration and
its required extensions must be checked before adopting Swappy.
Source: [Android Frame Pacing](https://developer.android.com/games/sdk/frame-pacing).

The existing `pace_present` already includes elapsed render and swap time in one
monotonic deadline; historical slow windows reported effectively zero pacing wait.
Only investigate replacing that scheduler after measuring CPU/GPU completion,
actual presentation intervals and queue depth. Do not stack a second independent
pacer on top. Choreographer ticks alone are not proof of on-time presentation.
Sources: [Android game loops](https://developer.android.com/games/develop/gameloops),
[Android system tracing](https://developer.android.com/games/optimize).

Suggested order: validate TLS transition correctness; capture one matched heavy
hub CPU sample; evaluate root's coarse graphics submission optimization; repeat
one intro with the paired clocks; then two load/play/return routes. Report median,
p95/p99 and maximum frame interval, frames above 16.667/33.333 ms, plus loading
wall time. A median of 60 FPS alone would miss the user's stutter requirement.
