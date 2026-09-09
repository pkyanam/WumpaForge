# Third-party notices

Inventory updated for the September 9 public source preparation. This file
preserves component attribution; it does not grant a blanket license to this
repository, the game, or a future executable. See [licensing review](docs/LICENSING.md).

## Incorporated source and build dependencies

| Component and pinned source | Where used | License evidence and retained notices |
| --- | --- | --- |
| [xboxrecomp](https://github.com/sp00nznet/xboxrecomp/tree/051a128df5ec27ef14f1ceaaead11c5457321eef), `051a128df5ec27ef14f1ceaaead11c5457321eef` | Fetched by `tools/bootstrap.py`; linked by `CMakeLists.txt`; modified by all five `patches/xboxrecomp-*.patch` files | Copyright 2026 sp00nz; [MIT text](LICENSES/xboxrecomp-MIT.txt) applies subject to the [upstream exceptions](LICENSES/xboxrecomp-NOTICE.txt). Upstream MIT is not a license for every embedded component. |
| xemu-derived APU/NV2A code inside that xboxrecomp revision | `src/apu/{apu_core.c,apu_dsp.c,apu_vp.c,apu_regs.h,apu_state.h,apu_debug.h,fpconv.h,apu_shim.h}`, `src/nv2a/nv2a_regs.h`; headers additionally mark `apu.h`, `nv2a_core.c`, `nv2a_state.h` as derived | Copyright espes (2012), Jannik Vogel (2015, 2018–2019 as applicable), Matt Borgerson (2018–2025 as applicable); shim additionally credits 2026 Burnout 3 Static Recompilation Project. Upstream NOTICE says LGPL-2.1-or-later; source headers say version 2 or later. Preserve [LGPL-2.1 text](LICENSES/LGPL-2.1.txt) and original per-file notices; resolve this discrepancy and missing extraction SHA before redistribution. |
| [xemu shader references](https://github.com/xemu-project/xemu/tree/fdfb5a8f481b2f870c57080e74ec8d3a31a47053), `fdfb5a8f481b2f870c57080e74ec8d3a31a47053` | Tracked `src/nv2a_vertex.c` expressly retains `GPL-2.0-only OR GPL-3.0-only`; directly compiled into `wrath_native` | Reference copyrights: espes (2012), Jannik Vogel (2014), Matt Borgerson (2025); antecedents Aaron Robinson and Kingofc (2004), Shadow_tj and PatrickvL (2007). [GPL v2](LICENSES/GPL-2.0.txt), [GPL v3](LICENSES/GPL-3.0.txt). [Detailed provenance](docs/NV2A-VERTEX.md). This is a license choice between those versions, not unrestricted permission. |
| [sdl2-compat release-2.32.70](https://github.com/libsdl-org/sdl2-compat/tree/release-2.32.70) and [SDL release-3.4.14](https://github.com/libsdl-org/SDL/tree/release-3.4.14) | Host SDL2 API implemented by Homebrew sdl2-compat over SDL3; video, audio and input | zlib license; Sam Lantinga. [sdl2-compat notice](LICENSES/sdl2-compat-LICENSE.txt), [SDL3 notice](LICENSES/SDL3-LICENSE.txt). These are observed installed versions, not bootstrap pins. |
| [libepoxy 1.5.10](https://github.com/anholt/libepoxy/tree/1.5.10) | Host OpenGL dispatch; linked through CMake | MIT, with Intel Corporation and Khronos Group notices in [COPYING](LICENSES/libepoxy-COPYING.txt). Observed installed version, not a bootstrap pin. |
| [OpenSSL 3](https://github.com/openssl/openssl) | SHA/crypto runtime dependency from native Homebrew; bundled into personal local apps | Apache-2.0; [retained license](LICENSES/OpenSSL-LICENSE.txt). Exact installed source and version are recorded by Homebrew; package manifest records the copied library. |
| [Capstone 5.0.7](https://github.com/capstone-engine/capstone/tree/5.0.7) | Python disassembly/lifting dependency pinned by `requirements.txt`; not a game runtime library | BSD-3-Clause text: copyright 2013 COSEINC; designed/implemented by Nguyen Anh Quynh. [Installed package license](LICENSES/Capstone-LICENSE.txt). A distributed wheel/toolchain needs its complete package-level third-party inventory. |
| [vgmstream](https://github.com/vgmstream/vgmstream/tree/09c9f40caae4747e44b6a993b3d5b654cef4d1f7), `09c9f40caae4747e44b6a993b3d5b654cef4d1f7` | `tools/generate_adpcm_fixture.py` downloads/extracts decoder code into an ignored test oracle; tracked numeric fixture is generated from synthetic input. The native ADPCM implementation cites this reference. | ISC-style permission; [complete upstream COPYING](LICENSES/vgmstream-COPYING.txt) retains all listed authors/portions. Reference source is not a compiled game dependency. See [ADPCM provenance](docs/ADPCM.md). |
| [XbSymbolDatabase](https://github.com/Cxbx-Reloaded/XbSymbolDatabase/tree/20eced544726f5558c5a408458f38a086cc4e543), `20eced544726f5558c5a408458f38a086cc4e543` | SDK signature identification documented in D3D, input and audio integration notes | MIT, copyright 2023 The XbSymbolDatabase authors. [License text](LICENSES/XbSymbolDatabase-MIT.txt). Downloaded database stays ignored; retain notice with any copied substantial portions. |

The audio patch modifies LGPL-marked APU files as well as other toolkit code.
Runtime, graphics, input and lifter patches principally modify the toolkit's
MIT-covered files. Patch context/deletions are also upstream material; a patch
format does not remove its original licensing. No license for newly authored
unmarked files is inferred from their directory name.

## Consulted reference projects

These references support diagnostics and format/behavior research. They are not
whole-project runtime dependencies. The vertex compiler's declared GPL status
above is an explicit exception to any general description of references as
uncopied. Assertions of independent implementation in existing notes have not
been replaced by a new clean-room or ownership certification.

| Reference | Exact recorded revision | Scope / license observation |
| --- | --- | --- |
| [Cxbx-Reloaded-legacy](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded-legacy/tree/96aabe72e238674a22a61695fb6f5295259edf62) | `96aabe72e238674a22a61695fb6f5295259edf62` | Xbox graphics/audio/input definitions; inspected headers GPL-2.0-or-later. |
| [Cxbx-Reloaded](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded/tree/585c49a50af1255ab155099e06f24505f9c5a800) | `585c49a50af1255ab155099e06f24505f9c5a800` | Graphics types and hardware contracts. Audio notes also cite moving `master` URLs; those reads need exact commit recovery before release provenance is complete. |
| [envytools](https://github.com/envytools/envytools/tree/f102b82381f3f11cee113d16374c87091db039d9) | `f102b82381f3f11cee113d16374c87091db039d9` | NV20 register descriptions. `rnndb/copyright.xml` supplies MIT-style database terms and contributor list; this is not a claim about every tool/file in envytools. |
| [nxdk](https://github.com/XboxDev/nxdk/tree/29638d0b001f179b73c3513489af10ddc2986216) | `29638d0b001f179b73c3513489af10ddc2986216` | Kernel/thread/pbkit references; per-file terms vary and were not exhaustively inventoried. No nxdk library is linked by this build. |
| [OpenCrashWOC](https://github.com/Open-Travelers/OpenCrashWOC/tree/a6f483f49c67402386b5294db0a8e35e91b26c2d) | `a6f483f49c67402386b5294db0a8e35e91b26c2d` | Ignored GameCube-derived reference checkout; no top-level project license found. No affirmative general reuse grant inferred. |
| [crashwoc-decomp-ps2](https://github.com/denzi-gh/crashwoc-decomp-ps2/tree/9af06d394a332c4fe7a15d93f353047ae78a3672) | `9af06d394a332c4fe7a15d93f353047ae78a3672` | Ignored matching-decomp reference; top-level GPL v3 text, not a grant over the retail game. |
| [crashwoc-decomp-gc](https://github.com/denzi-gh/crashwoc-decomp-gc/tree/4e0fda4222f014b60551fa1bc30cefaae18dce31) | `4e0fda4222f014b60551fa1bc30cefaae18dce31` | Research-only alternative; prior audit found no top-level license. |

Other alternatives surveyed, including LibTWOC, travelers-toolkit and additional
decomp forks, have exact references in [the architecture survey](docs/research/DECOMP-OPTIONS.md).
Published NVIDIA/Khronos specifications and Apple/Microsoft documentation retain
their respective notices; citing a specification does not grant rights to other
code or assets. No AMD FidelityFX CAS code was imported for the current sharpening
filter; the [presentation study](docs/research/PRESENTATION-UPSCALING.md) discusses
it as a possible alternative.

## Excluded proprietary input and retained text provenance

The supplied game ISO, extracted XBE/assets, generated game C, shader dumps,
screenshots, binaries and dependency checkouts remain ignored. Their exclusion
from Git does not license the game-derived executable. Names and game-specific
research in tracked files remain subject to a separate rights review.

`LICENSES/` contains verbatim copies from the pinned xboxrecomp checkout, pinned
xemu `COPYING` (GPL v2), the inspected PS2 checkout (GPL v3), pinned
XbSymbolDatabase, ignored vgmstream COPYING, and the installed package paths
listed above. These texts apply only where their component licenses apply; their
presence does not relicense the repository. Project licensing is described in the root README; these retained notices do not replace component terms.
