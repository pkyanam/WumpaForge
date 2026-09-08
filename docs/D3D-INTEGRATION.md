# Native graphics bridge — 2026-09-08

`src/graphics.c` implements four compiled host replacements for this game's Xbox
4361 SDK, plus the resource/draw subset described below. Its standalone test creates an actual SDL/OpenGL window on this M3,
clears its framebuffer, verifies pixels with `glReadPixels`, and swaps it. This
is **graphics compatibility validation, not a running game**. Game integration, complete shaders/resources and unbridged GPU pushbuffers
remain unfinished. The later resource/draw extension renders actual supplied
vertex/texture data in the standalone test.

## Integrating the four hooks

Add `src/graphics.c` to the native executable; its includes need the upstream
`src` directory, SDL2 and epoxy headers. Link `xbox_d3d8`, SDL2, and epoxy. The
file exports `recomp_func_t wrath_graphics_lookup(uint32_t address)` plus the
four exact `sub_*` symbols below. Call this lookup from `recomp_lookup_manual`.
Exclude these functions from generated definitions when lifting:

```
0x000FD6E0
0x000FF860
0x00100EA0
0x00100C40
```

Keep direct-call prototypes and dispatch entries resolving to those exported
symbols. Dispatch lookup alone cannot replace direct C calls if generated
function bodies remain linked. No CMake, dispatch, main, or generated sources
were changed in this graphics subtask.

All four calls run on SDL's/main macOS thread. The bridge checks
`pthread_main_np()` and rejects calls elsewhere. Parent's current game startup
runs on the main thread. SDL/OpenGL has to remain alive for the bridge lifetime.

## Address and ABI evidence

Addresses come from the supplied XBE, with local evidence in
`local/reports/disasm/asm/D3D.asm` and `text.asm`. Each signature listed below
matched exactly and uniquely among detected D3D function starts, by comparing
all sparse `(offset, byte)` pairs from its public signature. This identifies
functions, not argument types by itself; argument/cleanup details were checked
in local disassembly.

| Function / exported symbol | Address | Arguments / guest cleanup | Evidence |
|---|---|---|---|
| `sub_000FD6E0` CreateDevice | `0x000FD6E0` | Six stack args; `ret 0x18` | 3911 signature; caller `0x39B1A` |
| `sub_000FF860` SetViewport | `0x000FF860` | Viewport pointer; `ret 4` | 4034 signature; caller `0x39B83` |
| `sub_00100EA0` Clear | `0x00100EA0` | Six stack args; `ret 0x18` | 4034 signature; caller `0x39B2D`, `0x39E87` |
| `sub_00100C40` Swap | `0x00100C40` | Flags; `ret 4` | 4034 signature valid through 4432; caller `0x39E73` |

All replacements are C `void(void)`: read guest arguments at `g_esp+4`, return
in `g_eax`, advance `g_esp` by return-address size plus argument cleanup. Preserve
callee-saved guest registers by never modifying them. Guest addresses translate
through `g_xbox_mem_offset`, using 32-bit reads/writes, while native pointers
stay in host-only static storage.

CreateDevice receives `(adapter, device_type, focus_window, behavior_flags,
presentation_parameters, returned_device_pointer)`. Observed game arguments:
`0, 1, 0, 0x40, <stack struct>, 0x1E3C90`. The last argument is the address of the
game's device pointer. The SDK singleton is guest `0x10C110`, which is also
stored at `0x10EBF0` and `0x10EBF4`. At `0xFD72F` the SDK sets `0x10C550` to 1;
its behavior flag `0x10` is ORed into singleton+8. Default pushbuffer sizes at
`0x10F57C` and `0x10F578` are `0x80000` and `0x8000` unless already specified by
SetPushBufferSize. The bridge reproduces these observed globals without copying
a native COM pointer into guest RAM or wiping the other static device bytes.

Native creation uses `xbox_Direct3DCreate8(4361)->lpVtbl->CreateDevice(...)`.
Guest window tokens are not Cocoa pointers; the SDL backend creates its own
window. Guest dimensions and remaining common presentation values are copied
field-by-field into the native struct. Custom preallocated surface pointers are
currently rejected, rather than accepted without implementing their behavior.

### Guest presentation structure

Size is 68 bytes, with each scalar and pointer represented by one 32-bit word:

| Offset | Field |
|---|---|
| `0x00`, `0x04` | Width, height |
| `0x08`, `0x0C` | Xbox format, buffer count |
| `0x10`, `0x14` | Multisample mode, swap effect |
| `0x18`, `0x1C` | Guest window token, windowed |
| `0x20`, `0x24` | Depth/stencil enabled, depth format |
| `0x28`, `0x2C`, `0x30` | Flags, refresh Hz, presentation interval |
| `0x34`, `0x38`, `0x3C` | Three optional guest buffer-surface pointers |
| `0x40` | Optional guest depth-surface pointer |

The game's initializer zeroes 17 dwords at `0x39AC7`, consistent with this size.
A host `HWND` is 64 bits, so direct casting to native `D3DPRESENT_PARAMETERS`
would change layout after offset `0x18`.

Viewport is 24 bytes: X/Y/width/height are four 32-bit integers followed by
MinZ/MaxZ floats. SDK writes the clipped viewport at device+`0x9D0` through
`0x9E4` (`0xFF95B` onward). Bridge clips against the current window target,
calls native SetViewport, then mirrors the result to that guest location.

