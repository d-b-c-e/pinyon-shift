#include "input/wheelkit_input_driver.h"
#include "input/wheelkit_direct_gears.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace pinyon_shift::input {
using rex::X_RESULT;
using rex::X_STATUS;
namespace ri = rex::input;
namespace {
std::atomic<uint64_t> next_device{0x574B495400000001ull};  // WKIT namespace

float Level(const WheelkitSnapshot& snapshot, const char* name) {
  const auto& action = snapshot.actions[dbce::shim::action_index(name)];
  return action.status == dbce::shim::Status::Live && std::isfinite(action.value)
             ? action.value : 0.0f;
}

const struct ButtonBinding { const char* action; uint16_t button; } buttons[] = {
    {"clutch", ri::X_INPUT_GAMEPAD_LEFT_SHOULDER},
    {"handbrake", ri::X_INPUT_GAMEPAD_A},
    {"shiftUp", ri::X_INPUT_GAMEPAD_B},
    {"shiftDown", ri::X_INPUT_GAMEPAD_X},
    {"confirm", ri::X_INPUT_GAMEPAD_A},
    {"back", ri::X_INPUT_GAMEPAD_B},
    {"start", ri::X_INPUT_GAMEPAD_START},
    {"navUp", ri::X_INPUT_GAMEPAD_DPAD_UP},
    {"navDown", ri::X_INPUT_GAMEPAD_DPAD_DOWN},
    {"navLeft", ri::X_INPUT_GAMEPAD_DPAD_LEFT},
    {"navRight", ri::X_INPUT_GAMEPAD_DPAD_RIGHT},
    {"camera", ri::X_INPUT_GAMEPAD_RIGHT_SHOULDER},
};

ri::DeviceStatePolicy Policy(const WheelkitSnapshot& snapshot, bool native_gears, bool active) {
  ri::DeviceStatePolicy policy;
  if (native_gears) {
    // Keep invalid/stale gear values authoritative: another stick is not a gear.
    policy.bound_axes = policy.exclusive_axes =
        ri::DeviceStatePolicy::kRightX | ri::DeviceStatePolicy::kRightY;
    policy.valid = true;
  }
  if (!active || snapshot.validity != SnapshotValidity::Live) return policy;
  auto applied = [&](const char* action) {
    const auto& value = snapshot.actions[dbce::shim::action_index(action)];
    return dbce::shim::is_applied(value.status) && std::isfinite(value.value);
  };
  // Core status is the resolved binding/availability signal. In particular,
  // a bound centered/rearming zero is different from Unbound or DeviceMissing.
  if (applied("steer")) policy.bound_axes |= ri::DeviceStatePolicy::kLeftX;
  if (applied("throttle")) policy.bound_axes |= ri::DeviceStatePolicy::kRightTrigger;
  if (applied("brake")) policy.bound_axes |= ri::DeviceStatePolicy::kLeftTrigger;
  policy.valid = policy.bound_axes != 0;
  for (const auto& binding : buttons) policy.valid |= applied(binding.action);
  return policy;
}

ri::X_INPUT_GAMEPAD Project(const WheelkitSnapshot& snapshot, bool native_gears, bool active) {
  ri::X_INPUT_GAMEPAD pad{};
  if (native_gears) pad.thumb_rx = kNoDirectGear;
  if (!active || snapshot.validity != SnapshotValidity::Live) return pad;
  if (native_gears) {
    pad.thumb_rx = DirectGear(snapshot);
    pad.thumb_ry = int16_t(std::lround(
        std::clamp(Level(snapshot, "clutch"), 0.0f, 1.0f) * 32767.0f));
  }
  const float steer = std::clamp(Level(snapshot, "steer"), -1.0f, 1.0f);
  pad.thumb_lx = int16_t(std::lround(steer * (steer < 0 ? 32768.0f : 32767.0f)));
  pad.right_trigger = uint8_t(std::lround(
      std::clamp(Level(snapshot, "throttle"), 0.0f, 1.0f) * 255.0f));
  pad.left_trigger = uint8_t(std::lround(
      std::clamp(Level(snapshot, "brake"), 0.0f, 1.0f) * 255.0f));
  // FH1 roles: src/config/default_keys.h and the existing SETTINGS key labels.
  // The default pad path turns clutch/handbrake into binary LB/A controls.
  uint16_t held{};
  for (const auto& binding : buttons) {
    if (Level(snapshot, binding.action) < 0.5f) continue;
    if (native_gears) {
      const std::string_view action(binding.action);
      // Native wheel layout 1 uses LB/RB for shifts and rightY for clutch.
      if (action == "clutch" || action == "camera") continue;
      if (action == "shiftUp") {
        held |= ri::X_INPUT_GAMEPAD_LEFT_SHOULDER;
        continue;
      }
      if (action == "shiftDown") {
        held |= ri::X_INPUT_GAMEPAD_RIGHT_SHOULDER;
        continue;
      }
    }
    held |= binding.button;
  }
  pad.buttons = held;
  return pad;
}
}  // namespace

