# Current decompilation and native-port options

Read-only audit: 2026-09-08. Queried current GitHub repository metadata, all public
branches (one page, fewer than100 each), releases, recent issues/PRs, selected
source/build files and an additional active fork. No implementation changed,
no new large checkout, installation, or game build. Raw responses and selected
reference files are ignored under `local/reports/research/`.

## Recommendation

Continue the existing native ARM64 Xbox AOT route, using other decomps as semantic
references and occasionally replacing a narrowly verified Nu function. None of the
examined projects supplies a currently demonstrated native macOS game, complete
portable C engine, or a ready Xbox-source alternative. This is a relative effort
judgment, not a prediction that our remaining Xbox compatibility work is small.
The project already executes this supplied Xbox game's original logic, reads its
assets, and draws through native graphics; a wholesale C/asset-port pivot would
add missing game logic, pointer/endian reconstruction, asset conversion, and a
second platform backend while retaining graphics/audio integration work.

Do not confuse a matching console executable or an emulator-tested mod with an
ARM64 host build. The PS2 modding branch is useful evidence that selected recovered
C behaves in the game, but the rest of that game still runs as its retail MIPS ELF.
[Its own modding documentation](https://github.com/denzi-gh/crashwoc-decomp-ps2/blob/cb1bca991ea5768586e42b54c6cb3cbf493ca781/docs/modding-sdk.md)
explicitly describes fixed-address ELF patching and PCSX2 execution.

## Revisions and actual build products

| Project | Current audited revision | Observed status and target |
| --- | --- | --- |
| [Open-Travelers/OpenCrashWOC](https://github.com/Open-Travelers/OpenCrashWOC/tree/a6f483f49c67402386b5294db0a8e35e91b26c2d) | `a6f483f49c67402386b5294db0a8e35e91b26c2d` main; latest repository push2026-02-12 | GameCube-derived recovered C with incomplete host-port scaffolding; no published releases. |
| [denzi-gh/crashwoc-decomp-ps2](https://github.com/denzi-gh/crashwoc-decomp-ps2/tree/9af06d394a332c4fe7a15d93f353047ae78a3672) | `9af06d394a332c4fe7a15d93f353047ae78a3672` main; push2026-09-08 includes other branch activity | Matching PS2 PAL1.03 `SLES_503.86`; requires that original6,731,763-byte ELF. No releases. |
| [denzi-gh/crashwoc-decomp-gc](https://github.com/denzi-gh/crashwoc-decomp-gc/tree/4e0fda4222f014b60551fa1bc30cefaae18dce31) | `4e0fda4222f014b60551fa1bc30cefaae18dce31` main; push2026-07-16 | Matching GameCube USA Rev0 `GCBE7D` DOL, ProDG3.5/ninja; original SHA1 `c9cbd49a9eb0006f55533eb7d0fb5ebe2a73b72f`. No releases. |
| [calmsacibis995/crash-ps2](https://github.com/calmsacibis995/crash-ps2/tree/8f64fda50ec85fcee6b1570b690d995fa0c8ba23) | `8f64fda50ec85fcee6b1570b690d995fa0c8ba23` master; push2025-10-05 | Early PS2 USA1.00/E3 work; README explicitly says incomplete and unbuildable. No releases. |

GitHub push timestamps can refer to non-default branches and do not establish new
playable progress. The OpenCrashWOC and PS2 main SHAs are identical to those already
recorded locally; this audit independently rechecked them online.

OpenCrashWOC's [README](https://github.com/Open-Travelers/OpenCrashWOC/blob/a6f483f49c67402386b5294db0a8e35e91b26c2d/README.md)
labels building WIP and sound dummy; its percentage checklist is not tested native
playability coverage. The [CMake file](https://github.com/Open-Travelers/OpenCrashWOC/blob/a6f483f49c67402386b5294db0a8e35e91b26c2d/CMakeLists.txt)
merely gathers C sources into an executable. Our earlier same-revision compile
failed immediately on missing declarations in gamecode/ai.c and bug.c; see
`local/reports/opencrashwoc-build.log`. No redundant rebuild was needed. Direct
source inspection also finds unfinished cutscene code,32-bit pointer assumptions,
GameCube graphics calls, and a sound file with calls commented out. This is more
than a missing SDL link flag.

The GC project's [CMake file](https://github.com/denzi-gh/crashwoc-decomp-gc/blob/4e0fda4222f014b60551fa1bc30cefaae18dce31/CMakeLists.txt)
intentionally fails and directs users to ProDG/ninja. Its
[configuration](https://github.com/denzi-gh/crashwoc-decomp-gc/blob/4e0fda4222f014b60551fa1bc30cefaae18dce31/configure.py)
labels main, cut, nurndr, NuSound and gsprim objects NonMatching; that object-level
marker does not imply no useful functions have been reconstructed. It does mean
one cannot treat the source listing as a complete source-only portable game.
Its [graphics initialization](https://github.com/denzi-gh/crashwoc-decomp-gc/blob/4e0fda4222f014b60551fa1bc30cefaae18dce31/src/system/gs/gsinit.c)
still programs GX vertex descriptors. No macOS host renderer/build was demonstrated.

## Branches, current matching evidence, and additional projects

PS2 main has seven public branches: main, matchfunc, matching-functions,
matching-parallel-71, matching-various, modding-sdk and wipfun. The last is the head
of currently open [PR33](https://github.com/denzi-gh/crashwoc-decomp-ps2/pull/33),
revision `03ecbecd75b5e57295383125047980f4d0ecae85` (two changed files when queried).
The modding branch is `cb1bca991ea5768586e42b54c6cb3cbf493ca781`; it adds code to a
second PS2 ELF segment and hooks retail prologues. The author's documented test
boots and plays in PCSX2 with a HUD counter mod. It is an injection SDK for the
original console executable, with useful hook/lifetime methodology.

Counting checked-in PS2 main status entries yields843 matching,7 equivalent and2637
asm across195 status files (3487 entries). This is a function-status count, not
percentage of total executable bytes or portable source coverage; the symbol
registry separately lists3751 procedures. In particular,
[nusound status](https://github.com/denzi-gh/crashwoc-decomp-ps2/blob/9af06d394a332c4fe7a15d93f353047ae78a3672/config/pal103/status/nusound/nusound.toml)
has61 asm entries; [nurndr status](https://github.com/denzi-gh/crashwoc-decomp-ps2/blob/9af06d394a332c4fe7a15d93f353047ae78a3672/config/pal103/status/nu3d/nurndr.toml)
has23 matching and91 asm; nufile has32 matching/30 asm; main has19 matching/21 asm.
A [merged71-function PR](https://github.com/denzi-gh/crashwoc-decomp-ps2/pull/29)
reports retail image parity and843 matching claims verified. The
[pipeline documentation](https://github.com/denzi-gh/crashwoc-decomp-ps2/blob/9af06d394a332c4fe7a15d93f353047ae78a3672/docs/pipeline.md)
explains hybrid reconstruction and locked EE compiler/assembler use. Unrecovered
assembly remains required for the matching console build.

GC's other branches are crashwoc-multiplayer (`c19e217945b121c058f2b2967c29096f61f5edf5`)
and mod/custom-text (`b6bd4184416fa41b1c1d037ee6f45ce4232468ed`). These names are not
evidence of a host port. OpenCrashWOC has only main and a site branch; calmsacibis995
has only master. Recent issues in OpenCrashWOC ask for PS2/DOS support rather than
linking an existing playable host release.

The extra fork [ai-tdd-labs/OpenCrashWOC](https://github.com/ai-tdd-labs/OpenCrashWOC/tree/0f78eaa85ef5338d6667ba16f9b18d084bbc8724)
at `0f78eaa85ef5338d6667ba16f9b18d084bbc8724` is26 commits ahead/two behind upstream.
It adds Ghidra/DWARF and matching infrastructure. Its
[state document](https://github.com/ai-tdd-labs/OpenCrashWOC/blob/0f78eaa85ef5338d6667ba16f9b18d084bbc8724/decomp/ops/state/CURRENT_STATE.md)
reports a matching mixed DOL with8 C entries and2631 assembly entries, rather than a
complete C port. Its [scaffold](https://github.com/ai-tdd-labs/OpenCrashWOC/blob/0f78eaa85ef5338d6667ba16f9b18d084bbc8724/decomp/README.md)
is a function-matching workflow. It may help semantic mapping, but offers no
established native executable shortcut. Other listed forks are older; one returned404.

[chrisking1981/CrashWOC-PS2-Decomp](https://github.com/chrisking1981/CrashWOC-PS2-Decomp/tree/f4251625d81fe17351395ef086a995be2d319f51)
master `f4251625d81fe17351395ef086a995be2d319f51` contains README, research documents,
and three symbol lists: no game C source or build system in the returned tree.
It is useful name/type research. The README's source-reconstruction plans are not
an implemented engine. Its master branch resolves; querying main returns404.

No credible public Xbox-specific Wrath-of-Cortex source/native port was found in
these repositories, branches, fork inspection and targeted repository/web searches.
That is a bounded search result, not proof that none exists anywhere. Unverified
news claims about recompilation tooling were not treated as playable ports.

## Useful reuse now

| Work area | Concrete reference | Safe practical use in this Xbox target |
| --- | --- | --- |
| Cutscene/intro control flow | [OpenCrashWOC cut.c](https://github.com/Open-Travelers/OpenCrashWOC/blob/a6f483f49c67402386b5294db0a8e35e91b26c2d/code/src/gamecode/cut.c), [gcutscn.c](https://github.com/Open-Travelers/OpenCrashWOC/blob/a6f483f49c67402386b5294db0a8e35e91b26c2d/code/src/gamelib/gcutscn.c) | Names and behavior of InitCutScenes, LoadCutComponents, CutLoadScreenThreadProc, update/render/end callbacks. Cross-match local Xbox call graphs/strings before assigning addresses; no direct replacement based only on names. |
| Math, camera and scene animation | [numtx.c](https://github.com/Open-Travelers/OpenCrashWOC/blob/a6f483f49c67402386b5294db0a8e35e91b26c2d/code/src/numath/numtx.c), nuvec, nuhgobj; PS2 matching registries | Independent expected transforms, matrix order and animation data meanings for small numeric regression fixtures. A32-bit pointer-bearing Nu object cannot be cast into a native64-bit structure. |
| Rendering boundaries | PS2 nurndr and GC Nu renderer/GS routines | Identify batching, skins, material and scene ownership at higher-level Nu boundaries. The implementations terminate in different hardware APIs, so they do not replace Xbox D3D/NV2A support unchanged. |
| Sound/game policy | [GC nusound.c](https://github.com/denzi-gh/crashwoc-decomp-gc/blob/4e0fda4222f014b60551fa1bc30cefaae18dce31/src/nusound/nusound.c), [SS mixer](https://github.com/denzi-gh/crashwoc-decomp-gc/blob/4e0fda4222f014b60551fa1bc30cefaae18dce31/src/system/ss/ssmix.c) | Understand game-level loop tracking and volume/pan expectations. These are not Xbox DirectSound packet queues or Xbox ADPCM decoders; preserve our verified SDK ABI/backend. |
| Asset descriptions | [LibTWOC](https://github.com/Open-Travelers/LibTWOC/tree/513ccfe2f829684feaf04dd1929cc4d8b0a439fb), revision `513ccfe2f829684feaf04dd1929cc4d8b0a439fb` | MIT C# library with NUS geometry, materials, textures, skin and instance structures; use for small inspection/oracle tools, verifying Xbox variants before parsing. No need to import its whole dependency stack for one field. |
| Visual asset debugging | [travelers-toolkit](https://github.com/Open-Travelers/travelers-toolkit/tree/c22ef369c1c7d29b010d7c6096958639547167c2), revision `c22ef369c1c7d29b010d7c6096958639547167c2` | MIT OpenGL scene viewer; current README supports E3 scene files and warns object scenes may fail. Helpful format reference, not a player/runtime, and no proven Xbox retail asset converter. |

The shader/renderer layer is a particularly poor wholesale copy target: the Xbox
binary uses XDK4361 objects and shaders, GC code uses GX, PS2 code uses GS/VU paths.
The sound codecs and platform scheduling also differ. Reusing high-level semantic
knowledge can reduce blind reverse engineering; importing an unrelated hardware
backend would create another compatibility workload.

## Direct C port versus continued AOT

A source-only host port would first need a complete compilable selection of game
and Nu engine code, explicit treatment of missing functions, a64-bit-safe object
model, endian-aware asset readers, native rendering/input/audio/file APIs, and
validation against this USA Xbox revision. The checked projects target GC/PS2
originals that the supplied Xbox ISO does not contain. Converting assets is an
additional research task, not an established command in their build instructions.
Their checked-in source can be useful without proving that these gaps are solved.

Continuing AOT retains the supplied game's full original CPU logic and32-bit data
model. Its unresolved cost is correctness: compiler translation edge cases,
threads/callbacks, and complete reached graphics/audio/runtime behavior. Native
CPU code alone does not make it playable. Finish reached API boundaries and verify
real menu/gameplay before estimating completion. If a recurring subsystem proves
cheaper to express as direct C, replace that bounded Xbox function cluster only
after confirming ABI, shared memory and output equivalence with local disassembly.
A wholesale pivot becomes attractive only with new evidence of a complete portable
engine, compatible Xbox asset loading, and an actual native gameplay demonstration.

## Source provenance and license observations

OpenCrashWOC describes its work as reverse engineering without original source;
its current tree has no top-level license and GitHub reports no detected license.
The GC decomp likewise has no detected/top-level project license. That absence is
not an affirmative reuse license. PS2 denzi and calmsacibis995 trees contain
GPLv3 license files; chrisking, LibTWOC and travelers-toolkit identify MIT. These
are repository observations, not a determination of rights in every recovered SDK
fragment, debug table or game asset. Do not re-label all third-party material as
our newly authored code. Prefer factual ABI/format cross-checks and new focused
implementations; retain applicable notices for any intentionally imported code.
No files from a game disc were downloaded or published during this audit.
