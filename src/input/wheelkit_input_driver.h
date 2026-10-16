#pragma once

#include <functional>
#include <memory>
#include <mutex>

#include <rex/input/input_driver.h>

#include "input/wheelkit_input_lifecycle.h"

namespace pinyon_shift::input {

// Source-only adapter; construction does not register it with the game/SDK.
// Lifecycle owns all provider operations; this driver only projects snapshots.
class WheelkitInputDriver final : public rex::input::InputDriver {
 public:
  using Clock = std::function<std::int64_t()>;
  // Clock must share the provider's monotonic microsecond epoch. The default
  // serves steady-clock providers; native integration must verify its domain.
  explicit WheelkitInputDriver(std::shared_ptr<WheelkitInputLifecycle> lifecycle,
                              Clock clock = {});
  rex::X_STATUS Setup() override;
  void EnumerateDevices(std::vector<rex::input::DeviceInfo>& out) override;
  rex::X_RESULT GetDeviceState(rex::input::DeviceId id,
                              rex::input::X_INPUT_STATE* out) override;
  rex::X_RESULT GetDeviceCapabilities(rex::input::DeviceId id, uint32_t flags,
                                     rex::input::X_INPUT_CAPABILITIES* out) override;
  rex::X_RESULT SetDeviceVibration(rex::input::DeviceId id,
                                  rex::input::X_INPUT_VIBRATION* vibration) override;
  rex::X_RESULT GetDeviceKeystroke(rex::input::DeviceId id, uint32_t flags,
                                  rex::input::X_INPUT_KEYSTROKE* out) override;
  bool resolves_own_actions() const override { return true; }

 private:
  std::shared_ptr<WheelkitInputLifecycle> lifecycle_;
  Clock clock_;
  rex::input::DeviceId id_;
  std::mutex packet_mutex_;
  bool have_packet_{};
  uint32_t packet_{};
  rex::input::X_INPUT_GAMEPAD previous_{};
};

}  // namespace pinyon_shift::input
