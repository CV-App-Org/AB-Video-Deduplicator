/// Frame-insertion positions for video A, by target fps.
///
/// Ported 1:1 from the Python (`get_a_positions`) and C++ (`getAPositions`)
/// implementations so the three versions produce the same frame layout.
///
/// Given the number of available A frames [nA] and the target [fps]
/// (60 / 120 / 240), returns the set of indices in the output stream that
/// should be filled with an A frame; all other indices are filled with B.
Set<int> getAPositions(int fps, int nA) {
  final positions = <int>{};
  if (fps == 60) {
    for (var m = 0; m < nA; m++) {
      positions.add(m <= 2 ? m : 2 + 2 * (m - 2));
    }
  } else if (fps == 120) {
    for (var m = 0; m < nA; m++) {
      positions.add(m <= 1 ? m : 1 + 4 * (m - 1));
    }
  } else if (fps == 240) {
    if (nA == 0) return positions;
    if (nA <= 2) {
      for (var m = 0; m < nA; m++) {
        positions.add(m);
      }
      return positions;
    }
    positions.addAll(<int>[0, 1]);
    var nextPos = 1;
    const intervals = <int>[8, 9, 7];
    for (var i = 2; i < nA; i++) {
      nextPos += intervals[(i - 2) % 3];
      positions.add(nextPos);
    }
  } else {
    throw ArgumentError('不支持的帧率: $fps');
  }
  return positions;
}
