# Native runtime compatibility work

## Apple memory mapping

`patches/xboxrecomp-runtime.patch` supplies a real temporary-file-backed shared
RAM mapping on macOS. The temporary file is immediately unlinked. A sparse 4 GB
host virtual-address reservation contains guest RAM, aliases and device apertures;
only mappings wholly inside this owned reservation use `MAP_FIXED`. Exact-address
requests outside it fail if occupied instead of replacing unrelated mappings.
The reservation does not allocate 4 GB of resident memory.

`tools/tests/runtime_memory.c` verified base/mirror/tiled write aliasing, bounds,
aperture commits, release/recommit zeroing, and preservation of an occupied foreign
host mapping. Its measured run took 0.17 seconds and about 1.4 MB maximum RSS.

## Image-derived runtime storage

The upstream fixed storage at guest 0x00700000–0x0077FFFF and stack beginning
0x00780000 overlapped this XBE's .data/BSS. Runtime addresses now derive from the
maximum of all loaded section ends and the XBE header's base plus image size,
rounded upward to 64 KB. Guest image addresses remain untouched. The planner
reserves separate RenderWare compatibility data, kernel exports, TLS support,
the full image TLS template, thread data and the 8 MB stack. It rejects overflow
or insufficient mapped RAM. TLS thread storage follows the actual template size;
the former fixed +0x200 offset could overlap larger templates.

For the supplied XBE, `tools/tests/runtime_layout.c` verifies these addresses:

| Region | Guest address |
| --- | --- |
| Declared image end | 0x009500E0 |
| RenderWare compatibility data | 0x00960000 |
| Kernel exports | 0x009A0000 |
| TLS support structure | 0x009C0000 |
| PRCB support structure | 0x009C1000 |
| Image TLS template, 20 bytes | 0x009D0000 |
| Main TLS thread data, 64 bytes | 0x009D0040 |
| Stack range | 0x009E0000–0x011DFFFF |
| Initial ESP | 0x011DFFF0 |
| Heap range | 0x011E0000–0x03FFFFFF |
| Main TIB, below the image | 0x00001000 |

`XBOX_KERNEL_DATA_BASE`, `XBOX_STACK_BASE`, and `XBOX_HEAP_BASE` now read the same
runtime layout structure. Stack-top and worker-slice macros derive from it;
`xbox_MemoryLayoutInit()` and the launcher therefore initialise ESP consistently.
Heap bookkeeping resets after planning and before the first kernel allocation.
An audit found no duplicate live stack/heap/export addresses in `kernel.h`.
Total reported retail RAM remains 64 MB; this arrangement leaves 46.125 MB for
the main heap. Boot-04 and boot-05 confirm these relocated addresses in the native process,
with successful CRT and title-metadata initialisation. Gameplay remains pending.

The planner regression also checks a larger image and a TLS template larger than
128 KB, plus integer-overflow and exhaustion rejection. Both planner and memory
mapping regressions passed after this change.

## Optical root and media-status compatibility

LLDB on the first boot proved ordinal 202 was `NtOpenFile`, opening the bare
`\\Device\\CdRom0` device, not a symbolic-link query. A missing bare-device path
rule caused STATUS_OBJECT_NAME_NOT_FOUND. The path now resolves to the mounted
extracted game directory; a device-boundary check prevents matching CdRom01.
`local/reports/boot-02.log` confirms that open succeeds and boot advances to
IOCTL 0x4D014 (`SCSI_PASS_THROUGH_DIRECT`).

The next query is MODE SENSE(10), page 0x3E, requesting 28 bytes. The compatibility
bridge validates a live optical-device handle, the Xbox 44-byte direct-request
layout, all guest buffer bounds and the supported CDB. It reports the accessible
mounted Xbox game partition via an eight-byte mode header and twenty-byte status
page. Unsupported commands/pages return STATUS_NOT_SUPPORTED; invalid buffer
sizes fail. No challenge exchange or writable optical command is implemented.
The media-page layout is documented by the Xbox hardware research project:
https://xboxdevwiki.net/DVD_Drive#MODE_SENSE_and_MODE_SELECT

