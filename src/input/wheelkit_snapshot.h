#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>

#include "shim.hpp"

namespace pinyon_shift::input {

// The input-thread owner supplies focus/lifecycle validity. This boundary never
// opens a provider, resolves bindings, generates edges, or implements rearm.
enum class SnapshotValidity {
  NotStarted,
  Live,
  Disconnected,
  Unfocused,
  Stopped,
  Stale,
  InvalidClock,
};

struct ResolvedAction {
  float value{};
  dbce::shim::Status status{dbce::shim::Status::Unbound};
};

struct WheelkitSnapshot {
  std::uint64_t sequence{};
  std::int64_t tick_us{};
  SnapshotValidity validity{SnapshotValidity::NotStarted};
  std::array<ResolvedAction, dbce::shim::ActionCount> actions{};
};

class WheelkitSnapshotStore {
 public:
  static constexpr std::int64_t kFreshnessUs = 100'000;

  // One input-thread writer, once per Core tick. Core owns the action statuses
  // and held levels. Caller validity may further suppress, never enable, them.
  void Publish(const dbce::shim::Frame& frame, SnapshotValidity validity);

  // Guest/UI readers pass now in the provider's monotonic microsecond domain.
  // A retained pointer stays immutable across arbitrarily many publications.
  std::shared_ptr<const WheelkitSnapshot> Read(std::int64_t now_us) const;

  // Terminal for this store instance. A late/in-flight publisher cannot revive
  // it. A new adapter session needs a new store and Core.
  void Stop();

 private:
  mutable std::mutex mutex_;
  std::shared_ptr<const WheelkitSnapshot> latest_ =
      std::make_shared<const WheelkitSnapshot>();
  bool stopped_{};
};

}  // namespace pinyon_shift::input
