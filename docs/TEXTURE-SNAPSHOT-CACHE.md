# Exact texture snapshot cache

Boot40 measured20FPS windows with37ms/frame spent decoding/uploading textures
(about79% of frame work) and only2–3ms in presentation, with no software pacing
sleep. SetTexture unconditionally invalidated each bound texture; repeated binds
therefore decoded and uploaded every mip even when the game bytes were unchanged.

The native resource bridge now retains an exact encoded-byte snapshot after each
successful upload. Before reusing a texture it compares all guest encoded bytes
with `memcmp`; it never relies on a hash or on the dirty bit alone. Consequently
writes through a retained guest pointer are detected even without a Lock call.
SetTexture's existing dirty assignments remain; equal content can safely suppress
the expensive decode/upload. Draw-time validation also catches changes after a
bind. Byte comparisons themselves remain CPU work and should be measured in the
next run, but they avoid per-texel decoding, allocation and GL texture replacement.

The snapshot contains all encoded mip levels and, for cubes, all six padded face
chains. When a snapshot is available, decoding reads the copied bytes rather than
capturing a potentially different sequence after GPU upload. The snapshot is
marked valid only after successful upload; an upload error cannot validate stale
GPU contents. This does not add synchronization for invalid concurrent game
writes during a draw; the existing device/resource ownership contract remains.

P8 has two dependencies. Changed index bytes advance its content revision; exact
palette-byte comparison catches direct color writes as well as existing palette
Lock/SetPalette revision changes. Each stage retains its original palette handle,
palette revision and index revision key. An unchanged index array does not cause
a stale stage image when its palette changes, and updating one stage leaves an
independent unchanged palette variant reusable. Palette snapshots are host-only.

All snapshot allocations share a64MiB cap and allocate lazily. Resource release
frees their storage and returns the byte budget, including palettes and cube
owners. Allocation failure or cap exhaustion leaves the original full upload
path active, preserving pixels at reduced speed. No shadow bytes enter the game
heap, XBE, Git, or saved assets. The cap is additional native memory, not expanded
Xbox RAM. Encoded snapshots are generally smaller than expanded RGBA uploads.

Render-target ownership is unchanged. Native rendering uses the existing separate
surface target texture/FBO. Leaving that target resolves actual GPU pixels into
guest encoded storage; the subsequent parent upload detects those changed bytes.
If a resolve produces exactly the previous encoded bytes, reusing the existing
sampling texture is correct. CPU CopyRects and surface writes are likewise
checked against the parent snapshot. This change does not skip readback/resolve,
introduce stale guest authority, or attempt unsupported framebuffer feedback.

`tools/test_texture_snapshot.inc` extends the production graphics smoke with
actual GL texture readbacks. It checks twenty repeated unchanged binds perform
one initial upload only; direct writes with dirty clear change base and final-mip
pixels; a last-face cube mip changes; P8 direct index writes update both variants;
direct palette writes update only the dependent stage; and releases restore the
snapshot byte budget. Simulated cap exhaustion verifies that fresh direct writes
still upload without a snapshot. The full prior GPU smoke also passes its
render-target resolve/CopyRects, framebuffer, shader, alpha/blend, indexed and
immediate regressions. One compiler invocation ran component tests only:
`local/reports/graphics-snapshot-smoke.log`. Parent owns the next full game run and
measurement; no actual-game speedup is claimed until that result is available.