`tools/tests/optical_mode.c` checks lengths, page fields, transfer bounds and
unsupported-command rejection; `local/reports/optical-mode-test.log` records its
pass. The bridge now clears per-handle optical-device metadata on `NtClose`.

## Standalone regression commands

```sh
clang -std=c11 -Ithird_party/xboxrecomp/src tools/tests/runtime_layout.c -o build/runtime-layout-test
build/runtime-layout-test
clang -std=c11 -O0 -Ithird_party/xboxrecomp/src tools/tests/runtime_memory.c third_party/xboxrecomp/src/platform/win32_compat.c -o build/runtime-memory-test
build/runtime-memory-test
clang -std=c11 -Ithird_party/xboxrecomp/src tools/tests/optical_mode.c -o build/optical-mode-test
build/optical-mode-test
```

## Persistent HDD configuration and save routing

Boot-03's next failure was `NtOpenFile` on `\Device\Harddisk0\partition0`.
The lifted routine at 0x000ED6FA reads sector four (512 bytes at offset 0x800),
checks cache-ownership magic 0x97315286 and version 2, then initialises and writes
its own record when the sector is empty. Therefore the POSIX implementation
provides persistent empty storage and lets the game write that record; it does
not insert the upstream Windows branch's guessed partition-table contents.

Bare Partition0–5 device names map to lazily created `local/saves/PartitionN.img`
files with the retail configuration/partition sizes documented by
https://xboxdevwiki.net/Hard_Drive#Partitions . Partition0 is the 512 KB disk
configuration area; Partition1 is 5,132,779,520 bytes; Partition2 is 524,288,000
bytes; Partition3–5 are 786,432,000 bytes each. These are **logical** sizes:
`ftruncate` creates sparse files, so only sectors actually written consume
physical storage. Existing images are never reinitialised or truncated.

Native retail filesystem routes now keep HDD contents under saves:
E:/Partition1 uses `local/saves`, C:/Partition2 uses `SystemData`, and X/Y/Z use
separate `Cache/Partition3`, `Cache/Partition4`, `Cache/Partition5` directories.
Dynamic title-created T:/U: symbolic links take precedence; fallback T/U route to
TDATA/UDATA. D:/CdRom0 remains the extracted disc directory. This replaces
upstream's dashboard-specific practice of sending HDD paths to game assets.

Three files generated by earlier boot attempts were found under
`local/assets/UDATA/56550003`: TitleImage.xbx (10,240 bytes), TitleMeta.xbx
(68 bytes), and SaveImage.xbx (4,096 bytes), plus empty TDATA/56550003. They were
copied into `local/saves` and their original directories moved intact to
`local/reports/preserved-save-metadata`. No original files were deleted. Source,
preserved and destination SHA-256 values match; the manifest is
`local/reports/save-metadata-migration.json`. The asset directory again contains
only disc content.

`tools/tests/partition_devices.c` passed case-insensitive raw-device mapping,
logical sizes with less than 1 MB physical allocation, zero initial cache sector,
write/read preservation across reopening, and separate disc/HDD path routing.
It creates temporary images, checks them and removes its own test fixtures.

```sh
clang -std=c11 -Ithird_party/xboxrecomp/src tools/tests/partition_devices.c third_party/xboxrecomp/src/kernel/kernel_path.c -o build/partition-devices-test
build/partition-devices-test
```


## Directory-root distinction and boot-05

Boot-04 exposed a raw-device routing regression: XAPI opens bare Partition1
with CreateOptions 0x800021, including FILE_DIRECTORY_FILE. Such an open means
the mounted filesystem root, whereas a raw read/write device open needs the
sparse metadata image. `xbox_translate_path_for_open()` now carries directory
intent through symbolic-link resolution. Native filesystem root opens go to
the appropriate save directory; raw metadata opens continue to use image files.
The path regression test also checks the mounted Partition1 root is a directory.

