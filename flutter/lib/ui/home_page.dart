import 'dart:io';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:gal/gal.dart';
import 'package:path/path.dart' as p;
import 'package:path_provider/path_provider.dart';

import '../core/dedup_pipeline.dart';
import '../core/ffmpeg_factory.dart';

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  String? _videoA;
  String? _videoB;
  String? _output;
  int _fps = 60;
  bool _useGpu = false;
  bool _running = false;
  double _progress = 0;
  final List<String> _logs = <String>[];
  final ScrollController _logScroll = ScrollController();

  // On Android the output is auto-saved to the gallery (scoped storage), so a
  // user-picked output path is not required; on desktop it is.
  bool get _ready =>
      _videoA != null && _videoB != null && (Platform.isAndroid || _output != null);

  Future<void> _pickVideo(bool isA) async {
    final res = await FilePicker.platform.pickFiles(type: FileType.video);
    if (res != null && res.files.single.path != null) {
      setState(() {
        if (isA) {
          _videoA = res.files.single.path;
        } else {
          _videoB = res.files.single.path;
        }
      });
    }
  }

  Future<void> _pickOutput() async {
    final path = await FilePicker.platform.saveFile(
      dialogTitle: '选择输出路径',
      fileName: 'C.mp4',
      type: FileType.custom,
      allowedExtensions: <String>['mp4'],
    );
    if (path != null) {
      setState(() => _output = path.endsWith('.mp4') ? path : '$path.mp4');
    }
  }

  void _log(String message) {
    setState(() => _logs.add('• $message'));
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (_logScroll.hasClients) {
        _logScroll.jumpTo(_logScroll.position.maxScrollExtent);
      }
    });
  }

  Future<void> _run() async {
    setState(() {
      _running = true;
      _progress = 0;
      _logs.clear();
    });
    _log(_useGpu ? '已启用GPU加速模式。' : '使用CPU模式处理。');
    try {
      final baseTemp = await getTemporaryDirectory();
      final tempDir = p.join(baseTemp.path, 'ab_dedup_flutter');
      final String outPath = Platform.isAndroid
          ? p.join(baseTemp.path,
              'C_${DateTime.now().millisecondsSinceEpoch}.mp4')
          : _output!;
      final pipeline = DedupPipeline(createFfmpegExecutor());
      await pipeline.process(
        videoA: _videoA!,
        videoB: _videoB!,
        output: outPath,
        fps: _fps,
        useGpu: _useGpu,
        tempDir: tempDir,
        onProgress: (v) => setState(() => _progress = v / 100.0),
        onStatus: _log,
      );
      if (Platform.isAndroid) {
        if (!await Gal.hasAccess(toAlbum: true)) {
          await Gal.requestAccess(toAlbum: true);
        }
        await Gal.putVideo(outPath, album: 'ABDedup');
        _log('已保存到系统相册（相簿：ABDedup）');
      }
      _log('处理完成！');
    } catch (e) {
      _log('❌ 错误：$e');
    } finally {
      setState(() => _running = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('AB视频去重工具 (Flutter 版)'),
        backgroundColor: const Color(0xFF1e1e2f),
      ),
      body: Container(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
            colors: <Color>[Color(0xFF1e1e2f), Color(0xFF141422)],
          ),
        ),
        child: Padding(
          padding: const EdgeInsets.all(20),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: <Widget>[
              _pathRow('视频A路径（搬运）', _videoA, () => _pickVideo(true)),
              const SizedBox(height: 12),
              _pathRow('视频B路径（原创）', _videoB, () => _pickVideo(false)),
              const SizedBox(height: 12),
              if (Platform.isAndroid)
                _card(
                  child: Row(
                    children: const <Widget>[
                      SizedBox(
                        width: 130,
                        child: Text('输出路径',
                            style: TextStyle(fontWeight: FontWeight.w600)),
                      ),
                      Expanded(
                        child: Text('处理完成后自动保存到系统相册（相簿：ABDedup）',
                            style: TextStyle(color: Color(0xFFe0e0e0))),
                      ),
                    ],
                  ),
                )
              else
                _pathRow('输出路径', _output, _pickOutput),
              const SizedBox(height: 16),
              _card(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: <Widget>[
                    const Text('去重强度',
                        style: TextStyle(fontWeight: FontWeight.w600)),
                    RadioGroup<int>(
                      groupValue: _fps,
                      onChanged: (v) {
                        if (!_running) setState(() => _fps = v ?? 60);
                      },
                      child: Row(
                        children: <Widget>[
                          _fpsRadio('去重率50%', 60),
                          _fpsRadio('75%', 120),
                          _fpsRadio('87.5%', 240),
                        ],
                      ),
                    ),
                    CheckboxListTile(
                      contentPadding: EdgeInsets.zero,
                      controlAffinity: ListTileControlAffinity.leading,
                      title: const Text('启用GPU加速（需要NVIDIA显卡和驱动）'),
                      value: _useGpu,
                      onChanged: _running
                          ? null
                          : (v) => setState(() => _useGpu = v ?? false),
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 16),
              Center(
                child: FilledButton(
                  style: FilledButton.styleFrom(
                    backgroundColor: const Color(0xFFff7e5f),
                    padding:
                        const EdgeInsets.symmetric(horizontal: 40, vertical: 16),
                  ),
                  onPressed: (_ready && !_running) ? _run : null,
                  child: const Text('运行',
                      style: TextStyle(
                          fontSize: 16, fontWeight: FontWeight.bold)),
                ),
              ),
              const SizedBox(height: 16),
              LinearProgressIndicator(value: _running ? _progress : _progress),
              const SizedBox(height: 12),
              Expanded(
                child: _card(
                  child: ListView.builder(
                    controller: _logScroll,
                    itemCount: _logs.length,
                    itemBuilder: (_, i) => Text(
                      _logs[i],
                      style: const TextStyle(
                          fontSize: 12, color: Color(0xFFd0d0d0)),
                    ),
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _pathRow(String title, String? value, VoidCallback onPick) {
    return _card(
      child: Row(
        children: <Widget>[
          SizedBox(
            width: 130,
            child: Text(title,
                style: const TextStyle(fontWeight: FontWeight.w600)),
          ),
          Expanded(
            child: Text(
              value ?? '未选择',
              overflow: TextOverflow.ellipsis,
              style: const TextStyle(color: Color(0xFFe0e0e0)),
            ),
          ),
          const SizedBox(width: 8),
          FilledButton(
            style: FilledButton.styleFrom(
                backgroundColor: const Color(0xFF357abd)),
            onPressed: _running ? null : onPick,
            child: const Text('选择'),
          ),
        ],
      ),
    );
  }

  Widget _fpsRadio(String label, int value) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: <Widget>[
        Radio<int>(value: value),
        Text(label),
        const SizedBox(width: 8),
      ],
    );
  }

  Widget _card({required Widget child}) {
    return Container(
      padding: const EdgeInsets.all(15),
      decoration: BoxDecoration(
        color: const Color(0xE6282841),
        borderRadius: BorderRadius.circular(10),
      ),
      child: child,
    );
  }

  @override
  void dispose() {
    _logScroll.dispose();
    super.dispose();
  }
}