WheelkitInputDriver::WheelkitInputDriver(
    std::shared_ptr<WheelkitInputLifecycle> lifecycle, Clock clock, bool native_direct_gears)
    : InputDriver(nullptr, 0), lifecycle_(std::move(lifecycle)),
      clock_(std::move(clock)), native_direct_gears_(native_direct_gears),
      id_(static_cast<ri::DeviceId>(next_device.fetch_add(1))) {
  if (!lifecycle_) throw std::invalid_argument("input lifecycle required");
  if (!clock_) clock_ = [] {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
  };
}

X_STATUS WheelkitInputDriver::Setup() { return X_STATUS_SUCCESS; }

void WheelkitInputDriver::EnumerateDevices(std::vector<ri::DeviceInfo>& out) {
  ri::DeviceInfo info;
  info.id = id_;
  info.name = "Wheelkit resolved input";
  info.synthetic = true;  // never decide pad rumble success or consume a pad slot
  out.push_back(std::move(info));
}

X_RESULT WheelkitInputDriver::GetDeviceState(ri::DeviceId id, ri::X_INPUT_STATE* out) {
  return GetDeviceStateWithPolicy(id, out, nullptr);
}

X_RESULT WheelkitInputDriver::GetDeviceStateWithPolicy(
    ri::DeviceId id, ri::X_INPUT_STATE* out, ri::DeviceStatePolicy* policy) {
  if (id != id_) return X_ERROR_DEVICE_NOT_CONNECTED;
  const auto snapshot = lifecycle_->Read(clock_());
  const bool active = is_active();
  const auto pad = Project(*snapshot, native_direct_gears_, active);
  if (policy) *policy = Policy(*snapshot, native_direct_gears_, active);
  std::lock_guard lock(packet_mutex_);
  if (!have_packet_ || std::memcmp(&previous_, &pad, sizeof(pad)) != 0) {
    previous_ = pad;
    have_packet_ = true;
    ++packet_;
  }
  if (out) {
    *out = {};
    out->gamepad = pad;
    out->packet_number = packet_;
  }
  return X_ERROR_SUCCESS;
}

X_RESULT WheelkitInputDriver::GetDeviceCapabilities(
    ri::DeviceId id, uint32_t, ri::X_INPUT_CAPABILITIES* out) {
  if (id != id_) return X_ERROR_DEVICE_NOT_CONNECTED;
  if (out) {
    *out = {};
    out->type = ri::XINPUT_DEVTYPE_GAMEPAD;
    out->sub_type = native_direct_gears_ ? 2 : 1;
    out->gamepad.buttons = ri::X_INPUT_GAMEPAD_A | ri::X_INPUT_GAMEPAD_B |
        ri::X_INPUT_GAMEPAD_X | ri::X_INPUT_GAMEPAD_LEFT_SHOULDER |
        ri::X_INPUT_GAMEPAD_RIGHT_SHOULDER | ri::X_INPUT_GAMEPAD_START |
        ri::X_INPUT_GAMEPAD_DPAD_UP | ri::X_INPUT_GAMEPAD_DPAD_DOWN |
        ri::X_INPUT_GAMEPAD_DPAD_LEFT | ri::X_INPUT_GAMEPAD_DPAD_RIGHT;
    out->gamepad.left_trigger = out->gamepad.right_trigger = UINT8_MAX;
    out->gamepad.thumb_lx = INT16_MAX;
    if (native_direct_gears_) {
      // FH1 requires the native wheel FFB flag to recognize its shifter.
      // This is a guest protocol marker; this driver forwards no force output.
      out->flags = ri::X_INPUT_CAPS_FFB_SUPPORTED;
      out->gamepad.thumb_lx = -1;  // SDL default, no guessed 900-degree hardware
      out->gamepad.thumb_rx = out->gamepad.thumb_ry = -1;
    }
  }
  return X_ERROR_SUCCESS;
}

X_RESULT WheelkitInputDriver::SetDeviceVibration(ri::DeviceId id,
                                                ri::X_INPUT_VIBRATION*) {
  return id == id_ ? X_ERROR_SUCCESS : X_ERROR_DEVICE_NOT_CONNECTED;
}

X_RESULT WheelkitInputDriver::GetDeviceKeystroke(ri::DeviceId id, uint32_t,
                                                ri::X_INPUT_KEYSTROKE*) {
  return id == id_ ? X_ERROR_EMPTY : X_ERROR_DEVICE_NOT_CONNECTED;
}

}  // namespace pinyon_shift::input
