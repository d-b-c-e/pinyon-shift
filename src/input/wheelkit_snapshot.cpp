#include "input/wheelkit_snapshot.h"

namespace pinyon_shift::input {
namespace {

void Neutralize(WheelkitSnapshot& snapshot) {
  for (auto& action : snapshot.actions) action.value = 0;
}

}  // namespace

void WheelkitSnapshotStore::Publish(const dbce::shim::Frame& frame,
                                    SnapshotValidity validity) {
  auto snapshot = std::make_shared<WheelkitSnapshot>();
  snapshot->sequence = frame.seq;
  snapshot->tick_us = frame.tick_us;
  snapshot->validity = validity;
  for (std::size_t i = 0; i < snapshot->actions.size(); ++i) {
    snapshot->actions[i] = {frame.action[i].value, frame.action[i].status};
  }
  for (auto& action : snapshot->actions) {
    // Preserve Core's per-action neutral/rearm decisions. An unavailable
    // shifter does not suppress a separately available steering axis.
    if (!dbce::shim::is_applied(action.status)) action.value = 0;
  }
  if (validity != SnapshotValidity::Live) Neutralize(*snapshot);

  std::lock_guard lock(mutex_);
  if (!stopped_) latest_ = std::move(snapshot);
}

std::shared_ptr<const WheelkitSnapshot> WheelkitSnapshotStore::Read(
    std::int64_t now_us) const {
  std::shared_ptr<const WheelkitSnapshot> snapshot;
  {
    std::lock_guard lock(mutex_);
    snapshot = latest_;
  }
  if (snapshot->validity != SnapshotValidity::Live) return snapshot;

  auto validity = SnapshotValidity::Live;
  // Unsigned subtraction avoids signed overflow for a malformed clock value.
  if (now_us < snapshot->tick_us) {
    validity = SnapshotValidity::InvalidClock;
  } else if (static_cast<std::uint64_t>(now_us) -
                 static_cast<std::uint64_t>(snapshot->tick_us) > kFreshnessUs) {
    validity = SnapshotValidity::Stale;
  }
  if (validity == SnapshotValidity::Live) return snapshot;
  auto neutral = std::make_shared<WheelkitSnapshot>(*snapshot);
  neutral->validity = validity;
  Neutralize(*neutral);
  return neutral;
}

void WheelkitSnapshotStore::Stop() {
  std::lock_guard lock(mutex_);
  auto neutral = std::make_shared<WheelkitSnapshot>(*latest_);
  neutral->validity = SnapshotValidity::Stopped;
  Neutralize(*neutral);
  stopped_ = true;
  latest_ = std::move(neutral);
}

}  // namespace pinyon_shift::input
