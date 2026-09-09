# SHIELD TV Pro 2019 groundwork

**September 9 update:** [the isolated Shield build](shield2019/README.md) now
cross-compiles the full ARM64 game library and packages a development TV APK.
Device execution is still unavailable. The notes below describe the original
September 8 probe scaffold; its diagnostic has since been cross-compiled.

This directory contains an offline prerequisite checker and the source for an
isolated Android ARM64 memory diagnostic. **It does not build WumpaForge or an
APK.** There is no Android Activity, renderer port, game library or device result.
The root macOS build is unchanged. See [the plan](../docs/ANDROID-TV-PLAN.md).

## Checks available now

From the repository root, using existing Python/CMake and no Android SDK:

```sh
python3 -B -m unittest discover -s android -p 'test_*.py'
python3 -B android/preflight.py --help
clang -std=c11 -D_DARWIN_C_SOURCE -Wall -Wextra -Werror -fsyntax-only android/probe/memory_probe.c
```

The tests use synthetic metadata and CMake script mode. They verify rejection of
an unselected device, missing properties, 32-bit or inconsistent application ABI,
an insufficient/malformed API level, unsupported page size and wrong build target.
The syntax check covers the common mapping source on macOS; it neither links nor
executes it. `android_backing.c` uses Android's `memfd_create` declaration and is
not compiled or validated against NDK headers here.

## Device metadata, when available

Choose the exact authorized Pro serial in ADB. These are future read-only capture
commands, not commands run during this preparation:

```sh
adb -s SHIELD_SERIAL shell getprop > local/reports/shield-pro-2019.properties
adb -s SHIELD_SERIAL shell getconf PAGE_SIZE
```

Pass the **observed** page size (4096 below is an example, not a measured result):

```sh
python3 -B android/preflight.py \
  --properties local/reports/shield-pro-2019.properties \
  --page-size 4096 > local/reports/shield-pro-2019-preflight.json
```

Preflight is offline and never invokes ADB. It requires manufacturer NVIDIA,
device `mdarcy`, `arm64-v8a` in both application ABI lists, API 30 or newer and a
reported 4096/16384-byte host page. This identifies the selected initial target,
not every possible custom firmware. Exit 0 means metadata meets those gates;
`android_game_supported` remains false. Exit 1 means a gate failed, and exit 2
means invalid input. The JSON retains only selected properties; keep raw reports
in ignored `local/reports` because a full property dump can contain device IDs.

## Future cross-build of the diagnostic only

No NDK is installed by this directory. With an already installed, reviewed NDK,
set `WUMPA_ANDROID_NDK` to its actual root, then:

```sh
cmake -S android/probe -B build/android-probe \
  -DCMAKE_TOOLCHAIN_FILE="$WUMPA_ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-30
cmake --build build/android-probe --parallel 2
```

Coordinate the two-job limit with other builds. The CMake contract rejects host
macOS, 32-bit ABI/pointers and API levels below 30. This command is unexecuted;
successful offline contract tests do not prove a working NDK toolchain. The
intended output is the ELF diagnostic `build/android-probe/wumpaforge_memory_probe`.
Inspect it with the chosen NDK's `llvm-readelf -h -l` before any device execution:
require AArch64, the intended Android interpreter and 16 KiB load-segment
alignment. An ELF diagnostic is not an installable TV app.

The probe reserves a sparse 4 GiB span and maps a 64 MiB shared backing twice. It
checks page-aligned owned ranges, bidirectional alias bytes around page/physical
boundaries, alias replacement/recommit, and cleanup. Only a few backing pages are
touched. No guest code, executable guest mappings, GPU or game assets are used.
A shell pass would not establish the same permissions/layout inside an app;
application-sandbox testing and the remaining probes are still required.

No device execution, native cross-build, complete Xbox mapping test or memory
performance measurement was performed for this scaffold. Do not label its source
or synthetic tests as a playable port.
