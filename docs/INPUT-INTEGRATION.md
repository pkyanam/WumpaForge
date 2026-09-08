# Wrath of Cortex native input bridge — 2026-09-08

`src/input_bridge.c` identifies seven entry addresses and implements their guest
ABI using the tested SDL controller backend. This bypasses the original Xbox USB
bus path while preserving actual host connection/state/rumble results. Physical
Bluetooth controllers and playable game input have **not** been tested.

## Integration

The source exports exact `sub_*` wrappers required by the generated dispatch
table, as well as `wrath_input_lookup`. Compile it with the existing Xbox input
library. From the manual
lookup in `src/overrides.c`, call `wrath_input_lookup(address)` and return a non-NULL
result. Exclude these seven addresses from the generated lift so direct generated
calls resolve to the replacement too:

```text
0x00153C66
0x00154249
0x0015429F
0x001542AB
0x0015431A
0x00154AA8
0x00154AAD
```

Call these routines on the SDL/main thread. `xbox_InputShutdown()` remains the
host's responsibility before `SDL_Quit()`. The bridge uses guest memory/register
symbols already provided by the runtime, the native `xbox_Input*` API, and
`recomp_lookup_kernel` for optional feedback event signaling. It introduces no
new dependency, CPU interpreter, USB emulation, or graphics/event loop.

## Address evidence

