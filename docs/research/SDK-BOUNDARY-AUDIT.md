# Native graphics boundary audit after the Arctic Antics portal hang

The story18 main-thread capture stopped at original103330 waiting for NV2A
PGRAPH register100410 bit10000. Its caller chain was103420/1035C0, reached
through InsertFence FED90 after the game's DrawVertices wrapper3AF90. The
next implementation is native GPU completion, not a register acknowledgement.

A static scan of compiled game functions belowFD000 for direct calls into
FD000..10B000 found the expected native bridge exclusions plus original SDK
helpers. The following are relevant for future evidence-driven diagnosis:

- FED90: InsertFence, reached by actual portal draw. Native bridge required.
- 103B20: resource IsBusy, called by game3A310/3A360. It reads resource+8
  timestamp, device+30 next sequence and the completed value via device+34.
  It must observe the same native completion contract as inserted fences.
- 1038D0: resource GetType, pure Common/Format header decoding; no GPU wait.
- FEB30: GetViewport copies six DWORDs from native-maintained guest device.
- FEB50: light data copy plus dirty bit1000; no direct MMIO.
- FE9A0: copies original default parameter data; no GPU activity.
- FEC00: writes a two-argument GPU method packet, called by3AAE0's optimized
  same-description texture binding route. It can call103740 when the original
  push buffer fills. Whether this fast route is active in current gameplay is
  not yet captured; do not infer it caused the reported hologram distortion.
  Root has not changed this path. A future failure here needs native method
  semantics and exact texture binding evidence, not a no-op flush.

Original instructions are in ignored reports/disasm/asm/D3D.asm and text.asm;
generated translations are under local/generated. Reproduce boundary coverage
by extracting RECOMP_ABI_CALL targets from generated game functions, excluding
config/manual-functions.json. This is static coverage information; it does not
claim every SDK path, dynamic call or visual effect is implemented.
