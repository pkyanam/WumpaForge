# Reading an active texture render target

Audited 2026-09-08 against original Xbox4361 `103C80 → 1062B0` and
`1037C0`, using the supplied executable's disassembly. No game run was used
to establish this defect, and it is not a demonstrated cause of the demo or
Cortex visual artifacts.

## Original contract

`Texture_LockRect` forwards its arguments to `1062B0`. At `1062B6`, bit `20`
skips the resource-completion wait; otherwise `1062C1` calls `1037C0`, which
waits on current work or the resource timestamp through `1034D0`. The helper
then obtains the real mip address/pitch through `106080` and returns those
guest DWORDs. Bit `40` selects the tiled-address alias. This original function
does not allocate replacement memory or discard the mip.

The Xbox flag names are corroborated by
[Cxbx's pinned header](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded/blob/585c49a50af1255ab155099e06f24505f9c5a800/src/core/hle/D3D8/XbD3D8Types.h#L466):
NOFLUSH=`10`, NOOVERWRITE=`20`, TILED=`40`, READONLY=`80`. The toolkit's host
D3D header contains PC flag values and is not the guest flag authority.
The original instruction behavior controls synchronization here.

## Demonstrated failure and narrow correction

The synthetic GPU fixture creates a swizzled texture with two mips, initializes
its guest bytes blue, binds mip1 as a render target, and clears the actual GPU
target red with a green top-left region. Before switching targets, a synchronized
read-only lock returned the old blue guest bytes instead of the completed GPU
pixels. Root's actual GPU run failed with `FF0000FF`, expected `FF00FF00`, pitch64;
see `local/reports/texture-target-lock-before.log`.

The bridge now recognizes **only synchronized read-only locks of the active
render-target mip**. It matches parent handle, data address, mip byte count,
pitch and format, then uses the existing GPU-to-guest resolve. Its native
`glReadPixels` supplies completion; the result is written into the original
swizzled or linear guest layout. The ordinary eight-byte lock result remains
pitch and guest pointer, including existing linear subrectangle offsets.

Ordinary CPU upload locks, unrelated textures/mips, and NOOVERWRITE locks gain
no readback or global GPU wait. Tiled pointer aliases remain explicitly rejected.
No backend implementation or cumulative graphics dependency patch changed.

## Validation and remaining boundary

`tools/test_texture_target_lock.inc` runs inside the native graphics component
smoke test. It checks swizzled mip1 and linear whole/subrectangle locks, actual
GPU colors, top-left row orientation, pitch and pointer offsets, isolation of
the other mip, unchanged active target, no-wait staging behavior, and release
lifetimes. Reproduce through the `wrath_graphics_check` CMake target in
[testing instructions](TESTING.md). Root also used the focused executable
`build/input/test_texture_target_lock`; build diagnostics are in
`local/reports/texture-target-lock-build.log`.
The after-fix GPU run exited **0**, including this fixture and the complete
existing graphics smoke suite; `local/reports/texture-target-lock-after.log`
records the result. The baseline exited134 on the expected stale-pixel assertion.

This change does **not** implement bidirectional coherence for writable locks
of an active GPU target. Such a path would need to preserve existing rendered
pixels before exposing RAM, then import actual CPU changes before subsequent
GPU drawing/clears/copies or target resolve. The current bridge has no separate
UnlockRect interception to delimit those writes. A broad per-draw shadow scan
or unconditional readback would impose unmeasured cost and is not justified
by this test. Ordinary writable upload locks remain handled by exact byte
validation before texture upload.

All four direct game calls found to `103C80` are texture loading/filling paths:
three in `3CFE0`, and `AEE70 → 3CF90 → 103C80`. No captured call proves they lock
an active target. If that boundary is reached, capture lock flags, parent/mip,
active surface/owner, and the next CPU-write/GPU-operation ordering before
expanding the implementation.
