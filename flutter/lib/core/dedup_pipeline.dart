import 'dart:io';

import 'package:path/path.dart' as p;

import 'ffmpeg_executor.dart';
import 'frame_positions.dart';

/// End-to-end AB deduplication pipeline using PNG frame sequences as the
/// intermediate representation. Platform-agnostic: it only talks to an
/// injected [FfmpegExecutor], so it runs unchanged on desktop and Android.
class DedupPipeline {
  DedupPipeline(this.exec);

  final FfmpegExecutor exec;

  Future<void> process({
    required String videoA,
    required String videoB,
    required String output,
    required int fps,
    required bool useGpu,
    required String tempDir,
    void Function(int progress)? onProgress,
    void Function(String message)? onStatus,
  }) async {
    final sw = Stopwatch()..start();
    String t() => (sw.elapsedMilliseconds / 1000).toStringAsFixed(2);
    void status(String m) => onStatus?.call(m);
    void prog(int v) => onProgress?.call(v);

    final aDir = Directory(p.join(tempDir, 'a'));
    final bDir = Directory(p.join(tempDir, 'b'));
    final cDir = Directory(p.join(tempDir, 'c'));
    final tempB = p.join(tempDir, 'resized_b.mp4');
    final tempOut = p.join(tempDir, 'temp_output.mp4');

    try {
      for (final d in <Directory>[aDir, bDir, cDir]) {
        if (d.existsSync()) d.deleteSync(recursive: true);
        d.createSync(recursive: true);
      }

      status('开始处理，检查视频信息... (t=${t()}s)');
      prog(5);

      final a = await exec.probe(videoA);
      status('视频A信息: ${a.width}x${a.height}, '
          '${a.fps.toStringAsFixed(2)}fps, '
          '${a.duration.toStringAsFixed(2)}s, ${a.totalFrames}帧');
      final b = await exec.probe(videoB);
      status('视频B信息: ${b.width}x${b.height}');

      final encoder = useGpu ? 'h264_nvenc' : 'libx264';
      final quality = useGpu ? <String>['-preset', 'p6'] : <String>['-crf', '23'];

      String bSource = videoB;
      if (a.width != b.width || a.height != b.height) {
        status('分辨率不一致，将视频B (${b.width}x${b.height}) 调整为视频A的尺寸 '
            '(${a.width}x${a.height})... (t=${t()}s)');
        await exec.run(<String>[
          '-y', '-i', videoB,
          '-vf',
          'scale=${a.width}:${a.height}:force_original_aspect_ratio=decrease,'
              'pad=${a.width}:${a.height}:(ow-iw)/2:(oh-ih)/2',
          '-c:v', encoder, ...quality,
          '-c:a', 'aac', '-b:a', '128k',
          tempB,
        ]);
        bSource = tempB;
      } else {
        status('分辨率一致，跳过尺寸调整。');
      }
      prog(10);

      status('解码视频A为帧序列(PNG)... (t=${t()}s)');
      await exec.run(<String>['-y', '-i', videoA, p.join(aDir.path, 'frame_%05d.png')]);
      prog(30);
      status('解码视频B为帧序列(PNG)... (t=${t()}s)');
      await exec.run(<String>['-y', '-i', bSource, p.join(bDir.path, 'frame_%05d.png')]);
      prog(45);

      final aFrames = _sortedPng(aDir);
      final bFrames = _sortedPng(bDir);
      if (aFrames.isEmpty) throw FfmpegException('视频A无可用帧');
      if (bFrames.isEmpty) throw FfmpegException('视频B无可用帧');

      final nA = aFrames.length;
      final totalC = (a.duration * fps).floor();
      status('目标视频C: ${fps}fps, 时长与A一致(${a.duration.toStringAsFixed(2)}s), 总帧数: $totalC');

      final positions = getAPositions(fps, nA);

      status('组装帧序列... (t=${t()}s)');
      var aCounter = 0;
      var bIndex = 0;
      for (var i = 0; i < totalC; i++) {
        final String src;
        if (positions.contains(i) && aCounter < nA) {
          src = aFrames[aCounter];
          aCounter++;
        } else {
          src = bFrames[bIndex];
          bIndex = (bIndex + 1) % bFrames.length;
        }
        final dst =
            p.join(cDir.path, 'frame_${(i + 1).toString().padLeft(5, '0')}.png');
        File(src).copySync(dst);
        if ((i + 1) % 50 == 0 || i + 1 == totalC) {
          prog(45 + (25 * (i + 1) / totalC).round());
          status('组装帧: ${i + 1} / $totalC (t=${t()}s)');
        }
      }

      status('编码视频... (t=${t()}s)');
      await exec.run(<String>[
        '-y', '-framerate', '$fps',
        '-i', p.join(cDir.path, 'frame_%05d.png'),
        '-c:v', encoder, ...quality,
        '-pix_fmt', 'yuv420p',
        tempOut,
      ]);
      prog(90);

      if (a.hasAudio) {
        status('合并音频... (t=${t()}s)');
        await exec.run(<String>[
          '-y', '-i', tempOut, '-i', videoA,
          '-c:v', 'copy', '-c:a', 'aac', '-b:a', '128k', '-shortest',
          output,
        ]);
      } else {
        status('视频A无音频，直接输出。 (t=${t()}s)');
        await exec.run(<String>['-y', '-i', tempOut, '-c', 'copy', output]);
      }

      prog(100);
      status('视频处理完成! (总耗时: ${t()}s)');
    } finally {
      for (final d in <Directory>[aDir, bDir, cDir]) {
        if (d.existsSync()) d.deleteSync(recursive: true);
      }
      for (final f in <String>[tempB, tempOut]) {
        final ff = File(f);
        if (ff.existsSync()) ff.deleteSync();
      }
    }
  }

  List<String> _sortedPng(Directory d) {
    final files = d
        .listSync()
        .whereType<File>()
        .map((f) => f.path)
        .where((path) => path.endsWith('.png'))
        .toList()
      ..sort();
    return files;
  }
}
