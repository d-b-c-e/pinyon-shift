#include "input/wheelkit_device_assignment.h"

#include <stdexcept>
#include <utility>

namespace pinyon_shift::input {

WheelkitDeviceAssignment::WheelkitDeviceAssignment(
    std::unique_ptr<rex::input::DeviceAssignment> routing,
    rex::input::DeviceId wheel, bool enabled)
    : routing_(std::move(routing)), wheel_(wheel), enabled_(enabled) {
  if (!routing_) throw std::invalid_argument("routing assignment required");
}

void WheelkitDeviceAssignment::OnDevicesChanged(
    const std::vector<rex::input::DeviceInfo>& devices) {
  routing_->OnDevicesChanged(devices);
}

void WheelkitDeviceAssignment::DevicesForUser(
    uint32_t user, std::vector<rex::input::DeviceId>& out) const {
  routing_->DevicesForUser(user, out);
}

rex::input::DeviceId WheelkitDeviceAssignment::PreferredDeviceForUser(uint32_t user) const {
  return enabled_ && user == 0 ? wheel_ : routing_->PreferredDeviceForUser(user);
}

}  // namespace pinyon_shift::input
