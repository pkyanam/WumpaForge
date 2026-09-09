# Native resource registry pages

The former native table stopped at1024 resources regardless of remaining guest
RAM. Boot45's supported32x32 P8/DXT5 creation failures immediately before bad
resource releases motivate removing this arbitrary small cap. A live original
game capture is still needed to establish its causal role in that particular
crash; the component regression proves the hard limit itself is removed.

`src/graphics.c` now allocates resource metadata in256-record pages. Each record
is384 bytes on this arm64 build:96 KiB per page. Pages are allocated only after
all existing slots are occupied. The maximum128 pages bounds metadata at12 MiB
for32768 resources, excluding the1 KiB page-pointer array and allocator overhead.
No guest RAM or GPU resource is preallocated by this metadata capacity. Lookup,
free-slot search, and index-buffer range validation scan allocated pages only.

Page addresses never move, including while a child-surface allocation retains a
pointer to its parent. Release still destroys the actual native/guest resource
and clears its slot, which is reused before any new page is allocated. Empty
pages remain available until process exit so outstanding metadata pointers stay
stable. Guest handles and header layout are unchanged; this is native bookkeeping,
not extra Xbox physical memory. The existing `arg()` device lock covers registry
operations until `finish()` and remains unchanged.

A page-limit or calloc failure reports exact live count, capacity, host metadata
bytes, counts by resource type, total references/bindings and described guest
bytes (surface descriptions can overlap their owner's bytes). Output is bounded
to eight messages. Texture creation now reports E_OUTOFMEMORY and clears its
output on registry exhaustion instead of calling a supported format unsupported.
No original game failure is silently bypassed.

`tools/test_resource_pages.inc` invokes the actual guest IndexBuffer creation,
lookup and Release APIs to keep2046 index resources live simultaneously alongside
the smoke test's two existing surfaces. It verifies headers/data, retained metadata
pointers across page growth, resource creation beyond1024, exact release, reuse of
a freed slot beyond the old cap without growing, and release back to the prior
live count. A smoke-only eight-page bound exercises exhaustion at2048 resources
without unnecessary allocation or GPU objects. An additional supported DXT5
texture request verifies the accurate E_OUTOFMEMORY/output behavior at the bound.

One component compiler invocation with warnings-as-errors and the complete
existing graphics GPU smoke passed. Logs: ignored
`local/reports/resource-pages-build.log` and `resource-pages-smoke.log`.
No game launch or full game build was run for this task.

```sh
clang -std=c11 -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-missing-field-initializers -Wno-deprecated-declarations -DWRATH_GRAPHICS_SMOKE_TEST -Isrc -Itools -Ithird_party/xboxrecomp/src -Ithird_party/xboxrecomp/src/platform -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 src/graphics.c src/nv2a_vertex.c src/nv2a_vertex_input.c src/nv2a_pixel.c third_party/xboxrecomp/src/d3d/d3d8_gl.c -L/opt/homebrew/lib -lSDL2 -lepoxy -framework OpenGL -o build/input/test_resource_pages
build/input/test_resource_pages
```

For debugger snapshots use `s_resource_page_count`, `s_resource_pages[page]`,
and256 entries per page. `resource_capacity()` and `resource_at(index)` provide
the same traversal to code without assuming contiguous pages.

## Exact hot-handle lookup cache

Story23 snow profiling identified repeated linear page scans in original array
address commits. The512-entry pointer cache validates each cached pointer's live
handle, otherwise using the unchanged page scan. Pages remain address-stable;
release/reuse semantics and the separate retained-vertex generation validation
are preserved. Allocation is4KiB static on ARM64, without production timers or
counters. Full resource/GPU tests pass collisions, release/reuse and10000 warmed
hits with no scans. See [snow measurements and evidence](research/SNOW-RENDER-PERFORMANCE.md).
