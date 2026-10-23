#pragma once

#include <cmath>
#include <string_view>

#include "input/wheelkit_snapshot.h"

namespace pinyon_shift::input {

inline constexpr const char* kDirectGearActions[] = {
    "reverse", "neutral", "gear1", "gear2", "gear3", "gear4",
    "gear5", "gear6", "gear7", "gear8"};
inline constexpr int16_t kNoDirectGear = 10;

inline bool HasDirectGearProfile(const dbce::shim::Profile& profile,
                                 const dbce::shim::ProfilePlan& plan) {
  if (!plan.usable || profile.section.transmission != "H-pattern") return false;
  bool gate = false;
  for (const auto* name : kDirectGearActions) {
    const int i = dbce::shim::action_index(name);
    if (plan.base[i] == dbce::shim::Status::Invalid) return false;
    if (std::string_view(name) != "neutral" && plan.bound_of[i] >= 0) gate = true;
  }
  return gate;
}

// Raw rightX: -1 reverse, 0 neutral, 1..8 gears, >=9 no direct override.
inline int16_t DirectGear(const WheelkitSnapshot& snapshot) {
  if (snapshot.validity != SnapshotValidity::Live) return kNoDirectGear;
  bool gate = false, held = false;
  int16_t gear = 0;
  for (int i = 0; i < 10; ++i) {
    const auto& action = snapshot.actions[dbce::shim::action_index(kDirectGearActions[i])];
    if (action.status == dbce::shim::Status::Unbound) continue;
    if (action.status != dbce::shim::Status::Live || !std::isfinite(action.value) ||
        action.value < 0.0f || action.value > 1.0f) return kNoDirectGear;
    gate |= i != 1;
    if (action.value >= 0.5f) {
      if (held) return kNoDirectGear;
      held = true;
      gear = int16_t(i - 1);
    }
  }
  // An available, released physical gate is neutral; missing input is not.
  return gate ? gear : kNoDirectGear;
}

}  // namespace pinyon_shift::input
