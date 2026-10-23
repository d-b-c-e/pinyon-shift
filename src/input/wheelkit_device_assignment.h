#pragma once

#include <memory>
#include <rex/input/device_assignment.h>

namespace pinyon_shift::input {

// Per-title opt-in wrapper. Routing remains the caller's existing assignment;
// the SDK validates membership and the driver's current contribution validity.
class WheelkitDeviceAssignment final : public rex::input::DeviceAssignment {
 public:
  WheelkitDeviceAssignment(std::unique_ptr<rex::input::DeviceAssignment> routing,
                          rex::input::DeviceId wheel, bool enabled = false);
  void OnDevicesChanged(const std::vector<rex::input::DeviceInfo>& devices) override;
  void DevicesForUser(uint32_t user, std::vector<rex::input::DeviceId>& out) const override;
  rex::input::DeviceId PreferredDeviceForUser(uint32_t user) const override;

 private:
  std::unique_ptr<rex::input::DeviceAssignment> routing_;
  rex::input::DeviceId wheel_;
  bool enabled_{};
};

}  // namespace pinyon_shift::input
