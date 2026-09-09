# Current status

## macOS Apple Silicon

WumpaForge recompiles the supported USA original Xbox game ahead of time to native
ARM64. The user completed Arctic Antics with keyboard controls on an M3 MacBook
Air with 24 GB memory. This is demonstrated playability of one level, not validation
of the entire game. macOS 26 is the physically tested host; other supported build
hosts have not received a gameplay pass.

Public setup is focused on `setup.sh` (or the downloadable `install.sh` launcher):
choose a supported ISO, install necessary build dependencies, verify/extract,
translate, compile and package `build/WumpaForge.app`. A fresh source snapshot
successfully completed this entire path. A rerun verified the ISO/assets and reused
unchanged generated code and compiled outputs.

The app includes assets and native libraries, including SDL3 loaded by SDL2-compat.
It no longer needs the checkout or Homebrew after packaging. A relocated app
passed architecture, library-resolution, dynamic SDL loading, minimum-macOS,
asset and strict code-signature checks. The currently built dependencies require
macOS 26; each personal build records its actual minimum supported OS.

Bundled launches use `~/Library/Application Support/WumpaForge` for saves/run files.
A compiled sanitizer fixture verifies paths, permissions, repeated creation,
length bounds and collisions. Setup's save migration preserves existing files.
No game content or built app is included in the public source repository.

The final source build also includes the shared thread-local kernel-dispatch fix;
Mach-O symbols confirm actual TLS storage. A relocated app launched natively for
20 seconds, initialized graphics and audio and ended at its configured diagnostic
SIGALRM timeout. This is a launch check, not a new full-level playthrough. GitHub
asset-free CI passed on `aade68d` with Python 3.11 and 3.13.

## Known limits

- Story/hologram/attract-mode visual artifacts remain under investigation.
- Not every level, post-level hub return, or save/load route has been validated.
- Wireless Xbox/PlayStation mappings exist through SDL; physical pairing and
  reconnection tests remain outstanding.
- The 60 FPS target is not guaranteed in every scene. Upscaling sharpens the
  original 640×480 image; it does not add internal scene detail.
- Local ad hoc signing is provided; Apple notarization and prebuilt distribution
  are outside this source release.

## Shield experiment

Retained under [android/shield2019](../android/shield2019/README.md), separate from
Mac setup. An actual Arctic Antics load was observed, but hub/gameplay performance
remains inadequate. A newer indexed/O2 candidate builds and awaits device tests.
It is not a supported public-release platform. Leave the user's TV undisturbed.

The detailed investigation, measured frame windows and pending checks are in
[the Shield checkpoint](history/2026-09-09-shield-development.md). Earlier Mac
checkpoints are in [the September8 history](history/2026-09-08-checkpoints.md).

## Release scope

The repository is public at https://github.com/pkyanam/WumpaForge as of September 9,
2026. The owner selected GPL-3.0 for project source, preserving existing third-party
terms. Unauthenticated installer and source archive downloads were verified. Only source,
project branding, documentation and tooling are published. See the
[release audit](PUBLIC-RELEASE-AUDIT.md), [notices](../THIRD_PARTY_NOTICES.md) and
[licensing record](LICENSING.md). Generated code, ISO/assets, personal saves,
credentials, captures, binaries and dependency checkouts remain excluded.
