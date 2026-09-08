# Native controls

Keyboard/mouse and the first SDL gamepad both feed original Xbox port 0. Pair an
Xbox One/Series controller or DualSense in macOS Bluetooth settings, then focus
the game window. Keyboard remains available if that controller disconnects.
Additional attached controllers retain ports 1–3; this title normally uses port0.

| Action / original button | Keyboard / mouse | Xbox One / Series | DualSense |
|---|---|---|---|
| Move / left stick | WASD | Left stick | Left stick |
| Menu direction / D-pad | Arrow keys | D-pad | D-pad |
| Jump, confirm / A | Space | A | Cross |
| Spin / X | X or left mouse | X | Square |
| Crouch, slide / B | C or right mouse | B | Circle |
| Status / Y | E | Y | Triangle |
| Begin, pause / Start | Enter or Escape | Menu | Options |
| Back | Backspace | View | Create |
| White / Black | Q / R | LB / RB | L1 / R1 |
| Left / right trigger | Shift / Ctrl | LT / RT | L2 / R2 |
| Left / right stick click | Z / V | Stick clicks | L3 / R3 |
| Right stick | Middle-button drag | Right stick | Right stick |

Story movies can be skipped with Space or Enter (A or Start). Release the button
between presses: the original game acts on rising edges. Mouse motion only affects
modes that actually consume the right stick; it does not invent free-camera
controls. There is no pointer-controlled menu. Keyboard diagonals stay within the
stick's circular range. Modern face buttons are digital; triggers remain analog.

Desktop input is neutral whenever the game window loses keyboard focus. Mouse
also requires that same window's mouse focus; Cmd shortcuts do not contribute
input. The cursor is never captured or warped. An SDL event watcher preserves a press/release entirely between game polls for
16ms starting with its first focused sample, so repeated immediate state reads
see the same tap. It then releases unless still held. Taps within that interval coalesce. Physical
gamepad focus/background behavior remains SDL's policy. `WRATH_KEYBOARD=0` disables
the desktop source at startup, useful for controller-only diagnostics.

## Original evidence

