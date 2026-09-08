# Extracted asset coverage and later-load failure audit

Audited after the standalone `dad-demo.log` run (2026-09-08). No game process
was launched for this audit. Every one of2267 entries in the ISO-derived
`local/reports/disc-inventory.json` exists under `local/assets` with its exact
recorded size:972,298,886 bytes total, zero missing and zero size mismatches.
The initial check verified names/sizes. The original ISO remains
read-only. A subsequent root check with `tools/verify_assets.py` compared SHA-256
for every extracted file against its actual disc extent: all2267 files match,
zero mismatches. The complete ignored report is
`local/reports/asset-sha256-verification.json`; summary in verify-assets.log.
This validates extracted contents, not the game's runtime loading/rendering path.
`tools/pipeline.py prepare` obtains each filename, sector and size
from the disc's XDVDFS directory tree and checks its extent against ISO size.

Reproduce the low-cost coverage check without reading all972MB of contents:

```sh
python3 - <<'PYCODE'
import json
from pathlib import Path
entries = json.loads(Path('local/reports/disc-inventory.json').read_text())
base = Path('local/assets')
missing = [e['path'] for e in entries if not (base / e['path']).is_file()]
wrong = [e['path'] for e in entries if (base / e['path']).is_file()
         and (base / e['path']).stat().st_size != e['size']]
print(len(entries), sum(e['size'] for e in entries), missing, wrong)
assert not missing and not wrong
PYCODE
```

The run log contains nine anonymous FILE_NOT_FOUND results in three groups of
three: lines1970–1972,2854–2856,3210–3212. Earlier groups are followed by successful
DDS reads and continued execution. They therefore are not all fatal missing
assets. The final group precedes an unresolved indirect call to0, but the log
records target history, not the callsite or missing filenames. Temporal proximity
does not prove that failed file opens caused the null indirect call.

There is an exact original-Xbox candidate for optional three-file probing:
`86429..864BA` formats `%s.obj`, `%s.anm`, `%s.gra` using LevelFileName at561740,
then calls file-existence routine978A0. Conditional branches8644C,86483,864BA
explicitly skip the associated loaders when absent. This agrees with the
same semantic sequence in the locally pinned OpenCrashWOC `InitWorld`
(`code/src/gamecode/main.c`). It is evidence that such misses can be expected,
not identification of these anonymous log entries. Related original globals
from that same audited sequence: Demo23B750; Level19C068; LevelFileName561740.
Reading those globals at the failure distinguishes attract/demo loading from
another transition without guessing from the user-facing launcher name.

## Bounded failed-open diagnostics

The existing NtCreateFile failed-result line now includes the already translated
thread-local host path, guest return address saved by dispatch in
`g_xbox_kernel_caller`,
and guest ESP. At most64 failed-open details are printed per process followed
by one suppression notice. Successful calls and original return/register/stack
behavior are unchanged. `kernel_bridge.c` passes the existing native target's
C syntax check (eight pre-existing warnings). The cumulative
`patches/xboxrecomp-runtime.patch` includes this change and retains all earlier
runtime work.

Parent should break at `recomp_icall_not_code_log` and capture the native backtrace,
then read guest ESP and its first stack words using g_xbox_mem_offset. A generated
caller frame/source line identifies the exact original CALL instruction and object
whose function pointer became0. Also capture original globals listed above and
match each newly named failed path against the ISO inventory. Do not replace an
absent asset or bypass a null function call before this evidence exists.

If broad tracing is needed instead, existing `XBOX_LOG_LEVEL=4` records guest-to-host
path translations and failed opens in `local/run/xbox_kernel.log`. This is noisier
than the bounded failure diagnostics and the file is replaced by each new run.


## Boots43–44: optional misses identified; mesh failure isolated

The nine named failed opens are three attempts each for:

- `Crashdat/levels/b/hub/hubcubemap.dds`
- `Crashdat/levels/a/snow_m/snowcubemap.dds`
- `Crashdat/stuff/xloading.nux`

All are absent from the original disc inventory. This is expected: original
NuFileExists978A0 retries fopen three times (97916 increments the attempt counter,
97917 compares3). Cubemap helper1DEA0 explicitly returnsNULL at1DF00 when its
existence check fails. Scene loaderA9F60 tries a name ending `.nux`, then restores
the caller's original `.nus` name atA9F9F..A9FA5 when the alternate is absent.
The supplied `stuff/xloading.nus` is present and SHA-256 verified. The earlier
three-extension example above was a hypothesis; these actual filenames supersede
it for the observed log entries. No placeholder assets were created.

Boot43 stops at original3B460 during scene mesh conversion, with Level7 and Demo1.
The call through D3DX mesh vtable+38 resolves to0. Boot44's expanded shader probe
captured the entire1000-entry mesh registry: used_count0, free_head0,
used_headFFFFFFFF, no published mesh pointers. Original vtables169EB0/169F20
remain intact and both have nonzero111140 at+38 and11115D at+40. LLDB could not
resolve the guest TLS register globals; the JSON marks them unavailable rather
than substituting zeros. The registry capture does not need TLS access.

The reached D3DX initializer111CC7 passes format65 to CreateIndexBuffer100D00.
Our bridge incorrectly required2C, so construction failed before publishing the
first mesh; the game then used handle0. Retail100D00 ignores format/usage/pool
entirely. The corrected bridge preserves that behavior, as documented/tested in
`INDEXED-DRAWS.md`. A focused native GPU smoke test draws the same verified quad
using format65 and arbitrary ignored usage/pool, with all existing smoke checks
passing (`local/reports/mesh-index-smoke.log`). Actual game progression after the
fix still needs its next run.

The first diagnostic revision mislabeled arg0 as guest_return because dispatch
had already popped the return. It is corrected to the existing saved TLS caller;
boot43/44 old guest_return values should not be used as callsite evidence.
