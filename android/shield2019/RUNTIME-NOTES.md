# Android runtime adaptation

`patches/runtime-platform.patch` applies **after the repository's normal pinned
xboxrecomp patches**, to a separate dependency copy. It does not modify the
macOS checkout. The initial target is SHIELD TV Pro 2019 (`mdarcy`), ARM64,
Android API 30 or newer. It is a source adaptation, not a device validation.

## Changes

- `src/platform/win32_compat.c`: Android `TerminateThread` returns `FALSE` and
  `ERROR_CALL_NOT_IMPLEMENTED`. Bionic does not implement `pthread_cancel`.
  A source search found no calls to `TerminateThread` in the current runtime or
  game bridges; guest self-termination still uses the existing `ExitThread`.
  Failure does not falsely mark a running thread exited or signal its handle.
  If a future caller needs forced termination, it needs a separately designed
  cooperative lifecycle; a signal-handler `pthread_exit` is not a safe substitute.
- The same file clears `SecureZeroMemory` through volatile byte stores on Android.
  This avoids relying on a non-public `explicit_bzero` or API 34's
  `memset_explicit` while preserving observable stores at API 30.
- `src/kernel/xbox_memory_layout.c`: Android uses the existing owned, sparse
  4 GiB guest-address reservation and its matching teardown. Only owned ranges
  permit destructive fixed mappings, while RAM mirrors keep shared backing.
  This costs virtual address space, not 4 GiB of resident RAM. Android's existing
  `memfd_create` path is retained; the API is available at API 30.

The platform availability evidence is the official
[Bionic status document](https://android.googlesource.com/platform/bionic/+/refs/heads/android11-dev/docs/status.md)
and its [current API additions](https://android.googlesource.com/platform/bionic/+/master/docs/status.md).

## Remaining integration boundaries

- The kernel CMake target requests OpenSSL although the current kernel C sources
  contain no OpenSSL includes or calls. Android build integration can remove that
  dependency; a complete link with undefined symbols rejected must confirm it.
- `src/main.c` expects ordinary filesystem paths and derives writable `saves` and
  `run` directories next to assets. An Android launcher must supply a real readable
  extracted-assets directory and writable app storage; a `content://` URI is not
  a POSIX pathname. Keep original assets out of the APK/repository.
- `graphics.c` and `input_bridge.c` enforce the event-thread identity only on
  Apple. Android needs the SDL event-thread identity recorded explicitly. Worker
  loading-screen rendering must retain serialized graphics/context ownership.
- Existing C TLS and native `setjmp`/`longjmp` are structurally portable; no raw
  host `jmp_buf` is serialized to guest memory. This audit found no signal handler
  that requires a Linux/Android register-layout conversion. The pre-existing
  vectored-exception helper is still unimplemented, not newly validated here.
- Repeat the memory alias/protection probe inside the actual application sandbox.
  Successful compilation or an ADB-shell probe cannot establish app SELinux,
  address-space, lifecycle, GPU or whole-game behavior.

## Patch validation

On September 9, 2026, the patch was applied to fresh temporary copies of its two
input files using `git apply --check` and `git apply`. Result bytes matched the
intended edits; reverse application checked successfully; the original checkout
was verified unchanged. No compiler, device test, download or game launch was
performed by this patch task. Root build integration owns NDK compilation and
further validation.

## TV remote versus player-one controller

`runtime-input-tv.patch` is enabled only by `WRATH_ANDROID_TV`. The existing SDL
backend opens mapped controllers in enumeration order; a mapped TV remote can
therefore occupy Xbox port 0 before the player's Bluetooth controller. After
normal refresh, the TV policy promotes the first already-open controller with
both left-stick axes **or** A, B and Start if port 0 lacks those capabilities.
It uses SDL mapping capabilities, not vendor/product names. An existing eligible
port-0 pad remains in place, so adding another gamepad never steals player one.
Other devices stay open and usable on their logical ports. The existing four
open-controller limit remains; this does not evict devices to accommodate a
fifth device when all four slots are occupied.

Promotion swaps handles, explicitly stops both physical vibration effects,
clears cached input/vibration, and increments each affected port's own packet
counter. No cached buttons or rumble commands transfer to a different device.
New state is sampled from the promoted device. Remote-only input remains available
under the original enumeration behavior.

The actual SDL backend regression is reproducible without game assets:

```sh
python3 android/shield2019/tests/test_input_tv.py
```

It applies the patch to a temporary source copy and compiles one small UBSan
fixture against installed SDL2. The synthetic TV remote is followed by unknown
analog and digital gamepads; tests check promotion, non-reordering of a second
real pad, disconnect/reconnect, packet stability, old-button release and physical
rumble routing. It passed on the development Mac. `--baseline` intentionally
omits the patch and fails at the first gamepad-to-port-0 assertion (exit 134).
The fake rumble callback accommodates the existing sdl2-compat 2.32.70 callback
ABI issue, as the repository's earlier virtual-controller test does. Physical
Android Bluetooth mappings and pairing behavior remain untested.
