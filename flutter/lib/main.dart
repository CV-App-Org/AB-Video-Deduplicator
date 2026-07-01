import 'package:flutter/material.dart';

import 'ui/home_page.dart';

void main() {
  runApp(const AbDedupApp());
}

class AbDedupApp extends StatelessWidget {
  const AbDedupApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'AB视频去重工具',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        brightness: Brightness.dark,
        useMaterial3: true,
        scaffoldBackgroundColor: const Color(0xFF141422),
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xFF4a90e2),
          brightness: Brightness.dark,
        ),
      ),
      home: const HomePage(),
    );
  }
}
