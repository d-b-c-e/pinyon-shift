#pragma once

#include <optional>
#include <vector>

namespace pinyon_shift::display {

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
  bool operator==(const Rect&) const = default;
};

// Separate-monitor triples (pinyon_shift_triple_span): the one window rectangle over a single row of three equal,
// adjacent 2560x1440 displays, left to right with their tops aligned. No such row, two candidate rows (four in a row
// included) or another display inside the row gives none, and the caller keeps its usual window.
inline std::optional<Rect> TripleSpan(const std::vector<Rect>& displays) {
  constexpr int kWidth = 2560, kHeight = 1440;
  const auto panel_at = [&](int x, int y) {
    int matches = 0;
    for (const Rect& d : displays) matches += d == Rect{x, y, kWidth, kHeight};
    return matches == 1;
  };
  std::optional<Rect> span;
  for (const Rect& left : displays) {
    if (left != Rect{left.x, left.y, kWidth, kHeight} || !panel_at(left.x, left.y) ||
        !panel_at(left.x + kWidth, left.y) || !panel_at(left.x + 2 * kWidth, left.y)) {
      continue;
    }
    if (span) return std::nullopt;
    span = Rect{left.x, left.y, 3 * kWidth, kHeight};
  }
  if (!span) return std::nullopt;
  int inside = 0;
  for (const Rect& d : displays) {
    inside += d.x < span->x + span->w && span->x < d.x + d.w && d.y < span->y + span->h && span->y < d.y + d.h;
  }
  return inside == 3 ? span : std::nullopt;
}

}  // namespace pinyon_shift::display
