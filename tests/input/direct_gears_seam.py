"""Compile production gear/projection/caps/merge code without providers or a game."""
import argparse
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--sdk", required=True, type=Path)
parser.add_argument("--cxx", required=True)
parser.add_argument("--out", required=True, type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
sdk = args.sdk.resolve()


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n}", start) + 2] + "\n"


driver = (root / "src/input/wheelkit_input_driver.cpp").read_text()
system = (sdk / "src/input/input_system.cpp").read_text()
merge = (sdk / "src/input/state_merge.cpp").read_text()
policy = (sdk / "include/rex/input/input_driver.h").read_text()
input_types = (sdk / "include/rex/input/input.h").read_text()
app = (root / "src/input/wheelkit_app_input.cpp").read_text()
native = (root / "src/input/wheelkit_native_input.cpp").read_text()
assert 'Project(*snapshot, native_direct_gears_, active)' in driver
assert 'Policy(*snapshot, native_direct_gears_, active)' in driver
assert 'policy.exclusive_axes & policy.bound_axes' in system
assert app.index('!HasDirectGearProfile(profile, plan)') < app.index('std::make_shared<WheelkitInputLifecycle>')
assert 'lifecycle_, clock_, native_direct_gears_' in app
assert 'Environment(L"PINYON_SHIFT_WHEELKIT_NATIVE_GEARS") == L"1"' in native
assert 'bool native_direct_gears{};' in (root / "src/input/wheelkit_app_input.h").read_text()
assert 'bool native_direct_gears = false' in (root / "src/input/wheelkit_input_driver.h").read_text()

harness = '''
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include "input/wheelkit_direct_gears.h"
using namespace pinyon_shift::input;
namespace sh = dbce::shim;
namespace rex::input {
// Pure fixture uses host integers, not serialized big-endian ABI validation.
template<class T> using be = T;
using X_RESULT = int;
using DeviceId = int;
constexpr int X_ERROR_SUCCESS = 0, X_ERROR_DEVICE_NOT_CONNECTED = 1167;
'''
types = input_types[input_types.index("enum X_INPUT_CAPS"):input_types.index("struct X_INPUT_KEYSTROKE")]
harness += re.sub(r"static_assert_size\([^;]+;", "", types)
begin = policy.index("struct DeviceStatePolicy")
harness += policy[begin:policy.index("\n};", begin) + 4]
harness += function(merge, "int16_t LargerMagnitude(")
harness += function(merge, "void MergeInto(")
harness += '''
struct CapsDriver {
  DeviceId id_ = 7;
  bool native_direct_gears_ = false;
  X_RESULT GetDeviceCapabilities(DeviceId, uint32_t, X_INPUT_CAPABILITIES*);
};
'''
caps = function(driver, "X_RESULT WheelkitInputDriver::GetDeviceCapabilities(")
harness += caps.replace("WheelkitInputDriver::", "CapsDriver::").replace("ri::", "")
harness += "}\nnamespace ri = rex::input;\nusing ri::X_RESULT;\n"
begin = driver.index("float Level(")
harness += driver[begin:driver.index("}  // namespace", begin)]
harness += '''
ri::X_INPUT_STATE Fold(ri::X_INPUT_STATE preferred_state, ri::X_INPUT_STATE keyboard_state,
                      uint32_t preferred_axes, uint32_t exclusive_axes) {
  using namespace ri;
  X_INPUT_STATE merged{};
  merged.gamepad.thumb_rx = -32000; // unrelated physical pad already merged
  merged.gamepad.thumb_ry = -32000;
  MergeInto(merged, preferred_state);
  MergeInto(merged, keyboard_state);
  bool any_keyboard = true;
'''
begin = system.index("  // Preferred axes exclude")
harness += system[begin:system.index("  if (!host_pads_only)", begin)]
harness += "return merged;\n}\n"
harness += (root / "tests/input/direct_gears_seam.cpp").read_text()
args.out.mkdir(parents=True, exist_ok=True)
fixture = args.out.resolve() / "direct_gears_generated.cpp"
executable = args.out.resolve() / "direct_gears_seam.exe"
fixture.write_text(harness)
shim = root / "thirdparty/dbce-shim/native/shim/shim"
subprocess.run([args.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-I" + str(root / "src"), "-I" + str(shim),
                "-I" + str(root / "thirdparty/dbce-shim/native/controls"),
                str(fixture), str(shim / "parse.cpp"), "-o", str(executable)], check=True)
subprocess.run([str(executable)], check=True)
