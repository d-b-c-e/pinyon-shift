#pragma once

#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "shim.hpp"
#include "thirdparty/crypto/sha256.h"

namespace pinyon_shift::input {

// IniDocument uses String.Trim; input has already passed strict UTF-8 validation.
inline std::string_view ActivationTrim(std::string_view text) {
  size_t first = text.size(), last = 0;
  for (size_t i = 0; i < text.size();) {
    const auto start = i;
    unsigned code = static_cast<unsigned char>(text[i++]);
    if (code >= 0xC0) {
      const int extra = code < 0xE0 ? 1 : code < 0xF0 ? 2 : 3;
      code &= (1u << (6 - extra)) - 1;
      for (int j = 0; j < extra; ++j)
        code = (code << 6) | (static_cast<unsigned char>(text[i++]) & 0x3F);
    }
    const bool space = (code >= 9 && code <= 13) || code == 0x20 || code == 0x85 ||
        code == 0xA0 || code == 0x1680 || (code >= 0x2000 && code <= 0x200A) ||
        code == 0x2028 || code == 0x2029 || code == 0x202F || code == 0x205F || code == 0x3000;
    if (!space) { if (first == text.size()) first = start; last = i; }
  }
  return first == text.size() ? std::string_view{} : text.substr(first, last - first);
}

inline std::string ActivationLower(std::string_view text) {
  std::string result(text);
  for (auto& c : result) if (c >= 'A' && c <= 'Z') c = char(c + ('a' - 'A'));
  return result;
}

inline std::optional<std::array<std::string, 2>> WheelkitActivationHashes(
    const std::string& bytes) {
  if (bytes.size() > dbce::shim::MaxProfileBytes || !dbce::shim::strict_utf8(bytes))
    return std::nullopt;
  std::istringstream lines(bytes.compare(0, 3, "\xEF\xBB\xBF") == 0 ? bytes.substr(3) : bytes);
  std::array<std::string, 2> hashes;
  bool found[2]{}, inside = false;
  std::string raw;
  while (std::getline(lines, raw)) {
    const auto line = ActivationTrim(raw);
    if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
      if (inside) break;  // IniDocument only reads the first matching section.
      inside = ActivationLower(ActivationTrim(line.substr(1, line.size() - 2))) == "activation";
      continue;
    }
    if (!inside || line.empty() || line.front() == ';' || line.front() == '#') continue;
    const auto equal = line.find('=');
    if (equal == std::string_view::npos || equal == 0) continue;
    const auto key = ActivationLower(ActivationTrim(line.substr(0, equal)));
    const int slot = key == "executable" ? 0 : key == "runtime" ? 1 : -1;
    if (slot < 0 || found[slot]) continue;
    found[slot] = true;
    hashes[slot] = ActivationLower(ActivationTrim(line.substr(equal + 1)));
  }
  for (const auto& hash : hashes) {
    if (hash.size() != 64) return std::nullopt;
    for (char c : hash) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
      return std::nullopt;
  }
  return hashes;
}

inline std::string ReadWheelkitProfile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("Wheelkit profile cannot be opened");
  std::string bytes(dbce::shim::MaxProfileBytes + 1, '\0');
  input.read(bytes.data(), std::streamsize(bytes.size()));
  bytes.resize(size_t(input.gcount()));
  if (bytes.size() > dbce::shim::MaxProfileBytes || input.bad())
    throw std::runtime_error("Wheelkit profile exceeds bound or failed to read");
  return bytes;
}

inline std::string WheelkitModuleHash(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("Wheelkit module cannot be read");
  sha256::SHA256 hash;
  std::array<char, 65536> buffer;
  while (input) {
    input.read(buffer.data(), std::streamsize(buffer.size()));
    hash.add(buffer.data(), size_t(input.gcount()));
  }
  if (!input.eof() || input.bad()) throw std::runtime_error("Wheelkit module hash read failed");
  return hash.getHash();
}

struct AppliedWheelkitProfile {
  std::string bytes;
  std::filesystem::path report_folder;
};

inline AppliedWheelkitProfile ReadAppliedWheelkitProfile(
    const std::filesystem::path& executable, const std::filesystem::path& runtime,
    const std::filesystem::path& state_root) {
  if (executable.empty() || runtime.empty())
    throw std::runtime_error("running Wheelkit modules could not be located");
  auto bytes = ReadWheelkitProfile(executable.parent_path() / "dbce-controls.ini");
  const auto hashes = WheelkitActivationHashes(bytes);
  if (!hashes || (*hashes)[0] != WheelkitModuleHash(executable) ||
      (*hashes)[1] != WheelkitModuleHash(runtime))
    throw std::runtime_error("Wheelkit controls in dbce-controls.ini were applied for another build");
  if (state_root.empty() || !std::filesystem::is_directory(state_root))
    throw std::runtime_error("Wheelkit resolved state root is unavailable");
  auto report = state_root / "wheelkit";
  std::filesystem::create_directories(report);
  return {std::move(bytes), std::move(report)};
}

}  // namespace pinyon_shift::input
