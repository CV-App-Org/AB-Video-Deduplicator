# AB Video Deduplicator — C++ / Qt5 edition

A standalone native re-implementation of the Python/PyQt5 app in the repository root,
written in **C++ with Qt5 Widgets**. All video work is delegated to the external
**ffmpeg / ffprobe** binaries (on Windows: `ffmpeg.exe` / `ffprobe.exe`), exactly like the
Python version — no OpenCV dependency.

It blends frames from video A (content) and video B (material) into a high-frame-rate
output to change the file's data fingerprint, with three strengths:

| Strength | Target FPS | A:B ratio |
| --- | :---: | :---: |
| 50%   | 60  | 1 : 1 |
| 75%   | 120 | 1 : 3 |
| 87.5% | 240 | 1 : 7 |

The frame-insertion algorithm, ffmpeg command lines and audio muxing mirror the Python
implementation 1:1 — given the same inputs it produces a byte-identical output file.

## Requirements

- A C++17 compiler and CMake (>= 3.16)
- Qt5 Widgets (`qtbase5-dev` on Debian/Ubuntu)
- `ffmpeg` and `ffprobe` on `PATH` (or placed next to the built executable; on Windows that
  means shipping `ffmpeg.exe` / `ffprobe.exe` alongside `ab_dedup.exe`)

## Build

```bash
cmake -S . -B build              # add -DCMAKE_CXX_COMPILER=g++ if the default cc is clang without libstdc++
cmake --build build -j
```

The executable is `build/ab_dedup`.

## Run

GUI (needs a display):

```bash
./build/ab_dedup
```

Headless / scripting (also used for automated testing):

```bash
./build/ab_dedup --cli <videoA> <videoB> <output.mp4> <fps:60|120|240> [--gpu]
```

`--gpu` switches the encoder to NVIDIA NVENC (`h264_nvenc`); the default is CPU `libx264`.

## Layout

- `src/VideoProcessor.{h,cpp}` — core engine (`QThread`): ffprobe info, optional B resize,
  raw-frame read via ffmpeg pipes, A/B frame blending, encode + audio mux.
- `src/MainWindow.{h,cpp}` — Qt5 GUI mirroring the Python layout/styling.
- `src/main.cpp` — entry point; GUI by default, `--cli` for headless runs.
