# Active texture surface aliases

The native renderer previously decided whether to read live GPU pixels using
the exact surface handle passed to SetRenderTarget. Two live surface wrappers
for the same texture therefore disagreed while one was bound. Reading through
the other returned stale guest memory. A CopyRects write through the other
updated guest memory instead of the active GPU image, so a later target resolve
could overwrite it.

This is a concrete synthetic failure: bind wrapperA, clear its16×16 texture red,
then CopyRects from wrapperB into a separate texture. The original implementation
copied `FF0000FF` (the initial blue guest bytes), expected `FFFF0000` (live red).
Root captured the assertion failure in `local/reports/surface-alias-before.log`.
The test uses only generated pixels and synthetic resources.

## Original evidence and narrow correction

Original Xbox4361 CopyRects reads destination `Data` at `FF7B7` and source `Data`
at `FF7BA`, then calls `FF490` with those addresses and pitches. Distinct wrapper
handles do not create distinct storage. The supplied XBE's disassembly is in
ignored `local/reports/disasm/asm/D3D.asm`.

The prior hub capture `play-24-depth-resources.json` contains five such live
wrappers: `118EA80/AA0/AC0/AE0/B00`, all for texture `118D010`, Data `B91000`,
256×256 format6, pitch1024. This proves the wrapper pattern exists in the game;
it does not prove that this exact CopyRects sequence caused the user's observed
Cortex or demo distortion.

`surface_gpu_storage` now identifies the active GPU image for identical live
surface views sharing owner, Data, byte extent, dimensions, pitch and format.
Both source readback and destination CopyRects use that image. Partial overlaps,
different mips, reinterpreted formats and different pitches are not inferred to
be identical views. Resource references, target bindings and release ownership
are unchanged. Switching targets still resolves actual pixels into guest storage.

## Validation

`tools/test_surface_alias.inc` reproduces the old read failure, copies a green
subrectangle through the alternate destination wrapper, checks actual GPU pixels
inside and outside that rectangle, then verifies the modified pixels survive
the later swizzled guest-memory resolve. It also releases both wrappers and their
owner and checks for stale resources or GL errors. An unrelated texture remains
separate storage throughout the test.

The strict component compile is recorded in `surface-alias-build.log`. Root ran
the visible after-test: `surface-alias-after.log` exits0 and passes the new alias
fixture plus the full existing GL smoke suite. Full-build integration remains
root-owned. No live game process was
inspected or exercised for this audit. Draw topology and other vertex formats
were inspected briefly before narrowing the task to this counterexample; that
inspection is not a comprehensive conformance claim for those paths.
