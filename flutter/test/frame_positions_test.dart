import 'package:ab_dedup_flutter/core/frame_positions.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('getAPositions', () {
    test('fps=60 known layout', () {
      // m<=2 -> m; else 2 + 2*(m-2)
      expect(getAPositions(60, 5), <int>{0, 1, 2, 4, 6});
      expect(getAPositions(60, 1), <int>{0});
      expect(getAPositions(60, 3), <int>{0, 1, 2});
    });

    test('fps=120 known layout', () {
      // m<=1 -> m; else 1 + 4*(m-1)
      expect(getAPositions(120, 4), <int>{0, 1, 5, 9});
      expect(getAPositions(120, 2), <int>{0, 1});
    });

    test('fps=240 known layout', () {
      // {0,1} then +8,+9,+7 repeating
      expect(getAPositions(240, 5), <int>{0, 1, 9, 18, 25});
      expect(getAPositions(240, 2), <int>{0, 1});
      expect(getAPositions(240, 0), <int>{});
    });

    test('every A frame gets a distinct slot (no collisions)', () {
      for (final fps in <int>[60, 120, 240]) {
        for (final nA in <int>[1, 10, 50, 137]) {
          expect(getAPositions(fps, nA).length, nA,
              reason: 'fps=$fps nA=$nA should map to nA unique positions');
        }
      }
    });

    test('unsupported fps throws', () {
      expect(() => getAPositions(30, 10), throwsArgumentError);
    });
  });
}
