import 'dart:io';

import 'package:ab_dedup_flutter/core/dedup_pipeline.dart';
import 'package:ab_dedup_flutter/core/ffmpeg_desktop.dart';

/// Headless end-to-end runner for the desktop pipeline (no Flutter/GUI, no
/// FFmpegKit). Used for automated testing on Linux/Windows/macOS.
///
///   `dart run tool/cli.dart videoA videoB output.mp4 fps [--gpu]`
Future<void> main(List<String> args) async {
  if (args.length < 4) {
    stderr.writeln(
        '用法: dart run tool/cli.dart <A> <B> <out> <fps:60|120|240> [--gpu]');
    exit(2);
  }
  final a = args[0];
  final b = args[1];
  final out = args[2];
  final fps = int.parse(args[3]);
  final gpu = args.contains('--gpu');

  final tempDir = Directory.systemTemp.createTempSync('ab_dedup_cli').path;
  final pipeline = DedupPipeline(const DesktopFfmpegExecutor());
  try {
    await pipeline.process(
      videoA: a,
      videoB: b,
      output: out,
      fps: fps,
      useGpu: gpu,
      tempDir: tempDir,
      onStatus: (m) => stdout.writeln('STATUS: $m'),
      onProgress: (v) => stdout.writeln('PROGRESS: $v'),
    );
    stdout.writeln('DONE');
  } catch (e) {
    stderr.writeln('ERROR: $e');
    exit(1);
  }
}
