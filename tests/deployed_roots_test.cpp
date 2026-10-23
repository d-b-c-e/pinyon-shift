#include "deployed_roots.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include "deployed_roots_init.inc"

int checks = 0;
void check(bool result, const char* why) {
  ++checks;
  if (!result) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); }
}
int main(int argc, char** argv) {
  namespace fs = std::filesystem;
  using pinyon_shift::diagnostics::DeployedRoot;
  if (argc != 2) return 2;
  const auto root = fs::absolute(argv[1]); // runner owns this fresh temporary folder
  const auto bin = root / "bin";
  fs::create_directory(bin);
  check(!DeployedRoot(bin, false, false), "no markers keeps original roots");
  check(!fs::exists(root / "state"), "resolver never creates state");
  fs::create_directory(root / "game");
  std::ofstream(root / "game" / "default.xex") << "fixture marker, not game data";
  check(!DeployedRoot(bin, false, false), "game-only incomplete layout rejected");
  fs::create_directory(root / "state");
  std::ofstream(root / "state" / "owner-sentinel") << "preserved";
  check(DeployedRoot(bin, false, false) == root, "complete sibling layout selected");
  check(!DeployedRoot(bin, true, false), "game env alone disables both fallbacks");
  check(!DeployedRoot(bin, false, true), "state env alone disables both fallbacks");
  check(!DeployedRoot(bin, true, true), "both explicit envs disable fallback");
  fixture_executable_directory = bin;
  ResolveFixtureRoots();
  check(g_game_root == root / "game" && g_state_root == root / "state",
        "production initialization selects both deployed roots");
  fixture_game_env = (root / "explicit-game").string();
  ResolveFixtureRoots();
  check(g_game_root == root / "explicit-game" && g_state_root == bin / "pinyon_shift_state",
        "game-only env retains original state behavior");
  fixture_game_env.reset();
  fixture_state_env = (root / "explicit-state").string();
  ResolveFixtureRoots();
  check(!g_game_root && g_state_root == root / "explicit-state",
        "state-only env retains SDK game-root behavior");
  fixture_game_env = (root / "explicit-game").string();
  ResolveFixtureRoots();
  check(g_game_root == root / "explicit-game" && g_state_root == root / "explicit-state",
        "both explicit paths win");
  fixture_game_env = "";
  fixture_state_env.reset();
  ResolveFixtureRoots();
  check(!g_game_root && g_state_root == bin / "pinyon_shift_state",
        "present empty environment does not trigger deployed fallback");
  fixture_game_env.reset();
  std::ifstream sentinel(root / "state" / "owner-sentinel");
  std::string bytes; sentinel >> bytes;
  check(bytes == "preserved", "existing state not modified");
  const auto state_only = root / "state-only";
  fs::create_directories(state_only / "state");
  check(!DeployedRoot(state_only / "bin", false, false), "state-only incomplete layout rejected");
  fixture_executable_directory = state_only / "bin";
  ResolveFixtureRoots();
  check(!g_game_root && g_state_root == state_only / "bin" / "pinyon_shift_state",
        "production incomplete layout keeps original roots");
  const auto bad_marker = root / "directory-marker";
  fs::create_directories(bad_marker / "game" / "default.xex");
  fs::create_directory(bad_marker / "state");
  check(!DeployedRoot(bad_marker / "bin", false, false), "directory named default.xex rejected");
  const auto bad_state = root / "state-file";
  fs::create_directories(bad_state / "game");
  std::ofstream(bad_state / "game" / "default.xex") << "fixture";
  std::ofstream(bad_state / "state") << "not a directory";
  check(!DeployedRoot(bad_state / "bin", false, false), "file named state rejected");
  std::cout << checks << " deployed-root checks passed (scratch paths only)\n";
}