One-job native rebuild passed. In boot-05, title metadata initialisation and the
raw Partition0 open both succeed. Ordinal 219 reads 512 zero bytes at offset
0x800 successfully. The next blocker is static code discovery, not storage:
`sub_000F5DD0` (a CRT copy/move routine) performs an indirect jump to 0x000F606C,
which has no generated function. Its generated function stops at 0x000F606A and
contains jump-table bytes misidentified as instructions. The native process
aborts on the unresolved compiled call before game main 0x00087BF0. Evidence:
`local/reports/boot-05.log` and `local/reports/storage-build.log`.

## Current-thread handle duplication after game-main entry

Boot-07 reached the game main routine, then ordinal 197 (`NtDuplicateObject`)
crashed when guest pseudohandle 0xFFFFFFFE became unsigned native pointer
0x00000000FFFFFFFE. Bridge handle resolution now sign-extends the defined current
thread/process pseudohandles. POSIX duplication also normalises a widened 32-bit
pseudohandle and materialises a real waitable current-thread object when the
host entered through `main`/`pthread_create`, rather than returning a dummy.
A pthread lifetime reference keeps that object alive; exit signals duplicates.
`GetCurrentThread()` consistently returns the pseudohandle.

Guest duplicate handles now get independent table tokens even when the POSIX
references share the same native object pointer. CLOSE_SOURCE consumes exactly
one native reference and removes its guest token without double-closing. Optical
handle metadata is retained on duplicates and cleared when a token is released.

`tools/tests/thread_handles.c` passed widened-pseudohandle duplication, active
thread state, priority retention, independently closed references,
CLOSE_SOURCE with ordinary and pseudo sources, and real pthread-exit signaling.
The bounded standalone test is recorded in `local/reports/thread-handles-test.log`.
Native in-game verification of this latest handle fix is left to the parent.

```sh
clang -std=c11 -O0 -Ithird_party/xboxrecomp/src tools/tests/thread_handles.c third_party/xboxrecomp/src/platform/win32_compat.c -o build/thread-handles-test
build/thread-handles-test
```

## Worker-thread isolation audit

The non-first `PsCreateSystemThreadEx` path already launched real POSIX threads
and used thread-local guest registers. Inspection found concrete isolation gaps:
the copied worker TIB retained the main TIB's self-pointer and stack limit;
worker TIBs leaked; allocation bookkeeping could race when the main thread and
worker allocated simultaneously; resource exhaustion ran workers inline.

Workers now receive a complete, distinct TIB/TLS context before launch, with a
correct TIB self-pointer and the actual 512 KB worker-stack lower bound. Both
ordinary return and `PsTerminateSystemThread` free worker TIB and stack storage.
Launch/allocation failure reports insufficient resources instead of executing an
infinite worker on its caller. Heap allocation, free and size lookup use the
platform reader/writer lock; worker-stack accounting uses interlocked operations.

`tools/tests/worker_memory.c` includes the actual production memory-layout code
and exercises its allocator/TIB routines without executing game code. Four real
pthreads keep guest registers, TLS values, TIB identity, stacks and 1,024 allocated
blocks distinct, then reclaim their stacks. The test passed in 0.30 seconds with
about 3.75 MB maximum RSS; evidence is `local/reports/worker-memory-test.log`.
The prior runtime-layout, memory-alias/non-clobber and handle regressions also
passed after these changes. No complete game launch was performed for this audit.
The parent separately seeded missing audio worker routine 0x0005DA30 for AOT
function discovery.

```sh
clang -std=c11 -O0 -ffunction-sections -fdata-sections -Wl,-dead_strip -Ithird_party/xboxrecomp/src tools/tests/worker_memory.c third_party/xboxrecomp/src/platform/win32_compat.c -o build/worker-memory-test
build/worker-memory-test
```