Signatures were fetched read-only from
[XbSymbolDatabase revision 20eced544726f5558c5a408458f38a086cc4e543](https://github.com/Cxbx-Reloaded/XbSymbolDatabase/tree/20eced544726f5558c5a408458f38a086cc4e543).
The SDK registration explicitly selects the older signatures for later builds:
[Xapi_OOVPA.c](https://github.com/Cxbx-Reloaded/XbSymbolDatabase/blob/20eced544726f5558c5a408458f38a086cc4e543/src/OOVPADatabase/Xapi_OOVPA.c#L525).
All fixed byte/offset pairs were checked against the supplied `default.xbe`
sections; relative-call targets and surrounding guest callers were then inspected
with Capstone. Sparse signatures alone were not treated as proof.

| Function | Address | Signature evidence | Guest ABI / return |
| --- | --- | --- | --- |
| XInitDevices / USBD_Init | `0x00153C66` | 3911, 10 fixed bytes, unique match; final `ret 8` at `0x153CF1` | stdcall `(count, preallocTypes)`, void |
| XInitDevices public wrapper | `0x00154AA8` | Exact `jmp 0x153C66`; called by game at `0xEB810` | Same two arguments and void return |
| XInputOpen | `0x00154249` | 4242, 12 fixed bytes, unique; calls GetTypeInformation `0x1547DA` and SetLastError `0xEDD33`; `ret 0x10` at `0x15429C` | stdcall `(deviceType, port, slot, polling)`, guest handle or zero |
| XInputClose | `0x0015429F` | 3911 wrapper has two raw matches; this one calls XID_fCloseDevice `0x155158` and is used by the game with XInputOpen's handle | stdcall `(handle)`, void, `ret 4` |
| XInputGetState | `0x001542AB` | 4242, 13 fixed bytes, unique; game calls at `0x363AB` and `0x36A21`; `ret 8` at `0x154317` | stdcall `(handle, state)`, Win32 error code |
| XInputSetState | `0x0015431A` | 4242, 12 fixed bytes, unique; game rumble caller `0x36322`; `ret 8` at `0x15434D` | stdcall `(handle, feedback)`, Win32 error code |
| XGetDeviceChanges | `0x00154AAD` | 3911, 9 fixed bytes, unique; game polls at `0xEB6DF` and `0xEB744`; `ret 0xC` at `0x154B17` | stdcall `(deviceType, insertions, removals)`, BOOL |

The other raw XInputClose-pattern hit, `0x00153D6D`, belongs to a different USB
object close wrapper called inside USBD_Init. It is deliberately **not** bound to
controller close. The real `0x155158` callee examines the XID object/type fields.

Matching sources:
[3911 signatures](https://github.com/Cxbx-Reloaded/XbSymbolDatabase/blob/20eced544726f5558c5a408458f38a086cc4e543/src/OOVPADatabase/Xapi/3911.inl),
[4242 signatures](https://github.com/Cxbx-Reloaded/XbSymbolDatabase/blob/20eced544726f5558c5a408458f38a086cc4e543/src/OOVPADatabase/Xapi/4242.inl).
The relevant 3911 signatures for XGetDevices, XInputGetCapabilities and XInputPoll
have no matches in this XBE. Their absence from these signature results does not
prove no variant exists, so no speculative addresses or bindings were added.

Raw reproducible local evidence:
`local/reports/scan_input_signatures.py`,
`local/reports/input-signature-matches.json`,
`local/reports/xbsdb/`, and `local/reports/disasm/asm/{text,XPP}.asm`.
The scan reads only the extracted XBE and does not alter it or the ISO.

## Guest layouts and behavior

The type-information table at `0x153404` points to `0x1534E4`, whose first byte is
1 (gamepad), type pointer at +4 is `0x1534AC`, input descriptor at +8 points to
`0x1534CC` (18 bytes), and feedback descriptor at +12 points to `0x1534D8`
(4 motor bytes). Game `XInputOpen` at `0xEB5DE` passes this gamepad type, a port,
slot zero, and NULL polling parameters. The default word at `0x1534E0` is
`0x0801` (autopoll enabled). The game also enumerates memory-unit type `0x153430`.

- The XPP device type is three guest DWORDs: current, changed, previous masks at
  offsets 0/4/8. Gamepad bits are native ports 0–3. Observed disconnects invalidate
  open handles; generation tokens prevent closed handles aliasing newly opened
  ones. No memory-unit device is attached, so its connected mask is zero.
- State serialization writes exactly 22 bytes: packet DWORD + digital buttons
  WORD + eight analog button bytes + four signed thumb WORDs. A native state
  struct has two trailing padding bytes, which must not overwrite guest data.
- Feedback is packed, with status DWORD at 0, 32-bit event token at 4, report ID
  at 64, report length at 65, and motor WORDs at 66/68. The game's rumble caller
  independently confirms these offsets: its feedback starts at controller+0x30,
  motors are written at controller+0x72/+0x74, and its event at +0x34 is zeroed.
- Rumble completion reports the backend's actual success/error synchronously.
  Nonzero event tokens go through the existing kernel NtSetEvent import at
  `0x15AA98`, preserving its guest-to-native handle table. No pointer truncation
  or direct event-token-to-HANDLE cast is used.
- XInputOpen failures update guest LastError using the title's inspected TLS
  addressing sequence from `0xEDD33`, including the runtime TIB at `0x1000` and
  the title's negative TLS index (-5), when that memory has been initialized.
- The confirmed game caller uses default autopoll. An explicitly opened manual
  polling handle keeps its initial native snapshot; there is no confirmed
  XInputPoll address to bind in this binary.

Cross-check reference definitions:
[Cxbx XAPI layouts](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded-legacy/blob/96aabe72e238674a22a61695fb6f5295259edf62/src/core/hle/XAPI/Xapi.h),
[Cxbx XAPI contracts](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded-legacy/blob/96aabe72e238674a22a61695fb6f5295259edf62/src/core/hle/XAPI/Xapi.cpp).
These were read as ABI references; the bridge is implemented against the local
backend and does not import that emulator's runtime. In particular, any future
capabilities bridge must serialize the packed Xbox layout; the backend's native
`XBOX_INPUT_CAPABILITIES` is a different structure.

## Validation

A targeted compilation with `-std=c11 -Wall -Wextra -Werror` succeeded.
`build/test_input_bridge` is a Mach-O arm64 executable. Its SDL virtual DualSense
controller exercised the **actual** native backend, checking all bound address
and stdcall stack operations, empty/connected/changed masks, no MU, gamepad fields,
22-byte writes, 70-byte rumble layout, native motor values, zeroing rumble on close,
closed-handle generations, disconnect/reconnect invalidation, and LastError
through the real low-address TIB/negative-index TLS layout. Optional event
handling was tested with a captured mock kernel dispatch (correct 32-bit token
and temporary stack), not with a real game event or physical controller.

The bounded test and executable are local diagnostics:

```sh
clang -std=c11 -Wall -Wextra -Werror -Ithird_party/xboxrecomp/src \
  $(pkg-config --cflags sdl2) tools/tests/input_bridge.c \
  third_party/xboxrecomp/src/input/xinput_device.c $(pkg-config --libs sdl2) \
  -o build/test_input_bridge
./build/test_input_bridge
```

No full game build or launch was performed for this input task. Physical
Bluetooth connectivity, game-thread placement, game input and actual event
completion still require end-to-end validation. Replacement between two polls
without an observed disconnected state cannot currently be distinguished from
continued attachment through the backend's boolean connection API.

## Desktop and Bluetooth controls update

See [CONTROLS.md](CONTROLS.md) for default keyboard/mouse controls, exact original
A/Start story-skip evidence, focus policy, and current regression commands.
Port0 now remains present as a keyboard source unless `WRATH_KEYBOARD=0`; existing
controller-only tests explicitly disable it. All guest layouts/SDK addresses are
unchanged. Physical controllers are merged into their existing SDL port mappings.
