# AOT arithmetic and thread-state audit — 2026-09-08

This is a source/CPU-component audit. No game launch, debugger attach, UI action
or gameplay validation was performed. It does not establish the cause of the
reported intermittent demo distortion.

## Corrected guest rounding and NaN tests

The lifter emitted host `rint` for FRNDINT although FLDCW changes only the guest
control word. For example, guest round-down of-2.5 produced-2 with host nearest,
instead of-3. FTST reported NaN as equal instead of unordered. The emitted-code
fixture in `tools/test_x87_status_rounding.py` reproduced61 mismatches before the
fix and zero afterward, across all four host modes and all four guest modes.
It also covers signed zero, ties, infinities, NaNs and unchanged stack depth.
Existing FIST/FISTP and actual D3DX color-packing tests still pass. Both CPU
executables use UBSan and float-cast-overflow instrumentation.

FRNDINT now selects the guest RC explicitly, with nearest-even independent of
host state. FTST uses the same unordered-aware comparator as FCOM. These changes
do not implement x87 extended precision, arithmetic exception delivery or full
status flags. Reference: Intel's [instruction-set manual](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-2a-manual.pdf).

The existing generated source contains nine FRNDINT sites in F488D(2), F84A0,
F88AA, F88ED(2), F89A8,105A40 and13DF84. Five FTST sites occur in F885C,F88AA(2)
and105A40(2). SDK105A40 is called by10531B/return105320;13DF84 is in DSOUND.
These are static references, not evidence that a particular live frame executes
the faulty input cases. Reports: `local/reports/x87-status-before.log`,
`x87-status-fixed.log`, `x87-fist-regression.log`, `x87-lifter-regression.log`.

## Other concrete boundaries being audited

- Three FXAM instructions in CRT F7A64/F7ACB currently emit comment-only code;
  following FNSTSW operations can read stale comparison state. Their value
  classification and sign feed the table at1B3624. The same functions contain
  three byte ROL instructions incorrectly emitted through ROL32.
- FNSAVE/FRSTOR occur in F46C8's CRT special-value/error branch, around callF8B95
  atF47DB/returnF47E0. FNCLEX appears in initialization helperF4283 andF85F7.
  Full save-state behavior is not represented by the current double stack.
  No fake80-bit save structure is an acceptable repair.
- All guest integer, x87, XMM and MMX register definitions inspected in
  `xbox_memory_layout.c` use RECOMP_TLS. Generated function bodies contain no
  static temporaries or host `long` tokens. The existing four-worker fixture
  verifies separate guest registers/TIB/TLS/stacks/heap ownership.
- Indirect-call trace counters/ring entries remain shared volatile diagnostics.
  Concurrent updates can lose trace ordering; their bounded indices do not feed
  the actual local target or transform arithmetic. Plain shared guest memory is
  still a separate concurrency-model limitation, not a proven demo race.
- The current generated inventory has99 explicit unsupported-instruction fences
  and seven comment-only FPU fallbacks. No new runtime reachability claim follows
  from that count; original instruction/call-path checks remain necessary.
