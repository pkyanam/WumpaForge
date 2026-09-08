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
  src/graphics.c build/runtime/src/d3d/libxbox_d3d8.a \
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
