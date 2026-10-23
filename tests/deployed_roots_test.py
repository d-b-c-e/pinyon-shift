"""Compile the deployed-root resolver and production initializer with inert paths."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--cxx", required=True)
parser.add_argument("--out", required=True, type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (root / "src/pinyon_shift_diagnostics.cpp").read_text()
begin = source.index("  const auto executable_directory = ExecutableDirectory();", source.index("bool InitializeEarly()"))
initializer = source[begin:source.index("  g_session_id =", begin)]
app = (root / "src/pinyon_shift_app.cpp").read_text()
assert 'if (auto game_root = diagnostics::GameRoot())' in app
assert 'paths.user_data_root = LongHostPath(state_root / "user");' in app
fixture = r'''
// Only host environment lookup and executable location are faked.
std::filesystem::path fixture_executable_directory, g_state_root;
std::optional<std::filesystem::path> g_game_root;
std::optional<std::string> fixture_game_env, fixture_state_env;
namespace platform {
std::optional<std::string> EnvironmentVariable(const char* name) {
  return std::string(name) == "PINYON_SHIFT_GAME_ROOT" ? fixture_game_env : fixture_state_env;
}
}
std::filesystem::path ExecutableDirectory() { return fixture_executable_directory; }
std::optional<std::filesystem::path> EnvironmentPath(const char* name) {
  auto text = platform::EnvironmentVariable(name);
  if (!text || text->empty()) return std::nullopt;
  return std::filesystem::absolute(*text).lexically_normal();
}
void ResolveFixtureRoots() {
  using pinyon_shift::diagnostics::DeployedRoot;
'''
fixture += initializer + "}\n"
args.out.mkdir(parents=True, exist_ok=True)
out = args.out.resolve()
(out / "deployed_roots_init.inc").write_text(fixture)
executable = out / "deployed_roots_test.exe"
subprocess.run([args.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-I" + str(root / "src"), "-I" + str(out),
                str(root / "tests/deployed_roots_test.cpp"), "-o", str(executable)], check=True)
with tempfile.TemporaryDirectory(prefix="pinyon-deployed-roots-") as temporary:
    subprocess.run([str(executable), temporary], check=True)
