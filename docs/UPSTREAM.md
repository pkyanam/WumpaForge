# Source provenance

Shallow checkouts in ignored `third_party/`, inspected 2026-09-08:

| Directory | URL | Revision | Purpose |
| --- | --- | --- | --- |
| xboxrecomp | https://github.com/sp00nznet/xboxrecomp | 051a128df5ec27ef14f1ceaaead11c5457321eef | Selected static x86-to-C pipeline and compatibility runtime |
| OpenCrashWOC | https://github.com/Open-Travelers/OpenCrashWOC | a6f483f49c67402386b5294db0a8e35e91b26c2d | GameCube-derived recovered C reference; unbuildable upstream |
| crashwoc-decomp-ps2 | https://github.com/denzi-gh/crashwoc-decomp-ps2 | 9af06d394a332c4fe7a15d93f353047ae78a3672 | Initial reference; not the supplied platform |

Upstream code remains subject to its original license. Keep local runtime/tool
fixes reproducible as patches in the main repository before milestone commits.

The xboxrecomp README includes optimistic early claims but explicitly retracts
earlier playable Burnout/dashboard claims later in the file. Treat it as an
experimental toolchain. A successful lift/build is not evidence of a playable game.
