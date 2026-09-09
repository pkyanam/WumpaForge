# Contributing

WumpaForge is an experimental native Apple Silicon port. Read [AGENTS.md](AGENTS.md)
for architecture and memory rules, and [docs/STATUS.md](docs/STATUS.md) for known
limitations before changing the runtime.

Run `./setup.sh "/path/to/your/USA Xbox.iso"` to build. Setup requires your own
supported disc; there is no downloadable game payload or CI secret containing it.
Use at most two local compiler jobs. Keep all generated code, original assets,
saves, binaries, dependency checkouts and captures in ignored directories.

For setup/packaging changes, run:

```sh
python3 tools/test_setup.py
python3 tools/test_macos_paths.py
python3 tools/test_package.py
bash -n setup.sh install.sh 'Build WumpaForge.command'
```

After producing an app, run `python3 tools/test_package.py --app "build/WumpaForge.app"`.
It checks dependencies, architecture, signatures and assets without launching the
game. Runtime changes need the relevant [CPU/GPU tests](docs/TESTING.md) and a
clear account of what was physically tested. A passing synthetic test does not
establish full-game correctness or controller pairing.

Submit focused changes with the problem, resulting behavior and validation.
Do not attach game files or generated code to issues or pull requests. Preserve
source provenance and third-party notices. Project contributions are provided
under the project's GPL-3.0-only license unless an existing file's terms apply.

Android/Shield code remains experimental in `android/shield2019`; changes there
must not silently alter the validated Mac build or install anything on a device.
