# Native audio output — 2026-09-08

`patches/xboxrecomp-audio.patch` replaces the POSIX `xa2_*` output stubs with
SDL2's native audio device backend (CoreAudio on this Mac). Windows retains its
existing XAudio2/waveOut implementation. SDL2 was already required by the APU
CMake target; no new dependency or whole-project build was needed.

The callback consumes signed native-endian 16-bit interleaved stereo at 48,000 Hz.
SDL converts to the hardware format when required. A fixed 12 KiB ring holds at
most 3,072 stereo frames (64 ms); a 1,024-frame preroll starts playback and the
requested callback size is 512 frames (10.67 ms). Full-ring submissions are
rejected intact, underruns become silence, and the callback neither allocates
nor logs. Lifecycle calls serialize with submissions, SDL's device lock protects
ring access, and closing waits for the callback. The backend owns one SDL audio
subsystem reference and never tears down video or input with `SDL_Quit`.

Producer correction: each APU VP step is **32** frames. Eight steps form 256
stereo frames and the existing throttle schedules that group every 5,333 µs.
The upstream XAudio2 monitor instead rendered 1,024 software frames per group,
advancing software voices four times too fast and discarding the hardware mix.
The POSIX path now emits the existing 256-frame hardware mix plus 256 software
mixer frames, then clears the accumulation buffer. Muting clears both. Shutdown
sets the exit flag and joins directly, because waiting for idle hangs while a
software voice or test tone is active.

Validation completed on this Mac:

- `build/test_audio` is a Mach-O arm64 executable. Its dummy-device test verifies
  PCM channel ordering, ring wrap, whole-buffer rejection, silence on underrun,
  input validation, repeated init/shutdown, retained external SDL ownership,
  real software-voice production rate, and shutdown while a looping voice plays.
- `build/test_audio --native` successfully opened CoreAudio at 48 kHz/stereo/S16
  with 512-frame callbacks. Over 0.185 seconds it submitted 8,960 frames and played
  8,192 (8,881 frames elapsed, plus initial producer scheduling). The producer run
  reported zero underruns/overflows. Output used silent PCM; this checks native
  device delivery, **not audible in-game audio**. The earlier deterministic ring
  test intentionally reports one underrun and one rejected submission.
- `build/test_audio_monitor` verifies preservation of hardware PCM, addition of
  software PCM, 256-frame voice advancement, mute, and accumulation clearing.

Reproduce after the platform library has been built:

```sh
clang -std=c11 -O1 -g -Ithird_party/xboxrecomp/src \
  -Ithird_party/xboxrecomp/src/apu -Ithird_party/xboxrecomp/src/nv2a \
  $(pkg-config --cflags sdl2) tools/test_audio.c \
  third_party/xboxrecomp/src/apu/apu_core.c \
  third_party/xboxrecomp/src/apu/apu_vp.c \
  third_party/xboxrecomp/src/apu/apu_dsp.c \
  build/runtime/src/platform/libplatform.a $(pkg-config --libs sdl2) \
  -o build/test_audio
./build/test_audio
./build/test_audio --native
clang -std=c11 -O1 -g -Ithird_party/xboxrecomp/src \
  -Ithird_party/xboxrecomp/src/apu -Ithird_party/xboxrecomp/src/nv2a \
  tools/test_audio_monitor.c third_party/xboxrecomp/src/apu/apu_vp.c \
  third_party/xboxrecomp/src/apu/apu_dsp.c \
  build/runtime/src/platform/libplatform.a -o build/test_audio_monitor
./build/test_audio_monitor
```

Remaining integration: the actual game must reach and program its DirectSound/APU
path. The existing DSP implementation still explicitly bypasses GP/EP effects;
this patch provides output and correct producer timing, not those effects or a
complete game-specific DirectSound implementation. No game audio has been tested.
