# Streaming buffer overwrite stall

A bounded5-second native sample of the parent's boot44 process (PID134), without
another game launch, identified the remaining wait:

`dev_DrawPrimitiveUP → glBufferSubData_Exec → gldBufferSubData →
GLDContextRec::flushResource → flushContext → dispatch semaphore wait`.

The main thread had4064 samples;2327 (57.3%) were in glBufferSubData across its
call paths. The largest path alone contained1555 samples, including1222 waiting
on a semaphore. Only11 samples were in glBufferData. This explains why previous
timers around glDrawArrays, context ownership and Present did not locate the
stall: the wait occurred earlier, while uploading vertex bytes. Native report:
`local/reports/boot44-heavy-sample.txt` (sample time18:22:41 on2026-09-08).

The backend previously allocated its streaming VBO/IBO when capacity grew, then
repeatedly overwrote offset0 with glBufferSubData for subsequent draws. Those
bytes could still be referenced by queued GPU draws, forcing driver flushing and
waiting. Every UP draw supplies the full byte sequence, so the backend now calls
glBufferData with that sequence and GL_STREAM_DRAW each time for both vertex and
index stores. This re-specifies storage instead of overwriting a live store; the
driver can retain the old physical storage until prior ordered draws complete.
The programmed shader path already used glBufferData in this way.

The [Khronos glBufferData reference source](https://github.com/KhronosGroup/OpenGL-Refpages/blob/main/gl4/glBufferData.xml)
describes creation of a new buffer store, replacement of the previous store, and
initialization from the supplied data pointer. These are native OpenGL storage
semantics, not an emulated GPU queue. Draw order, bytes, formats, transforms,
frame pacing, game animation and audio clocks are unchanged. There is no custom
unbounded buffer ring or additional persistent CPU copy. Driver transient storage
and real-game frame performance still need to be measured in the next run.

`tools/tests/streaming_buffers.c` includes the production backend and submits1024
ordered draws through DrawPrimitiveUP and DrawIndexedPrimitiveUP, alternates
vertex/index sizes, and overwrites the caller's stack data immediately after each
call. A single final framebuffer readback checks256 distinct tile colors from
the last ordered pass. There are no intermediate readbacks or artificial GPU
waits between draws. All pixels matched; the run took71.783ms including final
readback. This is a correctness regression, not a standalone proof of gameplay
speed. Log: `local/reports/streaming-buffers-test.log`.

Validation used one small compiler job. The cumulative graphics patch preserves
all prior backend and CMake changes and passes reverse-apply checking. No root
application, shader/viewport, index-bridge ABI, or controller files were edited.
