# Actual native framebuffer captures

`WRATH_CAPTURE_FRAME=1800` with an absolute `WRATH_CAPTURE_PATH` saves one BMP.
Alternatively `WRATH_CAPTURE_FRAMES=900,1100,1800` and an absolute
`WRATH_CAPTURE_DIRECTORY` save up to16 selected frames as `frame-000900.bmp`.
Create the output directory first; do not combine the two modes. Each frame is
attempted once before its actual presentation. This reads the native GPU
backbuffer, restores pack/read state, and does not inject any scene content.

Readback affects timing: exclude capture windows from performance conclusions.
Boot43 produced all three requested files. Frames900 and1800 visibly verify the
Traveller's Tales logo and Crash title/menu after the viewport fix. Captures and
format-converted PNGs stay ignored under local/reports.

`tools/boot.py` permits bounded runs up to600seconds so the complete six-minute
backstory and the title's original attract transition can be observed. A debugger exit0 is never proof of success;
inspect its stop and actual game state.

For interactive UI testing, run `python3 tools/package.py` after a successful
native build, then add `--app` to boot.py. LLDB debugs that packaged executable
under the stable `local.wrath.native` app identity; the asset directory remains
local. The ordinary unbundled diagnostic target remains the default.

Window close and macOS Quit now terminate the host process explicitly. They do
not pretend that the original game returned or release guest memory while its
workers continue running. Process teardown lets macOS reclaim all game threads,
GPU objects and audio handles. This is separate from the still-unfinished
graceful guest-return cleanup path.
