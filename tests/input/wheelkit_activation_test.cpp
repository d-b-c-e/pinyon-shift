// Template compiled by wheelkit_activation_test.py with production function bodies.
#include "input/wheelkit_activation.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace fs = std::filesystem;
using namespace pinyon_shift::input;
int checks = 0, logs = 0;
void check(bool value, const char* why) {
  ++checks;
  if (!value) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); }
}
void write(const fs::path& path, const std::string& bytes) {
  std::ofstream file(path, std::ios::binary); file << bytes;
  if (!file) throw std::runtime_error("scratch write failed");
}
std::optional<std::string> explicit_enabled;
std::wstring explicit_profile, explicit_report;
fs::path running_exe, loaded_runtime, state_root;
bool runtime_loaded = true, lookup_failed = false, lookup_truncated = false;
using DWORD = unsigned long;
using HMODULE = void*;
HMODULE GetModuleHandleW(const wchar_t*) { return runtime_loaded ? reinterpret_cast<void*>(1) : nullptr; }
DWORD GetModuleFileNameW(HMODULE module, wchar_t* buffer, DWORD size) {
  if (lookup_failed) return 0;
  if (lookup_truncated) return size;
  const auto text = (module ? loaded_runtime : running_exe).wstring();
  if (text.size() >= size) return size;
  std::copy(text.begin(), text.end(), buffer);
  return DWORD(text.size());
}
namespace platform {
std::optional<std::string> EnvironmentVariable(const char*) { return explicit_enabled; }
}
namespace pinyon_shift::diagnostics {
const fs::path& StateRoot() { return state_root; }
}
std::wstring Environment(const wchar_t* key) {
  return std::wstring(key) == L"PINYON_SHIFT_WHEELKIT_PROFILE" ? explicit_profile : explicit_report;
}
struct WheelkitAppInputOptions {
  bool enabled{};
  struct {std::string profile_bytes; std::wstring report_folder;} lifecycle;
};
#define REXLOG_INFO(...) (++logs)
// PRODUCTION_MODULE_PATH
std::optional<WheelkitAppInputOptions> PrepareFixture(void* window) {
// PRODUCTION_ROUTE
  return options; // no provider construction or device work in this fixture
}
bool rejected(const fs::path& exe, const fs::path& runtime, const fs::path& state) {
  try { (void)ReadAppliedWheelkitProfile(exe, runtime, state); return false; }
  catch (const std::exception&) { return true; }
}
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const auto root = fs::absolute(argv[1]);
  fs::create_directory(root / "bin");
  fs::create_directory(root / "state");
  running_exe = root / "bin" / "pinyon_shift.exe";
  // Deliberately not bin/rexruntime.dll: hash the module actually loaded.
  loaded_runtime = root / "actual-loaded-runtime.dll";
  state_root = root / "state";
  write(running_exe, "abc"); write(loaded_runtime, "runtime bytes");
  const auto exe_hash = WheelkitModuleHash(running_exe);
  const auto runtime_hash = WheelkitModuleHash(loaded_runtime);
  check(exe_hash == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "real streaming SHA256 known vector");
  const auto controls = root / "bin" / "dbce-controls.ini";
  const auto block = "[Activation]\nExecutable = " + exe_hash + "\nRuntime = " + runtime_hash + "\n";
  const auto wrong = std::string(64, '0');
  const std::string vectors[] = {
      block,
      "[ activation ]\r\n eXeCuTaBlE = " + exe_hash + " \r\n RUNTIME = " + runtime_hash + "\t\r\n",
      "[Activation]\nExecutable=" + wrong + "\nRuntime=" + runtime_hash + "\n" + block,
      "[Activation]\nRuntime=" + runtime_hash + "\n" + block,
      "[Activation]\nExecutable=" + exe_hash + " ; applied\nRuntime=" + runtime_hash,
      "[Activation] ; note\nExecutable=" + exe_hash + "\nRuntime=" + runtime_hash,
      "[Activation]\n; Executable=" + exe_hash + "\nRuntime=" + runtime_hash,
      "Executable=" + exe_hash + "\nRuntime=" + runtime_hash,
      block + "Executable=" + wrong + "\n"};
  const bool expected[] = {true, true, false, false, false, false, false, false, true};
  for (size_t i = 0; i < std::size(vectors); ++i) {
    write(controls, vectors[i]);
    check(!rejected(running_exe, loaded_runtime, state_root) == expected[i],
          "nine real IniDocument grammar vectors from writer66277a9");
  }
  auto hashes = WheelkitActivationHashes("\xEF\xBB\xBF" + block);
  check(hashes && (*hashes)[0] == exe_hash, "optional UTF8 BOM");
  std::string uppercase = exe_hash;
  for (char& c : uppercase) if (c >= 'a' && c <= 'f') c = char(c - ('a' - 'A'));
  hashes = WheelkitActivationHashes("[Activation]\nExecutable=" + uppercase + "\nRuntime=" + runtime_hash);
  check(hashes && (*hashes)[0] == exe_hash, "uppercase hash accepted");
  hashes = WheelkitActivationHashes("\xC2\xA0[Activation]\xC2\xA0\nExecutable=" + exe_hash +
      "\nRuntime=\xE3\x80\x80" + runtime_hash + "\xE3\x80\x80");
  check(hashes.has_value(), "Unicode trim matches writer whitespace");
  check(!WheelkitActivationHashes("\xFF" + block), "invalid UTF8 refused");
  check(!WheelkitActivationHashes(std::string(1, '\0') + block), "NUL text refused by shared reader");
  check(!WheelkitActivationHashes("[Activation]\nExecutable=" + exe_hash +
      "\n[Other]\nRuntime=" + runtime_hash + "\n" + block), "later section cannot fill missing runtime");
  check(!WheelkitActivationHashes("[Activation]\nExecutable=\n" + block), "empty first key cannot be repaired later");
  check(!WheelkitActivationHashes("[Activation]\nExecutable=" + exe_hash +
      "#note\nRuntime=" + runtime_hash), "inline hash comment refused");
  check(!WheelkitActivationHashes("[Activation]\nExecutable=" + wrong.substr(1) +
      "\nRuntime=" + runtime_hash), "63 characters refused");
  check(!WheelkitActivationHashes("[Activation]\nExecutable=" + wrong +
      "g\nRuntime=" + runtime_hash), "nonhex/extra character refused");
  auto bounded = block + "#";
  bounded.resize(dbce::shim::MaxProfileBytes, ' ');
  write(controls, bounded);
  check(ReadWheelkitProfile(controls).size() == 65536 && WheelkitActivationHashes(bounded),
        "exact64KiB accepted");
  write(controls, bounded + " ");
  check(rejected(running_exe, loaded_runtime, state_root), "65537bytes refused");
  check(!WheelkitActivationHashes(bounded + " "), "parser also enforces total cap");
  write(controls, block);
  check(rejected(root / "bin" / "missing.exe", loaded_runtime, state_root), "missing module refused");
  check(rejected(running_exe, root / "missing.dll", state_root), "unreadable loaded runtime refused");
  check(rejected(running_exe, loaded_runtime, root / "absent-state"), "missing resolved state refused");
  check(!fs::exists(root / "absent-state"), "activation never creates an unresolved state root");
  write(loaded_runtime, "redeployed runtime");
  check(rejected(running_exe, loaded_runtime, state_root), "runtime replacement invalidates activation");
  write(loaded_runtime, "runtime bytes");
  write(running_exe, "redeployed exe");
  check(rejected(running_exe, loaded_runtime, state_root), "redeploy invalidates activation");
  state_root = root / "redeployed-state"; fs::create_directory(state_root);
  logs = 0;
  check(!PrepareFixture(&checks) && logs == 1, "mismatch uses ordinary input with one log");
  check(!fs::exists(state_root / "wheelkit"), "mismatch creates no report folder");
  write(running_exe, "abc");
  auto selected = PrepareFixture(&checks);
  check(selected && selected->enabled && selected->lifecycle.profile_bytes == block,
        "valid plainPlay selects exact controls bytes");
  check(selected && selected->lifecycle.report_folder == (state_root / "wheelkit").wstring() &&
        fs::is_directory(state_root / "wheelkit"), "matched report uses resolved state root");
  runtime_loaded = false; logs = 0;
  check(!PrepareFixture(&checks) && logs == 1, "missing loaded module is ordinary input");
  runtime_loaded = true; lookup_failed = true;
  check(!PrepareFixture(&checks), "failed module path lookup refused");
  lookup_failed = false; lookup_truncated = true;
  check(!PrepareFixture(&checks), "truncated module path refused");
  lookup_truncated = false;
  for (auto value : {"0", "", "true"}) {
    explicit_enabled = value; logs = 0;
    check(!PrepareFixture(&checks) && logs == 0, "explicit non1 disables autoactivation");
  }
  explicit_enabled = "1";
  explicit_profile = (root / "developer.ini").wstring();
  explicit_report = (root / "developer-reports").wstring();
  write(root / "developer.ini", "developer bytes without Activation");
  fs::create_directory(explicit_report);
  runtime_loaded = false;
  selected = PrepareFixture(&checks);
  check(selected && selected->lifecycle.profile_bytes == "developer bytes without Activation" &&
        selected->lifecycle.report_folder == explicit_report, "explicit developer route wins without loaded hash check");
  explicit_enabled.reset();
  check(!PrepareFixture(nullptr), "no window does not activate");
  std::cout << checks << " activation checks passed (production parser/hash/route; scratch modules)\n";
}
