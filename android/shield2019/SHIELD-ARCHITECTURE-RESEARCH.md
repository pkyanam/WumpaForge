# Shield Pro 2019 performance research — September 9

[NVIDIA product specifications](https://www.nvidia.com/en-us/shield/shield-tv-pro/)
identify Tegra X1+, a 256-core GPU and 3 GB RAM. The actual mdarcy device reports
Android 11 and desktop GL 4.1 NVIDIA 495.00. Its internal scene render remains 640x480;
the current EGL drawable is 1920x1080. This is not a 4K internal rendering problem.

[NVIDIA Tegra graphics guidance](https://docs.nvidia.com/jetson/l4t/Tegra%20Linux%20Driver%20Package%20Development%20Guide/graphics_opengl.html)
recommends reducing repeated state changes and queries, avoiding buffer allocation
in rendering loops and measuring both timing and output. Those recommendations
fit observed CPU/driver costs. Applied bounded changes include shader constants,
exact adjacent scalar setter deduplication and a capped indexed-expansion scratch
buffer. The guidance is for Tegra/OpenGL ES, not proof of speedups on this desktop GL
Android runtime. Full pixel tests passed after the latest two changes.

[NVIDIA driver documentation](https://download.nvidia.com/XFree86/Linux-x86_64/415.18/README/openglenvvariables.html)
describes `__GL_THREADED_OPTIMIZATIONS=1` as forcing driver worker-thread work and
warns that synchronous queries may reduce performance. This documentation targets
desktop Linux and includes Xlib requirements; Android support is not established.
The installed `/vendor/lib64/libglcore.so` contains that exact option name and
`GLThreadedOptimizations`, which justifies an opt-in experiment, not a capability
claim. App-external `driver-threading` sets the option before GL initialization.
The full GPU suite passes with the option; game comparison remains necessary.

Do not root/flash the Shield, change SELinux, or treatdesktop Linux/CUDAhelper SDKs as
Android-compatible. Existing non-root ADB simpleperf and NDK LLDB are sufficient to
measure current CPU hotspots. Allocation and driver-thread experiments preserve
native ARM64 game code and the actual original game state, with no CPU emulation.

Loading observation: user reached the hub, moved, and selected a level, but the green loading
portal never completed. This does not establish successful level play. Separately,
the captured hub transition crash points to an original heap null free-list link; avoid
replacing state-block or force-unlocking code to hide it.

## Driver-threading experiment result

Same experimental APK, marker on/off: lighter windows51.72/52.49FPS versus
51.38/52.93FPS; heavier comparable window21.04FPS both. Earlymaxload10.04s versus
9.93s. Windows differ slightly inanimation position; no demonstrated benefit.
Removed the marker and experimental host option; do not ship it as an optimization.
Logs: threaded-on-game.log and threaded-off-game.log, under ignoredlocalreports.
Whether Android actually enabled a driver worker is unproven; finding an option
string in a shared driver library is insufficient.
