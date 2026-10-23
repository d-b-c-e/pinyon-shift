#pragma once

#include <filesystem>
#include <optional>

namespace pinyon_shift::diagnostics {

inline std::optional<std::filesystem::path> DeployedRoot(
    const std::filesystem::path& executable_directory,
    bool explicit_game_root, bool explicit_state_root) {
  if (explicit_game_root || explicit_state_root) return std::nullopt;
  const auto root = executable_directory.parent_path();
  std::error_code error;
  if (!std::filesystem::is_regular_file(root / "game" / "default.xex", error) || error)
    return std::nullopt;
  if (!std::filesystem::is_directory(root / "state", error) || error)
    return std::nullopt;
  return root;
}

}  // namespace pinyon_shift::diagnostics