Clear receives `(count, rects, flags, color, depth_float_bits, stencil)`.
Xbox flags are **depth `0x01`, stencil `0x02`, individual R/G/B/A channels
`0x10/0x20/0x40/0x80`**. Target-all is `0xF0`. These differ from the host
compatibility header's `D3DCLEAR_TARGET=1`, depth=2, stencil=4. The bridge
translates flags for native whole-target Clear. Since upstream Clear ignores
rectangles and channel masks, other cases use native OpenGL scissoring/masks
and restore GL state. Clear with no rectangles affects the current viewport,
consistent with local SDK code at `0x10106F`–`0x1010BB`.

Swap defaults zero flags to 5 (`0x100C50`). Bit 2 performs a new swap; wait-only
flags do not submit another frame. The guest counter is at singleton+`0x2AC4`;
the original increments at `0x100CD4` and returns it at `0x100CE6`. Bridge uses
the native Swap method, preserves this counter behavior, and does not invent
GPU-fence state. Vsync comes from SDL. Full Xbox wait/fence semantics remain
outside this initial four-hook bridge.

## Additional identified addresses for subsequent work

| Function | Guest address | Signature version |
|---|---|---|
| SetPushBufferSize | `0x000FD6C0` | 3911 |
| CreateTexture | `0x000FE9C0` | 3911 |
| CreateVertexBuffer | `0x00100D70` | 3911 |
| SetStreamSource | `0x00102580` | 4034 |
| SetIndices | `0x000FFEA0` | 4034 |
| SetTexture | `0x000FFC90` | 4034 and 4361 |
| SetVertexShader | `0x00102940` | 4134 |
| SetVertexShaderConstant | `0x00102AA0` | 4034 |
| Begin / End | `0x00101EC0` / `0x00101F00` | 4039 |

These are findings only; no replacements for these functions were implemented.
In particular, guest singleton surface/pushbuffer internals are **not fully
initialized** by this bridge. Unbridged SDK calls can still fail or attempt GPU
MMIO. The four-hook result does not establish that subsequent game rendering
can run unchanged.

## Source provenance

Public signature database, revision `20eced544726f5558c5a408458f38a086cc4e543`:

