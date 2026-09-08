# Controller backend — 2026-09-08

Implemented the SDL2 runtime backend for the requested Xbox One/Series and PS5
DualSense Bluetooth controllers. **Physical controllers and playable game input
have not been tested.** The game itself remains a separate integration effort.

Source: https://github.com/sp00nznet/xboxrecomp.git at
`051a128df5ec27ef14f1ceaaead11c5457321eef`. Reproducible modifications are stored in
`patches/xboxrecomp-input.patch` and applied to the ignored upstream checkout's
`src/input/`. Apply from the upstream root with
`git apply ../../patches/xboxrecomp-input.patch` (once).

Changes: stable port allocation across hotplug/reconnect; lazy initialization;
non-consuming SDL event pumping; zero-preserving saturated Y inversion; clamped
analog trigger scaling; packets updated only when the state changes; documented
RB/R1=Black and LB/L1=White shoulder mapping; capability reporting and rumble
error propagation; active rumble refresh; clean input shutdown. SDL provides the
controller-family mappings. No input device emulation or CPU interpreter is used
by this host backend. Test-only SDL virtual devices simulate input sources.

Integration: call input APIs on the SDL/main thread; they are not thread safe.
Use `xbox_InputShutdown()` before host `SDL_Quit()`. The existing
`src/usb/usb_gamepad.c` input report path calls `xbox_InputGetState(0)`, so it
receives these mappings. Its USB bus/device plumbing and output reports still
need game-specific validation; backend rumble alone does not prove game rumble.

The installed SDL2 is Homebrew `sdl2-compat` 2.32.70 using SDL3. Its
[virtual rumble callback adapter](https://github.com/libsdl-org/sdl2-compat/blob/release-2.32.70/src/sdl2_compat.c#L10350)
forwards the SDL2 integer success result to SDL3's boolean result without
conversion. The test adapts its fake callback for exactly version 2.32.70;
runtime error handling and physical-device behavior are unchanged.

Validation (single lightweight compile, no full build):

```sh
mkdir -p build/input
clang -std=c11 -Wall -Wextra -Werror -Ithird_party/xboxrecomp/src \
  $(/opt/homebrew/bin/sdl2-config --cflags) tools/test_input.c \
  third_party/xboxrecomp/src/input/xinput_device.c \
  $(/opt/homebrew/bin/sdl2-config --libs) -o build/input/test_input
build/input/test_input
file build/input/test_input
```

Result: PASS for SDL virtual-device mappings, centered/extreme axes, triggers,
packet stability, simultaneous devices, disconnect/reconnect, stable ports,
invalid/disconnected calls, rumble delivery/stop, shutdown/reinitialization.
`file` reports `Mach-O 64-bit executable arm64`. Input source compiles with
warnings treated as errors. The test skips if physical controllers are already
present, to avoid confusing real and synthetic port assignments.

Mapping:

| Original Xbox | Xbox One / Series | DualSense |
|---|---|---|
| A / B / X / Y | A / B / X / Y | Cross / Circle / Square / Triangle |
| Black / White | RB / LB | R1 / L1 |
| Left / right trigger | LT / RT | L2 / R2 |
| Start / Back | Menu / View | Options / Create |
| Sticks, clicks, D-pad | Same | Same |
