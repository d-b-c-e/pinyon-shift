#include "input/wheelkit_app_input.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "dinput_provider.hpp"

namespace pinyon_shift::input {
namespace {
std::wstring Environment(const wchar_t* name) {
  const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
  if (!size) return {};
  std::wstring value(size, L'\0');
  const DWORD copied = GetEnvironmentVariableW(name, value.data(), size);
  if (!copied || copied >= size) return {};
  value.resize(copied);
  return value;
}
std::string ReadProfile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("explicit Wheelkit profile cannot be opened");
  std::string bytes(dbce::shim::MaxProfileBytes + 1, '\0');
  input.read(bytes.data(), std::streamsize(bytes.size()));
  bytes.resize(size_t(input.gcount()));
  if (bytes.size() > dbce::shim::MaxProfileBytes || input.bad())
    throw std::runtime_error("Wheelkit profile exceeds bound or failed to read");
  return bytes;
}
}

std::int64_t WheelkitQpcMicroseconds() {
  // Identical epoch, frequency and integer rounding to shared DInputProvider.
  static const LONGLONG frequency = [] {
    LARGE_INTEGER value;
    QueryPerformanceFrequency(&value);
    return value.QuadPart;
  }();
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  return std::int64_t(counter.QuadPart / frequency * 1000000 +
      (counter.QuadPart % frequency) * 1000000 / frequency);
}

std::shared_ptr<WheelkitAppInput> PrepareNativeWheelkitInput(void* window) {
  if (Environment(L"PINYON_SHIFT_WHEELKIT") != L"1") return {};
  const auto profile = Environment(L"PINYON_SHIFT_WHEELKIT_PROFILE");
  const auto report = Environment(L"PINYON_SHIFT_WHEELKIT_REPORT_DIR");
  if (!window || profile.empty() || report.empty() || !std::filesystem::is_directory(report))
    throw std::runtime_error("Wheelkit needs a window, explicit profile and existing scratch report directory");
  WheelkitAppInputOptions options;
  options.enabled = true;
  options.native_direct_gears = Environment(L"PINYON_SHIFT_WHEELKIT_NATIVE_GEARS") == L"1";
  options.lifecycle.profile_bytes = ReadProfile(profile);
  options.lifecycle.report_folder = report;
  return std::make_shared<WheelkitAppInput>(std::move(options),
      [native_window = static_cast<HWND>(window)]() -> std::shared_ptr<dbce::shim::ProviderOwner> {
        IDirectInput8W* input = nullptr;
        if (FAILED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION,
                                     IID_IDirectInput8W, reinterpret_cast<void**>(&input), nullptr)))
          throw std::runtime_error("shared DirectInput provider creation failed");
        return std::make_shared<dbce::shim::DInputProvider>(input, native_window);
      }, &WheelkitQpcMicroseconds);
}
}  // namespace pinyon_shift::input
