# WumpaForge

<img src="assets/branding/wumpaforge-icon.png" width="128" align="right" alt="WumpaForge fruit and crate icon">

Build **Crash Bandicoot: The Wrath of Cortex** as a native Apple Silicon Mac app
from your own **USA original Xbox ISO**.

WumpaForge recompiles the original game code ahead of time to ARM64 and provides
native graphics, audio and input compatibility. No CPU interpreter or JIT is used.
No game files are included in this repository.

**Experimental:** Arctic Antics has been completed on an M3 MacBook Air. Other
levels and some story effects still need testing. This is not a promise of a
crash-free game or 60 FPS in every scene.

## Build your app

On an **Apple Silicon Mac with macOS 14 or newer**, paste this into Terminal:

```sh
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/pkyanam/WumpaForge/main/install.sh)"
```

1. Choose your USA **Xbox** ISO when the file picker opens.
2. Let setup install the build tools, verify/extract your disc, recompile the game
   and package the app. Apple or Homebrew may ask for installation approval or an
   administrator password. If Apple Command Line Tools must be installed, finish
   that installer and rerun setup.
3. Open **`build/WumpaForge.app`** in the source folder printed by setup. You can
   move the app into Applications; its game assets and libraries are included.

Use an original unmodified USA Xbox image. PS2/GameCube images, other regions and
modified executables are unsupported. Setup verifies the executable's SHA-256
before using it. Supply your own disc image; no game download is provided.

Allow several GB of free space. Builds use at most two compiler jobs. No GitHub
account, AI subscription or API key is required. The first build takes longer;
rerunning the same checkout reuses verified translated code and compiled objects.
The full build log is in `local/reports/setup.log`.

Prefer to inspect the scripts first? Download the repository ZIP, extract it, and
double-click **`Build WumpaForge.command`**. Or use an existing checkout:

```sh
./setup.sh "/absolute/path/Crash Bandicoot - The Wrath of Cortex (USA).iso"
```

The ISO stays unchanged. After building, the app is independent of the source
checkout and Homebrew installation. It is a **personal local build**, not a game
binary for redistribution. Build on the Mac you intend to use: the app records
the minimum macOS version required by its actual compiled dependencies. Only
macOS 26 on Apple Silicon has received physical testing so far.

## Play

| Action | Keyboard / mouse |
| --- | --- |
| Move | WASD |
| Jump / confirm | Space |
| Spin | X or left mouse |
| Crouch / slide | C or right mouse |
| Pause | Enter |
| Fullscreen | F11 or Control–Command–F |

Focus the game window before using controls. Pair Xbox One/Series or PS5 DualSense
controllers in macOS Bluetooth settings; SDL supplies the mappings. Physical
Bluetooth validation remains pending. See [all controls](docs/CONTROLS.md).
The original story is available to watch or skip.

For sharper output, use **Display → Window Size → 1440p** and
**Display → Sharpening → Medium**. The window can be resized or maximized. This
scales the original 640×480, 4:3 image; it does not create new scene detail.
F10 changes sharpening, not resolution. [More display options](docs/WINDOW-PRESENTATION.md).

Saves live in `~/Library/Application Support/WumpaForge/saves`. Replacing or moving
the app keeps them intact. Setup copies older checkout saves on first migration
without overwriting existing user saves.

## Troubleshooting

- **Unsupported ISO:** verify the platform is original Xbox, region USA, and the
  disc image is unmodified. The required `default.xbe` SHA-256 is
  `e8d7cbf225d899eb88227c11d1f40434c34e27c1b23ed946fb2c3471168d2f4d`.
- **Build stopped:** inspect `local/reports/setup.log`, resolve the displayed error
  and rerun `./setup.sh` with the same ISO. Existing inputs are preserved.
- **Wrong architecture:** disable “Open using Rosetta” for Terminal and rerun.
- **macOS blocks a downloaded script:** review it, then run `bash setup.sh` from
  Terminal. The locally built app is ad hoc signed, not Apple-notarized.
- **Rendering or gameplay bug:** include your Mac model, macOS version, level,
  reproduction steps and relevant logs in an issue. Never attach your ISO,
  extracted assets or generated game code.

## Development

This is primarily **static recompilation**, informed by reverse engineering;
it is not a full reconstruction of the original readable game source.
[Status and known issues](docs/STATUS.md) · [Testing](docs/TESTING.md) ·
[Changelog](CHANGELOG.md) · [Source provenance](docs/UPSTREAM.md).

For AI-assisted setup, open the repository in an agent and say:
“Read AGENTS.md, then run setup.sh with my ISO at `/absolute/path/game.iso`.”
Agents should preserve user saves, keep game content out of commits and report
untested behavior honestly. AI assistance is entirely optional.

The [Shield Pro 2019 experiment](android/shield2019/README.md) is retained in
`android/shield2019/`. It is separate from the Mac setup and **not a supported
public-release target**. No Android SDK is needed to build the Mac app.

## Licensing and attribution

WumpaForge project code is licensed under [GPL-3.0-only](LICENSE), except where
a file carries its own third-party terms. Existing third-party licenses and notices
are preserved. See [third-party notices](THIRD_PARTY_NOTICES.md) and the
[licensing inventory](docs/LICENSING.md). Game content and trademarks belong to
their respective owners; the project is unaffiliated with them. Source licenses
do not grant rights to distribute the original game or a built copy containing it.
