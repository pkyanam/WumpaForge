# Public source release audit

Snapshot: 2026-09-09, source `ddb83af`. Scope: a source repository that builds a
local Apple Silicon app from the user's supported USA Xbox ISO. This audit does
not approve distributing the resulting game executable or assets. Repository
visibility was not changed.

## Release preparation completed

The findings below describe the earlier snapshot, not outstanding release gates.
Preparation through `aade68d` resolved the actionable items:

- The owner selected GPL-3.0; root `LICENSE` and README apply GPL-3.0-only to
  project code while preserving component licenses and notices.
- A fresh source checkout completed ISO verification, extraction, translation,
  native compilation and standalone app packaging. A repeat setup reused verified
  translation. The relocated app passed ARM64, dependency, bundled SDL3 loading,
  asset and strict signature checks. A bounded 20-second native launch initialized
  graphics and audio, then ended through the configured SIGALRM timeout.
- Packaging copies runtime assets and dependencies, includes notices and records
  library hashes and actual minimum macOS in a build manifest. No resulting app
  or game material is published.
- Public contributor instructions replace session authorizations. Current Shield
  commands use device placeholders. Existing development history is retained;
  historical local paths and Git author metadata remain visible.
- The shared kernel-dispatch TLS fix passed forced-interleaving tests and the
  fresh Mac binary contains the slot in Mach-O thread-local sections.
- GitHub asset-free CI passed both Python versions at `aade68d`.
- The repeated reachable-history scan found 870 unique blobs (13,550,769 bytes),
  only the generated branding PNG as binary content, no matched secret signatures,
  and no tracked ISO, XBE, generated payload, library, APK or game asset paths.

The owner explicitly authorized public visibility after preparation. The repository
was made public on September 9, 2026. Unauthenticated installer download matched
the checked-in script and passed shell syntax validation. The public source archive
contained 382 entries with no game payload paths. Known visual artifacts and
incomplete whole-game validation remain documented in STATUS.md.

## Original snapshot: content and history

Inspected every unique blob reachable from all local Git refs, plus the tracked
worktree. This extends [the earlier staging audit](REPOSITORY-AUDIT.md).

| Check | Result |
| --- | --- |
| Reachable commits / unique blobs | 249 / 842 |
| Historical uncompressed blob bytes | 13,400,760 |
| Tracked files / bytes at inspection | 341 / 4,070,368 |
| Binary blobs | One: `assets/branding/wumpaforge-icon.png`, 1,710,212 bytes |
| Branding provenance | Newly generated artwork, documented in [BRANDING.md](BRANDING.md); not extracted game material |
| Game input, generated C, compiled products, captures | No matching tracked or historical payload paths found |
| Common secret signatures | No GitHub token, AWS access-key ID, private-key block, or OpenAI-style key signatures found in reachable blobs |
| Local personal details | Current Shield instructions contain a private LAN address; historical story-debugging instructions contain a personal absolute workspace path |

Method: `git rev-list --objects --all`, `git cat-file --batch-check` and
`git cat-file --batch`, NUL-byte classification, payload-path checks, signature
searches and targeted provenance reads. Ignored game inputs, build outputs and
third-party checkouts were not recursively scanned or uploaded. Unreachable
objects, GitHub attachments/releases and arbitrary unknown secret formats are
outside this check. Commit author metadata remains in history.

Game addresses, short disassembly discussions, synthetic test fixtures and
compatibility code remain intentional source material. This content scan is not
a certification of independent authorship of every line.

## Original snapshot: release decisions and required preparation

1. **Select and document a source license before calling this open source.**
   There is currently no root license for newly authored unmarked files.
   `src/nv2a_vertex.c` already carries `GPL-2.0-only OR GPL-3.0-only`; its existing
   grant must remain intact. A straightforward candidate is GPL-3.0-only for
   project-authored source, with explicit third-party exceptions and notices.
   That is a recommendation for the owner to select, not a license applied by
   this audit. The previously discussed restrictive commercial binary license
   cannot simply override the linked GPL component. Public visibility alone
   does not give unmarked source a general reuse license; see
   [GitHub's licensing guidance](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository).
   The applicable full GPL texts are retained in `LICENSES/`.

2. **Keep the initial deliverable source-only.** Publish tools, patches,
   documentation and synthetic tests. Users supply their own supported Xbox ISO
   and build locally. Do not attach the generated `.app`, generated game C,
   extracted assets, the ISO or gameplay captures to a release. Supporting a
   user-supplied ISO does not itself grant rights to redistribute the resulting
   game. See [the existing component inventory](LICENSING.md).

3. **Preserve mixed upstream licensing and make its limits visible.**
   `THIRD_PARTY_NOTICES.md` and `LICENSES/` retain MIT, GPL, LGPL, zlib, BSD and
   reference notices. xboxrecomp's LGPL-version discrepancy and missing exact
   xemu extraction revision remain documented upstream provenance gaps; do not
   label every runtime file MIT. Preserve per-file headers and patch context.
   Keep binary redistribution out of this source release rather than claim a
   completed corresponding-source/relinking bundle. The SDL versions recorded
   in notices are observed development versions, not reproducible bootstrap pins.

4. **Make setup and app lifetime explicit.** At this snapshot the packager
   symlinks extracted assets and relies on host dylibs. A user cannot assume the
   app survives deleting its checkout or moving to another Mac. The release
   workflow should copy runtime assets/dependencies into the local app or clearly
   retain the checkout as a runtime requirement. Include retained notices in the
   locally built app. Validate an isolated source checkout with spaces in its
   path, unsupported/missing ISO failures, resume after interrupted setup, and a
   native ARM64 app dependency audit. These are root-owned packaging tasks; this
   audit performed no builds.

5. **Replace session instructions with contributor instructions.** Remove old
   allowance thresholds, deadlines and device-access authorizations from public
   `AGENTS.md` files. Use a caller-provided ADB serial in Shield examples rather
   than the developer's LAN address. Keep Android experiments under
   `android/shield2019/` and explicitly unsupported for this Mac-focused release.
   Current-file cleanup does not erase historical paths or contributor metadata.
   No credential was found that requires secret rotation or an automatic history
   rewrite; decide deliberately whether to publish the existing development
   history or a reviewed source snapshot.

6. **Describe validation accurately.** The user completed Arctic Antics on Mac
   build 61; later builds have component evidence and known visual/runtime gaps.
   Do not advertise every level, crash-free operation or universal 60 FPS.
   Output upscaling/sharpening improves presentation but does not create new
   internal scene detail. The Shield is experimental and outside this release's
   user-facing setup promise.

## Original snapshot: final publish check

After preparation, repeat the history/content scan and inspect staged paths.
Run the documented clean setup against the supported ISO locally, record the
result and app architecture/dependencies, then review the exact source archive.
License selection and an explicit publish action remain separate from preparing
this private repository. This report is an engineering inventory, not a legal
clearance opinion or a security guarantee.
