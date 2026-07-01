import 'dart:convert';

import 'package:ffmpeg_kit_flutter_new/ffmpeg_kit.dart';
import 'package:ffmpeg_kit_flutter_new/ffprobe_kit.dart';
import 'package:ffmpeg_kit_flutter_new/return_code.dart';

import 'ffmpeg_executor.dart';
import 'ffmpeg_desktop.dart' show parseProbeJson;

/// Android executor: uses the bundled FFmpegKit library (maintained fork of the
/// retired arthenica FFmpegKit). Same command arguments as the desktop path.
class MobileFfmpegExecutor implements FfmpegExecutor {
  @override
  Future<VideoInfo> probe(String path) async {
    final session = await FFprobeKit.executeWithArguments(<String>[
      '-v', 'quiet',
      '-print_format', 'json',
      '-show_streams',
      '-show_format',
      path,
    ]);
    final rc = await session.getReturnCode();
    if (!ReturnCode.isSuccess(rc)) {
      throw FfmpegException('ffprobe 失败: $path');
    }
    final output = await session.getOutput() ?? '';
    final root = jsonDecode(output) as Map<String, dynamic>;
    return parseProbeJson(root, path);
  }

  @override
  Future<void> run(List<String> args) async {
    final session = await FFmpegKit.executeWithArguments(args);
    final rc = await session.getReturnCode();
    if (!ReturnCode.isSuccess(rc)) {
      final output = await session.getOutput() ?? '';
      throw FfmpegException('FFmpeg 失败:\n$output');
    }
  }
}
