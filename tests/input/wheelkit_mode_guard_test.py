"""Compile the production native-wheel guard with fake cvars: no device, game or force is touched.

The SDK's raw wheel (wheel_enabled) can be turned on by the config file, a REX_* environment variable or a
command-line flag, whatever the file says. The private shim must then not start, and the effective native and
force settings, with their sources and the config file, must be in the log.
"""
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
source = (root / "src/input/wheelkit_native_input.cpp").read_text()
begin = source.index("bool NativeWheelEnabled()")
helper = source[begin:source.index("\n}", begin) + 2]
begin = source.index("{", source.index("PrepareNativeWheelkitInput(void* window)")) + 1
guard = source[begin:source.index("  WheelkitAppInputOptions options;", begin)]
assert "NativeWheelEnabled()" in guard and "throw" in guard, "the guard must refuse before the activation route"
header = (sdk / "include/rex/cvar.h").read_text()
begin = header.index("enum class Source {")
source_enum = header[begin:header.index("};", begin) + 2]
defined = set(re.findall(r"REXCVAR_DEFINE_\w+\(\s*(wheel_\w+)\s*,",
                         "".join(p.read_text(errors="replace") for p in (sdk / "src").rglob("*.cpp"))))
logged = re.findall(r'"(wheel_\w+)"', helper)
missing = sorted(set(logged) - defined)
assert not missing, f"logged names that are not SDK cvars: {missing}"

harness = r'''
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace rex::cvar {
''' + source_enum + r'''
std::map<std::string, std::pair<std::string, Source>, std::less<>> flags;
std::string GetFlagByName(std::string_view name) {
  const auto it = flags.find(name); return it == flags.end() ? "" : it->second.first;
}
Source GetFlagSource(std::string_view name) {
  const auto it = flags.find(name); return it == flags.end() ? Source::kDefault : it->second.second;
}
}  // namespace rex::cvar
namespace rex {
std::string path_to_utf8(const std::filesystem::path& path) { return path.generic_string(); }
}
namespace pinyon_shift::diagnostics {
const std::filesystem::path& StateRoot() { static const std::filesystem::path root = "D:/Pinyon Shift/state"; return root; }
}
std::vector<std::string> lines;
void Format(std::ostringstream& out, const char* text) { out << text; }
template <class T, class... Rest> void Format(std::ostringstream& out, const char* text, const T& value, const Rest&... rest) {
  const char* open = std::strstr(text, "{}");
  if (!open) { out << text; return; }
  out.write(text, open - text); out << value; Format(out, open + 2, rest...);
}
template <class... Args> void Log(const char* text, const Args&... args) {
  std::ostringstream out; Format(out, text, args...); lines.push_back(out.str());
}
#define REXLOG_INFO(...) Log(__VA_ARGS__)
''' + helper + r'''
int opened = 0;
std::shared_ptr<int> Prepare() {
''' + guard + r'''
  ++opened;  // stands for the activation route: profile read, then device and force opening
  return std::make_shared<int>(1);
}
int checks = 0;
void check(bool passed, const char* why) {
  ++checks;
  if (!passed) {
    std::cerr << "FAIL: " << why << '\n';
    for (const auto& line : lines) std::cerr << "  " << line << '\n';
    std::exit(1);
  }
}
bool Has(const std::string& text) { return !lines.empty() && lines.back().find(text) != std::string::npos; }
int main() {
  using rex::cvar::Source;
  auto& flags = rex::cvar::flags;
  // The file asks for the shim (native off, low force); the environment and command line say otherwise.
  flags["wheel_enabled"] = {"true", Source::kEnvironment};
  flags["wheel_ffb_strength"] = {"100", Source::kCommandLine};
  flags["wheel_ffb_mode"] = {"constant", Source::kConfig};
  flags["wheel_ffb_device"] = {"MOZA R12 Base", Source::kRuntime};
  std::string reason;
  try { Prepare(); } catch (const std::runtime_error& error) { reason = error.what(); }
  check(reason.find("wheel_enabled") != std::string::npos && opened == 0,
        "an environment override turning the native wheel on refuses the shim before anything opens");
  check(lines.size() == 1, "the settings are logged once, before the refusal");
  check(Has("config D:/Pinyon Shift/state/config/pinyon_shift.toml:"), "the log names the resolved config file");
  check(Has(" wheel_enabled=\"true\" (environment)"), "the native mode and its source");
  check(Has(" wheel_ffb_strength=\"100\" (command line)"), "a command-line force override");
  check(Has(" wheel_ffb_mode=\"constant\" (config)"), "a file force setting");
  check(Has(" wheel_ffb_device=\"MOZA R12 Base\" (runtime)"), "the last source name lines up with the SDK enum");

  flags["wheel_enabled"] = {"false", Source::kCommandLine};
  check(Prepare() && opened == 1, "with the native wheel off the activation route continues");
  check(lines.size() == 2 && Has(" wheel_enabled=\"false\" (command line)"), "and the settings are still logged");
  std::cout << checks << " guard checks passed (production guard, fake cvars, ''' + str(len(logged)) + r''' logged SDK cvars)\n";
}
'''
out = args.out.resolve()
out.mkdir(parents=True, exist_ok=True)
generated = out / "wheelkit_mode_guard_generated.cpp"
generated.write_text(harness)
executable = out / "wheelkit_mode_guard_test.exe"
subprocess.run([args.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", str(generated), "-o", str(executable)],
               check=True)
subprocess.run([str(executable)], check=True)
