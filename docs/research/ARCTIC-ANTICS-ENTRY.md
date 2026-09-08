# Original Arctic Antics entry audit

Read-only audit of the supplied USA retail Xbox XBE, XDK4361, 2026-09-08.
No level state, game code, assets, input or launch behavior changed.

## Target identity

The requested first-area winter/penguin stage is **Arctic Antics**, original
internal **Level7**, first hub **Hub0**, portal **slot0**. Internal level numbers
are not the displayed stage order. Original Level37 is the Warp Room.

The original localized name table is `1AEF18`, six 32-bit language pointers per
level. `649B2..649BF` computes `level*6 + language`. Level7's English pointer is
`1AEFC0 -> 169388 -> ARCTIC ANTICS`; Level37 points to `WARP ROOM`.
`LData` starts at `19B138`, stride `58` hexadecimal. Its first pointer for Level7
at `19B3A0` is `161D24 -> a\snow_m\snow`; Level37 is `b\hub\hub`.

The actual extracted `Crashdat/levels/a/snow_m/` contains `snow.nux`, `snow.ter`,
`snow.ai`, `snow.crt`, `snow.wmp`, `snow.ptl`, `snow.obj`, `snowchars.dat`, and
associated visibility, light and recording files. `strings snowchars.dat`
contains `penguin`, `penguinp.hgo`, and `pfidle.ani`, `pfspin.ani`, `pfwalk.ani`
among other penguin animations, plus seal, narwhal and mammoth models.
The root asset audit already verified every extracted file against the ISO.

`HData` at `19A7F0` has 12-byte records: six signed level bytes, two signed spline
indices, barrier and debris bytes, then a 16-bit sound identifier. Its first
record is `07 0D 0B 11 04 15 02 12 FF A4 BF 00`:

| Portal slot | Internal level | Original English name |
| --- | --- | --- |
| 0 | 7 | Arctic Antics |
| 1 | 13 | Tornado Alley |
| 2 | 11 | Bamboozled |
| 3 | 17 | Wizards and Lizards |
| 4 | 4 | Compactor Reactor |
| 5 | 21 | Rumble in the Roks |

Original `HubFromLevel 5F5F0` scans these six records and writes `temp_hub`
(`560ED4`) and `temp_hublevel` (`561680`) on a match. This table identification
is also consistent with the locally inspected OpenCrashWOC reference, but the
addresses and values here come from the Xbox image itself.

## Original hub navigation and portal trigger

Player update calls `HubSelect 69140`, then `HubLevelSelect 5F7E0`, then
`HubMoveVR 698E0`, only when Level is37 (`2B53C..2B564`). `HubSelect` checks
movement across the hub spline boundary and writes `Hub` at `19A838` at `695D0`.
This is physical movement through the original world, not a menu selection.

For Hub0, the first spline index is2. Its loaded pointer is the DWORD at
`19C148` (`19C118 + 2*24`). The pointed-to original spline has a signed 16-bit
point stride at `+2` and a point-array pointer at `+8`. Arctic Antics' portal
center is point2; point3 supplies its direction. This permits a read-only live
position comparison without forcing any game state.

`HubLevelSelect` computes squared horizontal distances to portal points
`2,4,6,8,10,12`; it selects the nearest within squared distance3.24 for ordinary
hubs. It writes the candidate slot to `560F1C`, reads availability from
`569420 + Hub*6 + slot`, and looks up the exact level via HData at `5FB7C`.
For the actual warp trigger it requires horizontal distance strictly below1.0
(the radius constant at `19A860`), and absolute vertical center displacement
at most5.0 (`5FC49..5FC7E`). An available portal increments `23B7B8` until60
updates (`5FCA9..5FCB7`), then starts the transition at `5FDBF`.
There is no A/Start button test in this selector.

The transition writes pending warp target `19C074` at `5FE37` and sets its
60-update animation duration `561668` at `5FE42`. On completion, `5F81D` writes
actual requested next level `19C06C` from HData. For the target all three level
values eventually become7 as the original loader advances.

Use normal **WASD** movement to the Arctic Antics portal, then release movement
and stand at its center for at least one second. Space remains jump and Enter
pause; neither is needed to activate this positional trigger. The camera's
current orientation and initial live position determine which direction to
hold; a screen-relative direction cannot be justified from the table alone.
A brief movement followed by the live picture/state is preferable to guessing
one long hold. See `docs/CONTROLS.md` for the verified keyboard mapping.

## Spawn behavior and read-only verification

`PlayerStartPos 24300` has several original spawn paths. Returning from a level
with valid last hub/level calls `HubStart 68ED0`: it finds that level's portal
pair and positions the player two direction vectors beyond the portal center.
Otherwise it uses `pos_START`, pointer at `562F54`, when present. Its cutscene
mode uses `cutpos_CRASH` at `17BE80` instead. There is also an original
`gamecut_hack` flag at `1BAADC` which, when1, makes the prior hub0 and level21
before this decision. These are original behaviors, not proposed overrides;
which path runs after the current story/skip must be observed live.

Useful values to capture while using the UI:

| Address | Meaning | Target / use |
| --- | --- | --- |
| `19C068` | Current Level |37 in hub, then7 |
| `23B750` | Demo |0 for user gameplay; prior attract run was Level7/Demo1 |
| `19A838` | Hub |0 |
| `560F1C` | Candidate portal slot |0 |
| `560ECC` | Candidate available |1 |
| `23B7B8` | Portal dwell counter |0..60 |
| `19C074` | Pending warp target |7 |
| `19C06C` | Requested next level |7 after original warp animation |
| `8F518C` | Player position float xyz |Compare to loaded spline point2 |
| `19C148` | Hub0 spline pointer |Read point stride+2 and array+8 |

Exact breakpoints `5FE37` and `5F81D` identify original portal acceptance and
level-change request; the parent owns all live captures and launches.
Success requires actual `Level7 / Demo0`, the snow assets loaded, and responsive
movement/jump/spin in the winter stage. The previous automatic Arctic Antics
attract demonstration does not establish playable user control.

## Reproduce the static checks

Read the XBE section map from `local/reports/default_analysis.json`; map each
virtual address to `raw_addr + address - virtual_addr` for its section, then
read little-endian values from `local/assets/default.xbe`. Check `19A7F0` for12
bytes, `1AEF18 + 7*24` for the English string pointer, and `19B138 + 7*88` for
the asset pointer. Original instructions are in
`local/reports/disasm/asm/text.asm`; no proprietary bytes need to be committed.
`strings local/assets/Crashdat/levels/a/snow_m/snowchars.dat` independently
shows the penguin asset names above.
