# ARM64 and static recompilation audit

2026-09-08; read-only review of the pinned toolchain, local patches and primary
architecture references. No new game build or launch during this research.

## Finding

The current route already produces native ARM64 instructions for this Xbox game's
CPU logic. The difficult remaining work is preserving the original behavior at
instruction, memory and SDK boundaries. Native compilation is one validation gate;
correct game output is another. There is no evidence yet for a dependable time
estimate to the title menu. Boot26 reaches the intro/cutscene loop but renders a
black captured frame. Research does not establish its cause.

The selected [xboxrecomp README at our pinned revision](https://github.com/sp00nznet/xboxrecomp/blob/051a128df5ec27ef14f1ceaaead11c5457321eef/README.md)
explicitly withdraws earlier playable Burnout/dashboard claims that depended on
handwritten visual scaffolding. It also describes instruction coverage as
unquantified. Treat this as an experimental translator and runtime, with local
evidence required for every milestone. Our unsupported-instruction patch stops
when reached instead of retaining upstream's silent comment fallback.

[XenonRecomp](https://github.com/hedge-dev/XenonRecomp) targets Xbox360 PowerPC,
whereas this disc contains an original Xbox x86 XBE. Its function-discovery,
configuration and instruction-test approach is useful methodology, but switching
to that translator would not translate this executable. It is not a ready option
for our architecture. No new toolchain was installed during this audit.

## Concrete correctness boundaries

| Boundary | Local evidence | Remaining concern / useful response |
| --- | --- | --- |
| Native CPU execution | CMake targets arm64; generated C is compiled into the native executable; original startup, loaders, callbacks and intro call stack execute. | Reaching a routine does not prove all flags, floating-point or indirect-call behavior. Preserve stop-on-unsupported diagnostics and original disassembly as authority. |
| Guest ABI versus host ABI | Guest addresses wrap to uint32_t in XBOX_PTR; SDK bridges decode guest stack/registers. The fixed NtFreeVirtualMemory bug demonstrated actual corruption from host-sized output writes. | Audit each reached boundary for explicit DWORDs, argument widths, return stack adjustment and object layout. Do not cast guest objects to host pointer-bearing structures or pass guest varargs to native libc. |
| Floating point | The template declares a TLS double[8] x87 stack. Local FIST/FISTP fixes honor guest rounding control for integer conversion. FLDCW stores the word. | Storing a control word does not reproduce all arithmetic rounding, precision or exceptions. Compare suspicious transforms/animation operations against numeric fixtures and original instructions before changing behavior. |
| Concurrent guest memory | MEM8/16/32/F/D use volatile typed loads/stores. Generated memory xadd/cmpxchg use atomic helpers; native worker completion uses protected host state. | Plain shared guest accesses still need a defensible concurrency model. An atomic RMW elsewhere does not automatically make every volatile racing access valid. Audit actual shared fields and synchronization; indiscriminate barriers or atomics are not a demonstrated fix. |
| Address space | A sparse4GB host reservation represents guest32-bit addresses; retail RAM/aliases and bounded heap are separate. Main/worker stacks and TLS are distinct. | This is not a complete Xbox virtual-memory implementation. Preserve exact alias/object semantics for reached APIs, and distinguish address-space size from resident RAM. |
| Graphics and SDK | Original matrix cache writes and SDK4361 offsets were verified against the binary; native shader/index/immediate tests pass. | Replaced SDK entry points must reproduce reached state, not merely return success. Fixed texture combiners/sampling remain incomplete and can change full-frame output. See graphics audit. |

[Apple's ARM64 ABI](https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms)
documents64-bit pointers, long and size_t; long double equals double on this
platform. Thus changing our x87 array to long double would not add extended
precision. Apple also specifies different variadic argument handling. Compiling
well-typed host C lets Clang handle the native calling convention; guest register
and stack decoding remains our explicit compatibility responsibility.

[Apple's architectural differences guide](https://developer.apple.com/documentation/apple-silicon/addressing-architectural-differences-in-your-macos-code)
calls for synchronization of shared data on ARM64 and conversion of Mach ticks
using timebase information. The [LLVM concurrency guide](https://llvm.org/docs/Atomics.html)
distinguishes volatile accesses from atomic synchronization. These support a
targeted concurrency audit, not a claim that an observed black frame is a race.
The startup audit independently finds that the relevant fade is per-loop arithmetic.

[XboxDev's memory map](https://xboxdevwiki.net/index.php?title=Memory&oldid=7416)
describes64MB retail memory shared by CPU/GPU and separate hardware mappings.
That physical map alone does not justify flattening every guest virtual address
or treating all graphics memory writes as already synchronized host texture data.

## Decision after research

Continue native AOT plus precise SDK compatibility, using other decomps as
semantic references. First capture intro state and native rendering state at the
same point. If fade is zero and animation advances, inspect geometry, textures,
combiners, fragment tests and target presentation; if progression is stopped,
inspect the documented animation/audio gates and generated arithmetic. Neither
branch requires a speculative engine rewrite.

Fix a reproduced contract and verify its real output before extending unrelated
features. Keep the two-job compile limit and bounded diagnostics. Defer broad
performance work until measured; repeated P8 uploads are a specific candidate,
but invalidation must still detect original CPU writes. Preserve native controller
mapping work and test real Bluetooth devices once a usable menu is present.

Source snapshots retrieved for Apple markdown are ignored in
local/reports/research/apple-arm64.md and apple-architecture.md. Local inspected
files: templates/runtime/recomp_types.h, tools/recomp/lifter.py, CMakeLists.txt,
src/timing_bridge.c and the existing runtime/graphics patches. No downloaded
game assets or source imports were added by this research.
