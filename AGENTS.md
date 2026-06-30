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

### Known pre-existing bug (not an environment issue)
On Linux + Python 3.12 the final mux step fails with `ValueError: flush of closed file` at
`src/main.py` (`writer_process.stdin.close()` immediately followed by
`writer_process.communicate()`). On POSIX, `communicate()` flushes the already-closed stdin
and raises; on Windows `communicate()` skips that flush, which is why the upstream author
never hit it. Frame blending completes (all frames are processed, progress reaches 89%) but
the output file is never written. This is application code, left unmodified during env setup.
