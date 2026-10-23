#include "input/wheelkit_app_input.h"
#include "input/wheelkit_activation.h"

#include <filesystem>
#include <stdexcept>

#include <rex/logging.h>

#include "dinput_provider.hpp"
#include "pinyon_shift_diagnostics.h"
#include "platform/host_platform.h"

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
std::filesystem::path ModulePath(HMODULE module) {
  std::wstring path(32768, L'\0');
  const DWORD length = GetModuleFileNameW(module, path.data(), DWORD(path.size()));
  if (!length || length >= path.size()) return {};
  path.resize(length);
  return path;
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
  WheelkitAppInputOptions options;
  const auto enabled = platform::EnvironmentVariable("PINYON_SHIFT_WHEELKIT");
  if (enabled) {
    if (*enabled != "1") return {};
    const auto profile = Environment(L"PINYON_SHIFT_WHEELKIT_PROFILE");
    const auto report = Environment(L"PINYON_SHIFT_WHEELKIT_REPORT_DIR");
    if (!window || profile.empty() || report.empty() || !std::filesystem::is_directory(report))
      throw std::runtime_error("Wheelkit needs a window, explicit profile and existing scratch report directory");
    options.lifecycle.profile_bytes = ReadWheelkitProfile(profile);
    options.lifecycle.report_folder = report;
  } else {
    try {
      const auto runtime = GetModuleHandleW(L"rexruntime.dll");
      if (!runtime) throw std::runtime_error("loaded rexruntime.dll is unavailable");
      if (!window) throw std::runtime_error("Wheelkit window is unavailable");
      auto applied = ReadAppliedWheelkitProfile(ModulePath(nullptr), ModulePath(runtime),
          pinyon_shift::diagnostics::StateRoot());
      options.lifecycle.profile_bytes = std::move(applied.bytes);
      options.lifecycle.report_folder = applied.report_folder.wstring();
    } catch (const std::exception& error) {
      REXLOG_INFO("{}; ordinary input retained", error.what());
      return {};
    }
  }
  options.enabled = true;
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
