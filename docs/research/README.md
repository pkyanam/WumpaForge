# Native port research checkpoint — 2026-09-08

The requested research pause is complete. Three bounded subagents examined
decompilation alternatives, Xbox graphics, and startup/runtime behavior; root
audited ARM64/static recompilation. No renderer implementation, game build or game
launch was performed during this research. Graphics work before the pause is
checkpoint86b9630.

- [Existing decompilation options](DECOMP-OPTIONS.md): current repositories,
  branches and build products; no demonstrated ready native port in the audited set.
- [Xbox graphics audit](XBOX-GRAPHICS-AUDIT.md): confirmed fixed texture/alpha and
  sampling omissions; winding and other live-state questions requiring measurement.
- [Startup/runtime audit](STARTUP-RUNTIME-AUDIT.md): exact intro/fade/animation fields
  and why host frame count is insufficient to explain black output.
- [ARM64/AOT audit](ARM64-AOT-AUDIT.md): native execution versus semantic correctness,
  guest-width boundaries, floating point and concurrent-memory assumptions.

## Decision and next work

Continue the current ARM64 AOT route with targeted Nu/game semantic references.
A wholesale C-port switch would add missing game logic, platform backends and
Xbox asset adaptation without supplying a verified playable engine. Keep this
decision open to new evidence of a substantially more complete native project.

Next take a bounded diagnostic pairing original intro progression with native
rendering state. Establish whether the scene advances with fade zero, which draws
produce fragments, and whether a later clear/overlay/target switch removes them.
Implement and test the first reproduced contract mismatch. Do not guess at timing,
flip all winding, bypass the original scene, or treat a synthetic title image as
progress. Keep controller/audio work visible, but prioritize real title/menu output.

Current evidence remains: native startup and Universal splash work; original
intro/cutscene code runs; frame120 of boot26 is black. No verified title/menu,
gameplay, audible game sound, or physical Bluetooth controller test. The remaining
uncertainty does not support a reliable completion ETA.
