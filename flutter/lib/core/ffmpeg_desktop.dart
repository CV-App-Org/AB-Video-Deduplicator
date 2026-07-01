import 'dart:convert';
import 'dart:io';

import 'ffmpeg_executor.dart';

/// Desktop executor: uses the system `ffmpeg` / `ffprobe` binaries on PATH
/// (Linux / Windows / macOS). No binaries are bundled.
class DesktopFfmpegExecutor implements FfmpegExecutor {
  const DesktopFfmpegExecutor({this.ffmpeg = 'ffmpeg', this.ffprobe = 'ffprobe'});

  final String ffmpeg;
  final String ffprobe;

  @override
  Future<VideoInfo> probe(String path) async {
    final result = await Process.run(ffprobe, <String>[
      '-v', 'quiet',
      '-print_format', 'json',
      '-show_streams',
      '-show_format',
      path,
    ]);
    if (result.exitCode != 0) {
      throw FfmpegException('无法运行 ffprobe (请确认已安装并在 PATH 中): $path');
    }
    final Map<String, dynamic> root =
        jsonDecode(result.stdout as String) as Map<String, dynamic>;
    return _parseProbe(root, path);
  }

  @override
  Future<void> run(List<String> args) async {
    final result = await Process.run(ffmpeg, args);
    if (result.exitCode != 0) {
      throw FfmpegException(
          'FFmpeg 失败 (code ${result.exitCode}):\n${result.stderr}');
    }
  }
}

/// Shared ffprobe-JSON parsing (also used by the mobile executor).
VideoInfo _parseProbe(Map<String, dynamic> root, String path) {
  final streams = (root['streams'] as List<dynamic>? ?? <dynamic>[])
      .cast<Map<String, dynamic>>();
  Map<String, dynamic>? video;
  var hasAudio = false;
  for (final s in streams) {
    final type = s['codec_type'];
    if (type == 'video' && video == null) {
      video = s;
    } else if (type == 'audio') {
      hasAudio = true;
    }
  }
  if (video == null) {
    throw FfmpegException('未找到视频流: $path');
  }

  final width = (video['width'] as num).toInt();
  final height = (video['height'] as num).toInt();

  double fps = 0;
  final rate = (video['r_frame_rate'] as String?) ?? '0/1';
  final parts = rate.split('/');
  if (parts.length == 2) {
    final num0 = double.tryParse(parts[0]) ?? 0;
    final den = double.tryParse(parts[1]) ?? 0;
    fps = den > 0 ? num0 / den : 0;
  } else {
    fps = double.tryParse(rate) ?? 0;
  }

  double duration = double.tryParse((video['duration'] as String?) ?? '') ?? 0;
  if (duration <= 0) {
    final format = root['format'] as Map<String, dynamic>?;
    duration = double.tryParse((format?['duration'] as String?) ?? '') ?? 0;
  }

  int totalFrames = int.tryParse((video['nb_frames'] as String?) ?? '') ?? 0;
  if (totalFrames <= 0 && duration > 0 && fps > 0) {
    totalFrames = (duration * fps).floor();
  }

  if (fps <= 0 || totalFrames <= 0 || duration <= 0) {
    throw FfmpegException('视频元数据不完整或无效: $path');
  }

  return VideoInfo(
    width: width,
    height: height,
    fps: fps,
    duration: duration,
    totalFrames: totalFrames,
    hasAudio: hasAudio,
  );
}

/// Exposed so the mobile executor can reuse the same parsing.
VideoInfo parseProbeJson(Map<String, dynamic> root, String path) =>
    _parseProbe(root, path);
