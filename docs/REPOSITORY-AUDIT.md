# Repository staging audit — 2026-09-08

Read-only inspection of the tracked worktree and all Git objects reachable from
local refs at checkpoint `75c0234`. This report itself is the only audit change.
No ISO, game process, build, dependency checkout, or Git history was modified.

## Result

No original ISO, extracted game assets, compiled executable, generated game-code
file, dependency checkout, or other binary blob was found in reachable history.
No common credential signatures were found in tracked files, historical blobs,
or commit messages. The current repository is suitable for the requested private
source staging with the qualifications below; this is a content audit, not a
guarantee that every possible secret format was detected.

| Check | Evidence |
| --- | --- |
| Reachable history | 119 commits; 453 unique blobs; 7,826,264 uncompressed blob bytes |
| Largest historical blob | `src/graphics.c`, 138,938 bytes; no blob exceeds 1 MiB |
| Current tracked worktree | 167 files; 1,422,218 bytes at inspection |
| Binary detection | No NUL-containing historical or current tracked files; no ISO, XBE, archive, image, audio, video, or compiled-product paths in reachable history |
| Secret checks | No common GitHub token, AWS access-key ID, private-key block, OpenAI-style key, or quoted credential-assignment signatures in historical blobs; no common credential signatures in commit messages |
| Worktree additions | No untracked, nonignored files at inspection |
| Ignored local material | Supplied ISO, `local/`, `build/`, `third_party/`, `.venv/`, and Python caches remain outside tracked history |

## Findings and precise follow-up

- **Low: personal absolute path.** `docs/research/STORY-PLAYBACK-AUDIT.md:113`
  contains a developer home/workspace path in an LLDB command, including in
  reachable history. Replace the current command with
  `command script import tools/story_probe.py`, documenting launch from the
  repository root if needed. A current-file edit does not remove old history.
  Git commit metadata also retains the contributor identity; no credential was
  found there. This audit does not recommend rewriting development history for
  a private staging repository.
- **Preventive: ignore coverage outside designated directories.** Existing rules
  correctly ignore the ISO and all intended extraction/build/dependency paths.
  They do not ignore `.env`, `.env.local`, `*.ips`, `core`, `screenshots/`, or
  compiled/game binaries placed elsewhere. Add explicit patterns for `.env`,
  `.env.*` with an exception for a deliberately safe `.env.example`, `*.xbe`,
  `*.xiso`, `*.o`, `*.a`, `*.dylib`, `*.so`, `*.app/`, `*.dSYM/`, `*.ips`,
  `*.crash`, `core`, `core.*`, `screenshots/`, and `captures/`. Keep intentionally
  authored documentation images possible rather than ignoring every PNG/JPEG.
  Add a mixed-case ISO pattern such as `*.[iI][sS][oO]`.
- **Informational: game-specific research is present.** Address maps, extracted
  numeric facts, short disassembly explanations, register encodings, and native
  compatibility implementations are intentionally tracked. The audit found no
  bulk lifted game functions or captured shader/asset payload files. These
  research facts must not be described as a game-independent engine.
- **Informational: fixtures and upstream patches.** The ADPCM header is synthetic
  data with its generator and pinned oracle attribution. GPU tests construct
  small synthetic instructions/inputs; full captured game shaders and constants
  are loaded from caller-supplied local files, rather than embedded. The tracked
  `patches/` contain source changes plus upstream context, with provenance in
  `docs/UPSTREAM.md`; retain applicable upstream notices. No dependency source
  checkout is tracked.

## Method and limits

Used `git rev-list --objects --all`, `git cat-file --batch-check`, and
`git cat-file --batch` to inspect every unique reachable blob, not only HEAD.
Inspected tracked worktree bytes, `git ls-files --others --exclude-standard`,
`git status --ignored`, and `git check-ignore --no-index` for representative
ISO, extraction, generated-code, build, dependency, environment, crash, and
screenshot paths. Manually reviewed fixture generators, optional shader-capture
loaders, runtime overrides, and upstream provenance to distinguish synthetic
numeric test vectors from game payloads.

Ignored local directories were checked for exclusion, not recursively scanned
for secrets or assets: they intentionally contain the user's game and generated
outputs and are not part of a normal Git push. Unreachable objects, reflog-only
history, remote GitHub state, and files added after this checkpoint are outside
this snapshot. Recheck staged paths and `git diff --cached --stat` immediately
before pushing; never use `git add -f` for ignored game or build material.
