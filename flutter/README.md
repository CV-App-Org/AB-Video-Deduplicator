# AB Video Deduplicator — Flutter edition

A cross-platform (Android / Windows / Linux) Flutter re-implementation of the
Python/C++ apps in this repo. Same feature: blend frames from video A (content)
and video B (material) into a high-frame-rate output that changes the file's data
fingerprint, keeping A's audio.

The frame-insertion algorithm is a pure-Dart 1:1 port (`getAPositions`). Actual
decode/encode/mux is delegated to **ffmpeg**, using **PNG frame sequences** as the
intermediate representation.

## Codec strategy per platform

| Platform | ffmpeg provider |
| --- | --- |
| Linux / Windows / macOS | System `ffmpeg`/`ffprobe` on PATH, via `Process` (nothing bundled) |
| Android | Bundled `ffmpeg_kit_flutter_new` library (maintained fork of the retired FFmpegKit) |

The pipeline (`lib/core/dedup_pipeline.dart`) is platform-agnostic; only the
`FfmpegExecutor` implementation differs (`ffmpeg_desktop.dart` vs
`ffmpeg_mobile.dart`, chosen by `ffmpeg_factory.dart`).

Pipeline: probe A/B → (resize B to A if needed) → decode A & B to PNG frames →
assemble target PNG sequence per `getAPositions` (A frames at computed slots, B
cycled) → encode with libx264 (or `h264_nvenc` if GPU) → mux A's audio.

## Requirements

- Flutter SDK 3.x
- **Desktop only:** system `ffmpeg` + `ffprobe` on PATH.
- Linux desktop build also needs: `ninja-build`, `libgtk-3-dev`, clang with a
  usable libstdc++ (`libstdc++-14-dev` on Ubuntu 24.04); file dialogs use `zenity`.

## Build & run

```bash
flutter pub get

# Desktop
flutter run -d linux         # or -d windows
flutter build linux          # bundle at build/linux/x64/release/bundle/

# Android
flutter build apk --release  # build/app/outputs/flutter-apk/app-release.apk
```

## Test

```bash
flutter test                                   # pure-Dart unit tests (getAPositions)
dart run tool/cli.dart A.mp4 B.mp4 out.mp4 60  # headless end-to-end (desktop)
```

## Get the APK

Two ways:

1. **CI release (recommended):** after merging, push a tag:
   ```bash
   git tag v0.1.0 && git push origin v0.1.0
   ```
   `.github/workflows/flutter-android.yml` builds the APK and attaches it to a
   GitHub Release for that tag. Requires the repo's Actions "Workflow
   permissions" to allow write (the workflow already declares `contents: write`).
   You can also trigger it manually (Actions → Build Android APK → Run workflow),
   which uploads the APK as a workflow artifact.
2. **Direct download:** a pre-built `app-release.apk` produced during development
   is attached to the PR / cloud-agent artifacts.

## Notes / limitations

- PNG intermediate frames trade disk for simplicity; long/high-res/high-fps
  inputs create many frames. Fine for typical clips; streaming is a future
  optimization.
- Desktop is not fully self-contained by design (system ffmpeg required).
- GPU (`h264_nvenc`) applies to desktop with an NVIDIA setup; ignore on mobile.
