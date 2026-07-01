# Test sample videos

Small synthetic clips used for testing the AB deduplicator (Python / C++ / Flutter).
Generated with ffmpeg (`testsrc` / `mandelbrot`), no copyright.

| File | Content | Resolution | FPS | Frames | Audio |
| --- | --- | --- | --- | --- | --- |
| `A.mp4` | content video (A) | 320x240 | 30 | 60 | none |
| `B.mp4` | material video (B) | 320x240 | 30 | 60 | none |

Example (Flutter headless / C++ CLI produce the same layout):

```bash
# C++
cpp/build/ab_dedup --cli data/A.mp4 data/B.mp4 /tmp/C.mp4 60
# Flutter (desktop, headless)
cd flutter && dart run tool/cli.dart ../data/A.mp4 ../data/B.mp4 /tmp/C.mp4 60
# Python GUI: pick data/A.mp4 and data/B.mp4
```
