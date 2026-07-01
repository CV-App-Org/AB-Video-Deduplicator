import 'dart:io';

import 'ffmpeg_executor.dart';
import 'ffmpeg_desktop.dart';
import 'ffmpeg_mobile.dart';

/// Returns the right executor for the current platform:
/// Android -> bundled FFmpegKit; everything else -> system ffmpeg via Process.
FfmpegExecutor createFfmpegExecutor() {
  if (Platform.isAndroid) {
    return MobileFfmpegExecutor();
  }
  return const DesktopFfmpegExecutor();
}
