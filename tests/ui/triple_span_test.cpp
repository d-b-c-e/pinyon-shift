#include "triple_span.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

using pinyon_shift::display::Rect;
using pinyon_shift::display::TripleSpan;

int checks = 0;
void check(bool result, const char* why) {
  ++checks;
  if (!result) {
    std::cerr << "FAIL: " << why << '\n';
    std::exit(1);
  }
}

int main() {
  const Rect left{-2560, 0, 2560, 1440}, centre{0, 0, 2560, 1440}, right{2560, 0, 2560, 1440};
  const Rect span{-2560, 0, 7680, 1440};
  check(TripleSpan({left, centre, right}) == span, "the owner's row spans -2560,0 7680x1440");
  std::vector<Rect> shuffled{right, left, centre};
  check(TripleSpan(shuffled) == span, "display order does not matter");
  std::reverse(shuffled.begin(), shuffled.end());
  check(TripleSpan(shuffled) == span, "reverse order does not matter");
  check(TripleSpan({left, centre, right, {0, -1440, 2560, 1440}}) == span, "a fourth display above the row is outside it");
  check(TripleSpan({{0, 0, 2560, 1440}, {2560, 0, 2560, 1440}, {5120, 0, 2560, 1440}}) == Rect{0, 0, 7680, 1440},
        "a row starting at the primary spans from 0");
  check(!TripleSpan({}), "no displays");
  check(!TripleSpan({centre}), "one display");
  check(!TripleSpan({left, centre}), "two displays");
  check(!TripleSpan({left, {0, 0, 3840, 2160}, {3840, 0, 2560, 1440}}), "an unequal centre");
  check(!TripleSpan({{-1920, 0, 1920, 1080}, {0, 0, 1920, 1080}, {1920, 0, 1920, 1080}}), "1080p panels");
  check(!TripleSpan({left, centre, {2600, 0, 2560, 1440}}), "a gap between panels");
  check(!TripleSpan({left, centre, {2560, 10, 2560, 1440}}), "tops not aligned");
  check(!TripleSpan({{0, -1440, 2560, 1440}, centre, {0, 1440, 2560, 1440}}), "a column, not a row");
  check(!TripleSpan({left, centre, right, {5120, 0, 2560, 1440}}), "four in a row is ambiguous");
  check(!TripleSpan({left, centre, right, {-2560, 1440, 2560, 1440}, {0, 1440, 2560, 1440}, {2560, 1440, 2560, 1440}}),
        "two rows of three are ambiguous");
  check(!TripleSpan({left, centre, right, {100, 100, 1920, 1080}}), "another display inside the row");
  check(!TripleSpan({left, centre, right, centre}), "a duplicated (mirrored) panel");
  std::cout << "PASS: " << checks << " triple span checks\n";
  return 0;
}