- [3911 signatures](https://github.com/Cxbx-Reloaded/XbSymbolDatabase/blob/20eced544726f5558c5a408458f38a086cc4e543/src/OOVPADatabase/D3D8/3911.inl)
- [4034 signatures](https://github.com/Cxbx-Reloaded/XbSymbolDatabase/blob/20eced544726f5558c5a408458f38a086cc4e543/src/OOVPADatabase/D3D8/4034.inl)
- [4039 signatures](https://github.com/Cxbx-Reloaded/XbSymbolDatabase/blob/20eced544726f5558c5a408458f38a086cc4e543/src/OOVPADatabase/D3D8/4039.inl)
- [4134 signatures](https://github.com/Cxbx-Reloaded/XbSymbolDatabase/blob/20eced544726f5558c5a408458f38a086cc4e543/src/OOVPADatabase/D3D8/4134.inl)
- [4361 signatures](https://github.com/Cxbx-Reloaded/XbSymbolDatabase/blob/20eced544726f5558c5a408458f38a086cc4e543/src/OOVPADatabase/D3D8/4361.inl)

Presentation structure and Xbox clear flag values cross-checked against public
[Cxbx Xbox D3D types](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded/blob/585c49a50af1255ab155099e06f24505f9c5a800/src/core/hle/D3D8/XbD3D8Types.h).
Only interface facts were used; the bridge implementation was written locally.
No emulator code or CPU interpreter was added. Native renderer is existing
xboxrecomp revision `051a128df5ec27ef14f1ceaaead11c5457321eef`,
`src/d3d/d3d8_gl.c`.

## Reproducible standalone test

The test is included conditionally in `src/graphics.c`; it runs without game
assets. Reuses the already-built runtime library, with one compile process:

```sh
mkdir -p build/input
clang -std=c11 -Wall -Wextra -Werror -Wno-deprecated-declarations \
  -DWRATH_GRAPHICS_SMOKE_TEST -Ithird_party/xboxrecomp/src \
  $(/opt/homebrew/bin/sdl2-config --cflags) -I/opt/homebrew/include \
  src/graphics.c build/native/third_party/xboxrecomp/src/d3d/libxbox_d3d8.a \
  -L/opt/homebrew/lib $(/opt/homebrew/bin/sdl2-config --libs) -lepoxy \
  -o build/input/test_graphics
build/input/test_graphics
file build/input/test_graphics
```

Observed result:

```
[d3d8_gl] GL 4.1 Metal - 90.5 / GLSL 4.10
[wrath graphics] native device 320x240, guest handle 0x0010C110
PASS: native GL device, exact colors/masks/viewport, guest ABI, swap
Mach-O 64-bit executable arm64
```

The SDL backend's 3.3 core request successfully creates a 4.1 context on this
Mac; no renderer modification was necessary. No game UI or image was supplied
to the test; it verifies deliberate framebuffer clear colors and reads them
back before presenting.

## Resource and draw extension

`src/graphics.c` now also bridges this bounded set. Add all the following guest
addresses to the lift exclusions; their exact `sub_*` symbols and manual lookup
entries are exported by the same source file. No additional CMake source is
needed. Production graphics now also calls the existing guest allocator
`xbox_HeapAlloc(size,alignment)` / `xbox_HeapFree(guest_address)`.

| Address | SDK function | Stack arguments, in order | Cleanup |
|---|---|---|---|
| `0x000FE9C0` | CreateTexture | width,height,levels,usage,format,pool,out | 28 |
| `0x00103C80` | Texture LockRect | texture,level,out,rect,flags | 20 |
| `0x00103C20` | Texture GetLevelDesc | texture,level,out | 12 |
| `0x00103C30` | Texture GetSurfaceLevel | texture,level,out | 12 |
| `0x00103DD0` | Texture GetLevelCount | texture | 4 |
| `0x00103A90` | Resource AddRef | resource | 4 |
| `0x00103AD0` | Resource Release | resource | 4 |
| `0x000FFC90` | SetTexture | stage,texture | 8 |
| `0x00100D70` | CreateVertexBuffer | length,usage,FVF,pool,out | 20 |
| `0x00100DD0` | VertexBuffer Lock | buffer,offset,size,out,flags | 20 |
| `0x00102580` | SetStreamSource | stream,buffer,stride | 12 |
| `0x00102940` | SetVertexShader | FVF-or-program-handle | 4 |
| `0x001019C0` | DrawVerticesUP | type,vertex_count,data,stride | 16 |
| `0x00101B20` | DrawVertices | type,start_vertex,vertex_count | 12 |

Create/lock/description/set/draw calls return HRESULT in the bridge. AddRef and
Release return the guest external reference count; GetLevelCount returns the
serialized level count. Cleanup excludes the extra four-byte guest return
address, as in the initial table.

Additional exact signature matches: 3911 identifies Resource AddRef/Release,
Texture LockRect/GetSurfaceLevel, and GetLevelCount. `0x103C20` is the local
GetLevelDesc tail-jump to `0x105FC0`, used by texture-binding wrapper `0x3AAE0`
which consumes width/height at descriptor+20/+24. VertexBuffer Lock is verified
by disassembly at `0x100DD0` (resource data pointer plus requested offset, written
to output, ret20), and its game wrappers `0x3A310` etc.

DrawVerticesUP was identified by local ABI and its caller at `0x398D0`: the game
wrapper looks up primitive conversion factors at `0x15AE90`, converts primitive
count to **vertex count**, and calls `0x1019C0` at `0x398FC` with four arguments.
The SDK copies data using the provided stride and issues NV2A inline-vertex
commands. DrawVertices at `0x101B20` is called by wrapper `0x3AF90`, consumes
three arguments and emits vertex-batch method `0x1810`. These draw addresses
were not claimed as public sparse-signature matches.

### Serialization and texture data

Resource headers are guest allocations and contain only 32-bit fields:

- Vertex buffer: `Common`, `Data`, `Lock` (12 bytes).
- Texture: those fields plus `Format`, `Size` (20 bytes).
- Surface view: texture fields plus parent-resource pointer (24 bytes).

`Common` retains external references in bits 0–15 and bound references in bits
19 onward, matching the local SetTexture/SetStreamSource additions of `0x80000`.
Allocated vertex buffers use `0x01000001` and textures `0x01040001` initially.
Native texture/buffer pointers live in a separate host table. Bound resources
survive guest Release until unbound. Surface views retain their parent texture
and share its mip data rather than allocating duplicate pixels.

Texture `Format` carries dimension=2, DMA channel=1, Xbox pixel format in bits
8–15, level count in 16–19, and log2 base dimensions in 20–27 for swizzled or
compressed textures. Linear `Size` stores width-1, height-1, and pitch/64-1.
Guest LockRect returns an **8-byte** pair: 32-bit pitch and 32-bit guest address,
not native `D3DLOCKED_RECT` containing a 64-bit pointer. Mip storage remains in
the requested Xbox format; the CPU conversion uploads BGRA8 to the native
renderer. Guest code can write or swizzle its own source data without receiving
an invalid native pointer.

Implemented conversion: swizzled/linear ARGB8888, XRGB8888, RGB565, ARGB1555,
XRGB1555, ARGB4444, L8, A8, A8L8, and block-compressed DXT1/3/5. Morton addressing
handles rectangular power-of-two dimensions. Linear pitch is aligned to 64
bytes. Unsupported formats, tiled locks, invalid levels and unsupported
swizzled subrectangle locks return failure. Guest mip chains are allocated and
lockable, but **the existing native backend currently samples only level 0**;
mip filtering is not claimed implemented.

### Draw translation

Xbox primitive values are 1=points, 2=lines, 3=line strip, 5=triangle list,
6=triangle strip, 7=triangle fan, 8=quad list. They differ from the native header's
triangle values 4/5/6. The bridge converts vertex counts to native primitive
counts. Quad lists become two triangles per quad; they never use invalid core
OpenGL `GL_QUADS`. Unsupported primitive kinds return failure.

Even FVF handles for XYZ and XYZRHW are supported. Odd Xbox vertex-program
handles are explicitly rejected and invalidate the active bridge FVF, avoiding
rendering with a stale prior shader. The original tests bit 0 at `0x102946` to
distinguish these forms. Existing renderer XYZRHW requires clip-space input,
so bridge copies guest screen vertices, converts coordinates using the active
viewport, maps D3D depth to GL depth, and reconstructs clip W from reciprocal W.
Guest vertex data is never rewritten. Bound guest buffers draw from their
current bytes, so CPU writes do not leave stale native buffer copies.

This extension supports stream zero only. The native renderer still samples
texture stage zero, lacks Xbox programmable shader translation and full texture
stage state behavior. Render-target surface binding, indexed draws, palette
resources, external registered-resource adoption and full mip filtering remain
for later work. GetSurfaceLevel supplies a real shared guest surface view, but
this alone does not implement native render-to-texture. No placeholder textures,
fake logos, scene geometry or game-success stubs were added.

### Expanded validation

The same standalone build command now tests:

- Pixel-verified guest texture Create/Lock/Bind and textured screen-space quad.
- Guest vertex-buffer Create/Lock/Bind/Draw and pixel-verified untextured result.
- Surface-parent aliases, external/bound reference lifetimes and release.
- Morton-address facts, RGB565 conversion, DXT1 red endpoint and transparency.
- The original clear/channel/viewport, guest ABI, and swap checks.

Result: `PASS: native GL, clears, guest ABI, texture upload/quad, vertex buffer,
lifetime, swap`. Compiles with warnings treated as errors. Tests use deliberately
constructed tiny inputs, no game assets. macOS emits a one-time sampler warning
when an untextured draw follows deletion/unbinding of the previous texture;
asserted untextured framebuffer pixels are nevertheless correct and
`glGetError()` reports no error. Hardware/game rendering remains unvalidated.

## Framebuffer capture and CopyRects extension

Added two compiled replacements; both must be excluded from lifted definitions:

| Address | Function | Guest arguments | Callee argument cleanup |
|---|---|---|---|
| `0x000FF450` | GetBackBuffer | index,type,out | 12 bytes |
| `0x000FF580` | CopyRects | source,rectangles,count,destination,points | 20 bytes |

GetBackBuffer has a unique exact sparse-byte match to public 4134 signature
[at the previously recorded database revision](https://github.com/Cxbx-Reloaded/XbSymbolDatabase/blob/20eced544726f5558c5a408458f38a086cc4e543/src/OOVPADatabase/D3D8/4134.inl).
Its local code handles index 0, front index -1, and other buffer indices by
selecting singleton+`0x207C/0x2080/0x2084`, writes output and calls Resource AddRef.
The current bridge supports back index 0 and front index -1, type 0; additional
queued backbuffers are rejected rather than aliased to the same GL buffer.

CopyRects is identified by complete local code and call-site behavior, not an
exact sparse-signature match. At `0xFF588` it loads source argument one; after
local stack allocation it loads destination at `0xFF5AD`. It reads source format
and pitch, handles source rectangle arrays in groups of 16, synthesizes full
source rectangles when absent (`0xFF6C7` onward), and defaults destination
points to each source rectangle's origin (`0xFF764` onward). It submits both
resource data pointers to its transfer routine at `0xFF7CC`, then returns with
`ret 0x14`.

The frame routine `0x39E00` calls GetBackBuffer at `0x39E1E`, then CopyRects at
`0x39E40` with `(backbuffer,NULL,0,[0x4246D4],NULL)`, releases the backbuffer and
calls Swap. The destination is a real texture-level surface: game initializer
`0xAAF75` calls wrapper `0x3CFB0` (GetSurfaceLevel) and stores its result into
`0x4246D4` at `0xAAF7E`. This is a framebuffer capture path, likely for retained
background imagery; it does not imply that the game loads a static logo there.
The purpose is an inference; the observed copy direction and arguments are
explicit in the disassembly.

Implementation:

- GetBackBuffer creates a tracked guest surface representing the live native
  GL back/front buffer. Device ownership retains it after caller Release.
  The serialized header is linear ARGB8888 with native drawable dimensions;
  native pointers are never placed in guest memory. Singleton back/front
  resource globals are maintained. This does not expose a coherent CPU alias
  of GPU storage; direct surface locking remains outside this subset.
- CopyRects reads actual framebuffer pixels using native `glReadPixels`,
  preserves pixel-pack/read-buffer state and converts bottom-up GL rows to
  top-down Xbox rows. It can also source current tracked guest surface pixels.
- Copies to guest surfaces encode pixels into supported uncompressed Xbox
  formats and respect linear pitch or Morton layout. Parent textures are marked
  dirty so subsequent sampling uploads the captured image. Compressed source
  decoding is supported; compressed destinations are rejected.
- Copies to a framebuffer upload the supplied source region into a temporary
  GL texture and use `glBlitFramebuffer` with the required Y inversion. These are
  the actual source pixels; no replacement image or invented scene is involved.
  The temporary GL objects are released immediately and prior GL bindings,
  scissor enablement and pixel-unpack state are restored.
- Rectangles and destination points are bounds-checked, with no scaling.
  Source pixels are snapshotted before any destination writes, preserving
  overlapping-copy behavior. Unsupported/untracked surfaces return failure.

Expanded standalone test passed actual two-way copies. It clears differently
colored top/bottom bands, captures a 16x8 region into a guest texture-level
surface, verifies exact serialized colors and row orientation, restores that
surface to a different framebuffer location, and verifies exact framebuffer
pixels. It also verifies guest reference lifetimes and a nondefault pixel-pack
row-length state survives capture. Existing clear, texture/quad, vertex-buffer,
ABI and swap checks remain passing. Command remains the same standalone
`WRATH_GRAPHICS_SMOKE_TEST` command above; no full game build was run by this
subtask.

Observed result: `PASS: native GL, clears, guest ABI, texture/quad, vertex buffer,
lifetime, framebuffer copies, swap`. This remains subsystem validation; actual
menu/game behavior is owned by the main runtime integration effort.

## Boot-08 render-state boundary (2026-09-08)

Boot-08 reaches native CreateDevice640x480, then stalls in the original SDK
pushbuffer replenishment loop. `local/reports/boot-08.log` and
`boot-08-stack.log` show the live route `103740 -> FD830 -> 3A830 -> 9E860`.
Saved ECX at guest stack `0x011DFE04` is `0x40304`; saved EDX at `0x011DFE08`
is zero. Caller arguments at `0x011DFE1C/20` are state59,value0. Thus the first
blocked operation is ALPHABLENDENABLE=false, not missing game rendering data.

Four additional exact SDK entry points are exported in `src/graphics.c` and
must be excluded from automatic lifting:

| Address | SDK operation | Guest ABI |
|---|---|---|
| `0x000FD830` | SetRenderStateSimple | ECX=one-word NV097 method packet, EDX=value; bare ret |
| `0x000FDAD0` | SetRenderState_CullMode | one DWORD; ret4 |
| `0x000FDB40` | SetRenderState_FrontFace | one DWORD; ret4 |
| `0x000FE5C0` | SetRenderState_ZEnable | one DWORD; ret4 |

These identifications follow complete local instructions and caller evidence.
`FD830` writes ECX and EDX into the pushbuffer, and on exhaustion saves both
registers before calling `103740` at `FD84E`. It takes no guest stack arguments.
Game wrapper `3A830` reads the method packet from `15AD48+state*4`, invokes it
at `3A84F`, and mirrors value into `10EE18+state*4` at `3A854`. The SDK generic
setter `FD860` follows the same convention. The bridge replaces the SDK boundary;
the original compiled game wrapper and its state decisions still execute.

The original XBE table confirms these state/method pairs (hex method only):

```
57:354 58:33C 59:304 60:300 61:340 62:344 63:348 64:35C 65:310
66:37C 67:358 68:374 69:378 70:364 71:368 72:36C 73:360 74:350
75:34C 76:9F8 77:384 78:388 79:330 80:334 81:338
```

Packet headers add `0x40000` (one parameter). NV097 names and enum encodings
are corroborated by the pinned toolkit's
[`nv2a_regs.h`](https://github.com/sp00nznet/xboxrecomp/blob/051a128df5ec27ef14f1ceaaead11c5457321eef/src/nv2a/nv2a_regs.h).
Xbox comparison and blend-factor values use GL encodings; the host D3D API
uses different ordinal enums. The bridge explicitly converts these, converts
Xbox color mask bytes into native channel bits, mirrors the guest state cache,
and applies actual native depth, blend, masks, dither, stencil functions/ops,
blend equation/color and polygon-offset state. No pushbuffer cursor, GPU idle
register or hardware acknowledgment is fabricated.

`FDAD0` emits CULL_FACE_ENABLE method308, then method39C with GL_FRONT if
its winding equals FrontFace, GL_BACK otherwise. It mirrors CullMode at10F018.
CullMode accepts 0/GL_CW/GL_CCW and identifies the removed winding; native
D3DCULL enums reproduce that result. `FDB40` mirrors FrontFace at10F014 and
reapplies the current CullMode in the original SDK. The bridge retains this
guest value; its native culled winding remains unchanged. `FE5C0` emits depth
enable30C gated by depth-surface availability, mirrors at10F008 and has special
W-buffer work when value2 is used. Current bridge supports 0/1 and gates actual
native depth testing on the CreateDevice auto-depth setting; W-buffer stops.

Limits are explicit: enabled alpha test requires fragment-shader discard, which
the current upstream native shader does not implement. It aborts with method,
value and guest return address, as do unknown method packets, flat shading,
unsupported blend factors and W-buffer mode. This includes swath-width method9F8
and shader-combiner states. Disabled alpha test and its cached function/reference
are valid. The stencil fail/enable complex setters remain outside this subset;
unbridged complex SDK entries may still reach hardware queue code. No upstream
renderer source was changed in this step.

Validation: production source compiles with `-Wall -Wextra -Werror`. The existing
standalone ARM64 native graphics smoke test now calls these guest SDK boundaries,
asserts exact stack consumption and guest cache values, verifies native depth/
cull enablement, and draws 50%-alpha red over black using Xbox blend enums.
Actual GL readback produces red127..129, green0, blue0, demonstrating the native
renderer retained the translated blend state. Prior resource/copy/swap checks
still pass. This is subsystem validation; no game title/menu is claimed here.

## Boot-09 FillMode and initialization audit (2026-09-08)

Boot-09 passes the initial blend, depth and cull setters. The next stalled call
is FillMode(state120,value`0x1B02`) from game `9EB6E -> 3A8C1 -> FDDF0`, with
saved return `FDE03` after the SDK pushbuffer exhaustion call. Added exclusions:

| Address | SDK operation | Guest ABI |
|---|---|---|
| `0x000FDDF0` | SetRenderState_FillMode | GL_POINT/GL_LINE/GL_FILL DWORD; ret4 |
| `0x000FDF60` | SetTextureStageState_TexCoordIndex | stage,value DWORDs; ret8 |

FillMode local instructions emit consecutive front/back polygon mode methods
`38C/390`, and mirror the requested mode at `10EFF8`. Back mode comes from
`10EFFC` only when two-sided lighting `10F000` is enabled. Native GL core accepts
one polygon mode for both faces, so the bridge faithfully supports common
front/back modes and explicitly stops on distinct modes. It applies real
`glPolygonMode` and translates GL enum values into native D3DFILL enum values.

The full `9E860` audit finds FillMode is its last hardware-writing scalar setter.
Its Material call `3A6E0 -> FEB50` copies68bytes into singleton+9F0 and dirties
lighting state. Remaining state indices92,93,95,100,101,102,103 are deferred
cache writes in the compiled `3A830`; they do not themselves submit commands.
Their eventual shader/lighting semantics remain a separate rendering task.

`A01A0` records ten material state blocks with the repeated pattern
`3A780(BeginStateBlock) -> 9E860 -> AADE0 -> 3A7A0(EndStateBlock)`.
SDK Begin `1012CC -> 101290` clears recording flags; End `10191E -> 1012DA`
serializes changed guest states/material data into guest heap memory. These
CPU routines remain compiled game-library code; no replacement success token
is returned. The bridge's guest state mirrors preserve the recorded values.

`AADE0` sets texture-stage state through wrapper `3AC60`. Most states are
again deferred. The textured branch calls state28,value0 at`AAE5B`, which
routes to hardware-writing SDK `FDF60`. Its code stores value at
`10EC88+stage*128`, maps the explicit vertex attribute index at`1B11D1+stage`,
updates generated-coordinate flags at singleton+454 and dirties bits47F.
The new bridge supports stage0, explicit coordinate set0: the actual native
FVF shader reads those UVs. It reproduces those guest mirrors and native
D3DTSS_TEXCOORDINDEX cache. Generated coordinates and other coordinate/stage
selections stop explicitly; they require additional vertex-stage semantics.

### Native alpha test patch

The previous section's alpha-test limitation is superseded by
`patches/xboxrecomp-graphics.patch`. Static audit proves the same initializer
will request enabled alpha testing: material bits are set to blend mode1 at
`A0577` and `A0659`, then calls to`9E860` at`A05CB/A06AD` take its
`9E997/9E999` alpha-enable1 path. This justified fixing the renderer before
another full boot.

The patch changes only pinned upstream `src/d3d/d3d8_gl.c` (22 added lines).
It adds alpha-enable, comparison-function and reference uniforms, uploads
native D3D render-state values before draws, and executes real fragment
`discard` after texture/vertex-color modulation when the comparison fails.
All eight D3DCMP predicates are supported, with 8-bit ALPHAREF normalized to
0..1; default alpha testing remains disabled. The guest SDK simple-state
bridge now forwards enabled alpha test to this implementation. Apply this
patch after the other tracked runtime patches during bootstrap.

Validation used a tiny separately compiled updated GL renderer object linked
before the existing D3D archive, avoiding a full game build. Production bridge
compiles with `-Wall -Wextra -Werror`. Standalone native smoke tests pass 24
actual fragment-readback cases (all eight predicates, each with source alpha
below/equal/above the reference), real wireframe-vs-fill pixel checks, explicit
UV-set guest mirrors, and all earlier blend, resource, copy and swap tests.
The archived renderer library was not changed by this test, so a normal parent
build must pick up the patched source. These remain subsystem tests, not game
menu verification.

## Boot-10 actual render-target boundary (2026-09-08)

The next stall after real asset reads is SetRenderTarget, not an indexed draw.
`boot-10.log` has returnFF210 from original queue refill; exact native debugger
stack and `boot-10-surfaces.log` confirm game39BE1 calls SDKFEF20. Added bridges:

| Address | SDK operation | Guest ABI |
|---|---|---|
| `0x000FEF20` | SetRenderTarget | colorSurface,depthSurface; ret8 |
| `0x000FF830` | GetDepthStencilSurface | outputPointer; ret4 |

FEF20 reads color after48bytes local/saved stack atFEF2C, substitutes current
singleton+2070 for null color, then consumes optional depth. Its original
FF204..FF210 loop is where boot-10 stalls. FF427 invokes SetViewport with
zero origin, clamped full-target dimensions and depth range0..1. These are
API semantics carried into the bridge, without hardware queue execution.

Actual debugger evidence: game1E3CC8 contains019E1030; depth1E3CCC and fallback
1E8D60 are zero. Color header is
`010D0001 019E1080 00000000 00011221 271DF27F 00000000`.
Singleton2070/207C also contain019E1030;2074/2080 are zero. This is exactly
our tracked linear640x480,2560-byte-pitch backbuffer. Local399F0 obtains it
through native GetBackBufferFF450, then calls previously unbridged
GetDepthStencilSurfaceFF830 twice. The missing depth pointer exposed a real
CreateDevice compatibility gap despite native SDL auto-depth being present.

CreateDevice now materializes the requested linear D24S8 guest surface header
and retains it as a device-owned resource representing the native drawable's
actual depth/stencil storage. Native pointers are never serialized. FF830
returns the currently bound guest depth handle and increments external refs;
with no depth bound it writes null and returns original observed
D3DERR_NOTFOUND88760866. The guest data allocation is header backing storage,
not a coherent CPU depth-buffer mapping: direct depth locking/readback remains
outside the supported APIs.

SetRenderTarget currently accepts the tracked native backbuffer and this native
default depth resource or null. It actually binds GL framebuffer0/backbuffer,
mirrors singleton2070/2074, resets native and guest viewport to full target,
and enables depth/stencil according to the guest state cache and attachment
availability. A null depth target disables both tests and excludes depth/stencil
bits from subsequent clears, preserving the physically retained window depth
attachment. The device-owned depth reference survives caller Release and
binding changes. Texture/foreign render targets return an explicit diagnostic
and INVALIDCALL; no unrelated surface is silently redirected to the window.

Validation: production bridge compiles under `-Wall -Wextra -Werror`. Native
smoke clears real depth to0.25, draws a quad at0.5 and confirms no color appears;
detaching depth through the guest SetRenderTarget API makes that same quad
visible. It verifies GetDepthStencilSurface returns NOTFOUND while detached,
restores the depth handle and checks its retained lifetime. Existing fragment
comparison, blend, fill, resources, framebuffer-copy and swap tests still pass.
Only the tiny subsystem executable was built; no full game build by this subtask.

## Boot-11 fixed pipeline selection (2026-09-08)

Boot-11 advances past render-target binding and clear, then stalls at102738
from398CA/B5AB6. The exact API is SetShaderConstantMode, not SetPixelShader.
Added exclusions:

| Address | SDK operation | Guest ABI |
|---|---|---|
| `0x001026F0` | SetShaderConstantMode | mode DWORD; ret4 |
| `0x00102BB0` | SetPixelShader | handle DWORD; ret4 |

The public generic3911
[SetShaderConstantMode signature](https://github.com/Cxbx-Reloaded/XbSymbolDatabase/blob/20eced544726f5558c5a408458f38a086cc4e543/src/OOVPADatabase/D3D8/3911.inl)
exactly matches local entry `mov eax,[esp+4]`, `test al,10`, device-global load,
and `or ecx,200` at offset12. Local code maps input bit10 to device flags200,
stores the remaining mode at singleton+2018, and uploads default NV2A fixed
vertex-program constants when mode0 is selected. Game wrapper398C0 calls it.
The native fixed vertex shader already owns the corresponding matrix/uniform
pipeline; it needs no NV2A constant-bank packet. The bridge supports observed
mode0, mirrors guest flags and mode, and marks dirty1600 like the original.
Other modes stop explicitly until their constant-register semantics are needed.

The immediately following game sequence calls398A0 with FVF152 and3ADD0 with
zero. The former reaches existing SetVertexShader102940; the latter reaches
actual SetPixelShader102BB0. This setter stores the shader handle at
singleton+370. The null branch restores fixed texture-stage combination state,
restores the cached texture factor and marks dirty4800 (plus2000 when prior
shader texture dependencies require it). The bridge calls native SetPixelShader0,
restores native texture-factor state and reproduces these guest mirrors.
Nonzero Xbox shader objects require actual register-combiner translation and
stop explicitly; none is silently accepted as the fixed shader.

Validation: production bridge passes `-Wall -Wextra -Werror`. Tiny native smoke
invokes both new guest APIs, asserts ret4 consumption, the mode/shader globals
and dirty bits, then runs actual textured-quad, alpha-test, fill, blend, depth,
resource and framebuffer-copy readbacks successfully. No additional upstream
renderer changes or full game build were required.

## Boot-12 stencil setup and screen-queue audit (2026-09-08)

Boot-12 reaches9FFD0 and stalls atFE673 while requesting state125,value1.
Two additional SDK exclusions are implemented:

| Address | SDK operation | Guest ABI |
|---|---|---|
| `0x000FE660` | SetRenderState_StencilEnable | DWORD boolean; ret4 |
| `0x000FE6F0` | SetRenderState_StencilFail | GL stencil-op enum; ret4 |

FE660 writes NV097 STENCIL_TEST_ENABLE32C, gated on a non-null bound depth
surface at singleton2074, then mirrors value at10F00C. FE6F0 writes
STENCIL_OP_FAIL370 and mirrors at10F010. Both originally also recompute
NV2A early-depth-test optimizations through method1D84; the native graphics
driver owns that optimization. The bridge implements actual GL stencil tests
and fail operations and preserves the guest values; no NV2A idle/status reply
is fabricated.

The complete9FFD0 screen-queue sequence initializes alpha reference0,
stencil enable1, stencil functionALWAYS, depth-failKEEP, stencil-failKEEP,
reference1 and read/write masksFFFFFFFF. Every scalar boundary is now bridged.
Per queue-item branches use stencil-passREPLACE, INCR_WRAP, DECR_WRAP orZERO,
plus cull direction and blend factors already covered by the simple-state
bridge. The end resets smooth shading and stencil enable0. Lighting states
92/103 remain deferred guest cache writes and need later fixed-lighting work.

Queue types2/3 conditionally request SHADEMODE_FLAT atA0090. The existing
`patches/xboxrecomp-graphics.patch` now additionally gives the native fixed
vertex shader a flat color output; the fragment shader selects flat or smooth
color from the actual render-state cache while UVs continue interpolating.
GL uses the first-vertex convention, matching documented Direct3D
[flat shading semantics](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/bb153356(v=vs.85)).
The previous explicit flat-shading limitation is superseded. Shader programs
and unsupported fixed-lighting/texture-stage combinations remain outside this
small renderer extension.

Tiny native smoke tests verify an actual stencil-failing fragment writes
reference3 through REPLACE while leaving its color black, then a matching
EQUAL stencil test permits the same red geometry. They check guest stencil
mirrors and disablement. Another real draw gives the first vertex red and
other vertices differing colors: flat mode reads pure red inside the quad,
smooth mode reads interpolated color. All previous subsystem tests and the
production `-Wall -Wextra -Werror` compile pass. No full game build was run.

## Opt-in native frame capture (2026-09-08)

The existing native Swap bridge accepts two diagnostic environment variables:

```
WRATH_CAPTURE_FRAME=1
WRATH_CAPTURE_PATH=/absolute/path/to/frame.bmp
```

Frame numbering is one-based and counts actual native presentation submissions,
matching guest swap-count increments. Immediately before the requested Swap,
the bridge reads the real default-framebuffer GL_BACK pixels and writes that
single image as BMP. It flips native GL bottom-up rows into top-down image rows,
logs the exact count/dimensions/path, and releases its temporary buffers. It
never creates replacement game imagery. Capture is disabled by default; malformed
configuration reports once and is ignored. No new lifted-function exclusions
or upstream patch changes are needed.

The capture restores read framebuffer, read buffer, pixel-pack buffer, alignment,
row length, skipped rows and skipped pixels. The native game and render loop
continue normally after writing. File extension does not select a codec: output
is always BMP. Choose an ignored path under local/reports for actual game frames.

Standalone validation saved/reloaded `build/input/frame-smoke.bmp`, verified
exact top/bottom pixel orientation and dimensions, and retained GL_FRONT read
selection, an intentionally undersized bound pack PBO, nonzero row/skip values
and nondefault pack alignment. All prior native graphics assertions and the
production `-Wall -Wextra -Werror` compile pass. This test BMP contains only
subsystem test geometry; it is not evidence of original game rendering.

## First logo visual audit (in progress)

The actual native frame-15 capture reaches the original game's Universal logo,
but its dark background/bright outlines differ from the raw source. The original
`0x875A0` loads `Crashdat/Gfx/copyr1.raw` through string VA `0x162EA0`; this is a
512×512 RGB24/BGR24 payload (786432 bytes), with 239487 of 262144 pixels pure white
and a predominantly dark filled logo. The similarly named BMP is not the source
for this particular path. `local/reports/copyr1-raw-rgb.png` is an ignored lossless
diagnostic decode, not game-output evidence or a substitute render.

The descriptor type-2 route in `0x3CFE0` converts each source pixel to opaque
ARGB, normalizes it, calls the original D3DX saturation helper `0x10F661` with
saturation 1, clamps channels and repacks bytes through `0x7AF40`. An isolated
native harness extracting the exact generated statements `0x3D324–0x3D529` and
both helper bodies, with constants copied from this XBE, reproduces all 256
grayscale inputs exactly, including 255 → FFFFFFFF. This rules out that initial
conversion sequence in isolation. The remaining diagnosis requires comparing
the post-D3DX guest texture and active draw state against the actual framebuffer;
no corrective renderer change has been justified yet.

The follow-up LLDB dump resolves the mismatch: D3DX receives an actual opaque
white ARGB source (`0x1170FC` first source bytes are FFFFFFFF), but its output
texture at handle `0x0294D030`, data `0x0294D080`, is DXT5 (`0xF`), 512² with ten
levels. In the base level, 14364 of 16384 blocks begin
`01010000000000000000000000000000`: alpha endpoints 1 and black RGB endpoints.
The native draw has blending disabled, source ONE/destination ZERO, FVF 0x144,
stride 28. The renderer is displaying this already corrupted compressed data.

A hardware watchpoint on the actual write-combined alias (`data|0xF0000000`)
stops in DXT5 alpha encoding `0x124437`, called from `0x11E93F`. The latter calls
`0x11CBE3` to select x87 rounding toward zero (`FLDCW` with RC bits 0xC00), then
converts normalized channels with `FISTP(channel*255 + .5)`. The lifter previously
emitted host `llrint` without consulting the guest control word. Thus white
255.5 became 256; the game's unmasked byte packing yielded 0x01010100 and DXT
compressed it as black with alpha 1.

The generic FIST/FISTP lifting now explicitly honors all four guest RC modes,
including nearest-even independent of host floating-point rounding. Nonfinite
or out-of-range conversion stores the masked x87 integer-indefinite value
without an undefined C cast. It preserves FIST versus FISTP stack behavior.
The change is saved in the existing cumulative `patches/xboxrecomp-lifter.patch`;
no game/API replacement and no rendering adjustment are involved. The native
regression `tools/test_x87_rounding.py` checks all guest/host rounding combinations,
positive/negative ties, 16/32/64-bit bounds and invalid values, FPU stack effects,
and all 256 original D3DX grayscale byte conversions, under undefined-behavior
and float-cast-overflow sanitizers. Full regeneration/build/frame recapture is
still required to confirm the original logo after the fix.

## Texture render targets (root, post boot14)

SetRenderTarget now creates real GL framebuffer/color storage for uncompressed
texture-level surfaces, preserving the guest resource header/parent relationship.
At a target switch actual GPU pixels resolve to the original guest swizzled/linear
storage, so subsequent native texture uploads see the draw results. This initial
implementation favors correctness and bounded128x128 readbacks over an unverified
GPU-only alias path. Texture-depth attachments remain explicitly unsupported.

Active-target dimensions now govern viewport clamp and clear rectangle origin.
Window backbuffer ownership is independent from its current target binding.
The native graphics test draws a red top half over a blue offscreen8x4 surface,
verifies GPU pixels, switches to the window, verifies Morton-ordered guest pixels,
rebinds to check preserved orientation, and checks release lifetimes. It passes
with the current production D3D library; the older build/runtime library was stale
and is no longer the documented test link path. Full game validation is pending
the next boot with the independently fixed x87 rounding.
