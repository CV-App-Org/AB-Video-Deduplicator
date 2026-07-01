/// Metadata for a video stream.
class VideoInfo {
  VideoInfo({
    required this.width,
    required this.height,
    required this.fps,
    required this.duration,
    required this.totalFrames,
    required this.hasAudio,
  });

  final int width;
  final int height;
  final double fps;
  final double duration;
  final int totalFrames;
  final bool hasAudio;
}

/// Abstraction over "run an ffmpeg/ffprobe operation".
///
/// Desktop (Linux/Windows/macOS) shells out to the system `ffmpeg`/`ffprobe`
/// binaries on PATH; Android uses the bundled FFmpegKit library. The rest of
/// the pipeline is identical across platforms.
abstract class FfmpegExecutor {
  /// Probe [path] for stream metadata. Throws on failure.
  Future<VideoInfo> probe(String path);

  /// Run one ffmpeg invocation with [args]. Throws [FfmpegException] on
  /// non-zero exit.
  Future<void> run(List<String> args);
}

class FfmpegException implements Exception {
  FfmpegException(this.message);
  final String message;
  @override
  String toString() => message;
}