The [publisher's Xbox manual, pp3–4](https://manuals.plus/m/e0300ac0aaf55f791f92f31ecd0414829621ee726044faf434892b60c88f7e29)
confirms the basic actions above. The supplied XBE itself confirms the input path:
`363AB` calls native XInputGetState; `365D3` maps A pressure above32 to game bit40;
`3662A` maps Start to800. `36635–3663F` computes rising edges into controller+D0.
The original story loop at`2DB67` tests mask840 there and requests movie completion
at`2DB73`. Another original skip test is`2C8A5`. No game flags or scene state are
patched to manufacture these actions. The native bridge preserves its exact
22-byte Xbox input packet (see INPUT-INTEGRATION.md).

SDL documents [keyboard snapshots](https://wiki.libsdl.org/SDL2/SDL_GetKeyboardState),
[keyboard focus](https://wiki.libsdl.org/SDL2/SDL_GetKeyboardFocus),
[mouse snapshots](https://wiki.libsdl.org/SDL2/SDL_GetMouseState) and
[standard controller buttons](https://wiki.libsdl.org/SDL2/SDL_GameControllerButton).
Implementation is in the input-only toolkit patch; the pure desktop snapshot
helper is `src/input/desktop_input.h` in that checkout. Input calls remain on the
SDL/main thread. No renderer or translated game code changes are needed.

## Validation and limits

2026-09-08 native arm64 focused tests passed, each built with one compiler job:

```
clang -std=c11 -Wall -Wextra -Werror -Ithird_party/xboxrecomp/src $(pkg-config --cflags sdl2) tools/tests/desktop_input.c third_party/xboxrecomp/src/input/xinput_device.c $(pkg-config --libs sdl2) -o build/test_desktop_input
build/test_desktop_input
clang -std=c11 -Wall -Wextra -Werror -Ithird_party/xboxrecomp/src $(pkg-config --cflags sdl2) tools/test_input.c third_party/xboxrecomp/src/input/xinput_device.c $(pkg-config --libs sdl2) -o build/test_input
build/test_input
clang -std=c11 -Wall -Wextra -Werror -Ithird_party/xboxrecomp/src $(pkg-config --cflags sdl2) tools/tests/input_bridge.c third_party/xboxrecomp/src/input/xinput_device.c $(pkg-config --libs sdl2) -o build/test_input_bridge
build/test_input_bridge
```

Logs: `local/reports/desktop-input-test.log`, `input-controller-test.log`, and
`input-bridge-test.log`. Desktop tests cover focus/Command suppression, keys,
mouse signs/saturation, opposing directions/diagonals, merge preservation,
keyboard-only capabilities, and physical detach releasing buttons while the
logical port stays connected. Controller tests cover all six digital analog-button
channels and eight original digital buttons on both virtual families, stick
endpoints/Y inversion, trigger pressure, packets, hotplug and rumble. Bridge tests
cover original addresses, guest ABI, generation handles and event completion.

These are snapshot/virtual-device tests, not physical Bluetooth transport or
real macOS focus-transition/gameplay tests. No controllers were assumed connected.
Parent integration testing must confirm actual menu/skip/movement with the game
window and both real wireless controller families when available.

## Brief-tap latch (story UI follow-up)

The first real title UI test did not advance after short Computer Use Return/Space
presses. That is consistent with snapshot polling loss, but did not independently
establish that focus or guest handles were correct. The native input backend now
records non-repeat key/mouse down events without removing them from SDL's queue.
The next focused port0 poll merges each pending edge into the normal held snapshot
for a bounded16ms exposure interval. Key-up does not erase an unobserved press;
the normal snapshot supplies release once that interval expires. Focus loss/window close clears pending edges, mouse
leave clears mouse edges, and Command shortcuts clear/suppress desktop edges.
Unfocused or other-window events never migrate into the game on a later poll.

[SDL_AddEventWatch](https://wiki.libsdl.org/SDL2/SDL_AddEventWatch) may execute on
another thread, so its bounded state is protected by a spinlock. The callback
performs no SDL window calls, allocation, GL work, or event consumption. It ignores
quit events before locking, preserving SDL's signal-delivery behavior. Initialization
installs the watcher once; shutdown removes it. No game menu state is changed.

`tools/tests/desktop_input.c` additionally verifies down/up before polling,
release on the second poll, repeat suppression, same-poll coalescing, blur/refocus,
window isolation, Command suppression and held-state preservation. The actual
production watcher also passes worker-thread SDL_PushEvent delivery, untouched
event queue and shutdown removal in `tools/tests/desktop_latch_watch.c`:

```
clang -std=c11 -Wall -Wextra -Werror -Ithird_party/xboxrecomp/src $(pkg-config --cflags sdl2) tools/tests/desktop_latch_watch.c $(pkg-config --libs sdl2) -o build/test_desktop_latch_watch
build/test_desktop_latch_watch
```

Evidence: `local/reports/desktop-input-latch-test.log`,
`desktop-latch-watch-test.log`, `input-controller-latch-test.log`, and
`input-bridge-latch-test.log` all pass. Real title interaction awaits integration.

## Repeated original reads (story-02)

The original frame routine`86D30` calls`EB0A0` at`86D48`, which calls`369F0` and
XInputGetState at`36A21` for discovery/raw state. It then calls`36330` at`86D5C`,
which reads XInputGetState again at`363AB` and computes the game button edges.
The first read does not compute controller+CC/+D0. Consuming a desktop tap for
one API read therefore loses it before the original menu sees it.

The native desktop latch now exposes a tap for16ms from its first focused sample,
so those immediate repeated reads agree. This is a host input accessibility policy,
not an assertion that the Xbox USB polling period was16ms. Existing held SDL state
is still authoritative beyond that interval, and focus loss clears even an active
exposure immediately. Multiple taps inside the exposure window coalesce. Tests
cover both immediate reads, bounded release, active focus loss and timer wrap.

`WRATH_TRACE_INPUT=1` enables at most64 desktop focus/event reports and64 guest
input-read reports, including exact return caller, handle, result, packet and
A/Start state. It observes without injecting input and makes a further UI failure
distinguishable from absent focus, absent SDL events, or missing guest delivery.
Evidence: `desktop-input-doublepoll-test.log`, `desktop-latch-doublepoll-watch-test.log`,
`input-controller-doublepoll-test.log` and `input-bridge-doublepoll-test.log` under
`local/reports`. Actual title navigation still requires the parent integration run.
