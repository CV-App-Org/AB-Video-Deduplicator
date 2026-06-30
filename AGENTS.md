# AGENTS.md

## Cursor Cloud specific instructions

This repo is a single **PyQt5 desktop application** (AB Video Deduplicator). There is no
backend/database/web service — it runs locally and shells out to the `ffmpeg`/`ffprobe`
binaries for all video work. There are no automated tests, linters, or CI config in the repo.

### Running the app
- GUI run: `DISPLAY=:1 python3 src/main.py` (a display/X server is required; the cloud VM
  exposes one at `:1`). For headless logic checks, set `QT_QPA_PLATFORM=offscreen`.
- `ffmpeg` and `ffprobe` must be on `PATH` (already installed at `/usr/bin`).
- `src/resources.py` (compiled Qt icon resources) is already committed, so `pyrcc5` is not
  needed; the app degrades gracefully with only a warning if it is missing.

### OpenCV must be the *headless* variant
`requirements.txt` lists `opencv-python`, but on Linux the GUI variant of OpenCV ships its
own Qt5 plugins and overrides `QT_QPA_PLATFORM_PLUGIN_PATH` on `import cv2`, which makes the
PyQt5 `xcb` platform plugin fail to load and the app crash on launch. This app only uses
`cv2` for video decoding (no `cv2` GUI calls), so the dev environment uses
`opencv-python-headless` instead. The startup update script enforces this; do not switch
back to `opencv-python`.

### POSIX `flush of closed file` fix
The final mux step in `src/main.py` used to call `writer_process.stdin.close()` immediately
before `writer_process.communicate()`. On POSIX + Python 3.12 `communicate()` flushes the
already-closed stdin and raises `ValueError: flush of closed file`, so the run got stuck at
89% and never wrote the output (Windows `communicate()` skips that flush, which is why the
upstream author never hit it). This branch removes the redundant `stdin.close()` and lets
`communicate()` flush/close stdin itself; the full pipeline now completes to 100% and writes
the output video on Linux.

### C++ / Qt5 edition (`cpp/`)
`cpp/` is a standalone native port (Qt5 Widgets + ffmpeg/ffprobe, no OpenCV). Build with
`cmake -S cpp -B cpp/build && cmake --build cpp/build -j`. Gotcha: the default `cc`/`c++` on
this VM is Clang without a usable `libstdc++`, so pass `-DCMAKE_CXX_COMPILER=g++` to CMake.
The binary `cpp/build/ab_dedup` runs the GUI by default, or a headless
`--cli <A> <B> <out> <fps>` mode used for automated testing. See `cpp/README.md`.
