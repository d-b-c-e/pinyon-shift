#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "host_lifecycle.hpp"
#include "input/wheelkit_snapshot.h"

namespace pinyon_shift::input {

struct WheelkitLifecycleOptions {
  std::wstring report_folder;  // caller-owned scratch directory; must exist
  std::string profile_bytes;
  bool allow_background{};
};

struct WheelkitLifecycleStatus {
  std::uint64_t hosts_started{}, frame_ticks{}, last_frame{}, coalesced_frames{};
  bool done{};
  std::optional<dbce::shim::hosting::LifetimeReport> host_stop;
  std::string failure;
};

struct WheelkitLifecycleStop {
  bool owner_joined{};
  WheelkitLifecycleStatus status;
  bool resolved() const {
    return owner_joined && status.failure.empty() &&
           (!status.host_stop || status.host_stop->resolved);
  }
};

class WheelkitInputLifecycle {
 public:
  // Called only on the adapter's input thread, once for each host epoch.
  // The returned provider is owned by the existing shared in-process host.
  using ProviderFactory =
      std::function<std::shared_ptr<dbce::shim::ProviderOwner>()>;
  WheelkitInputLifecycle(WheelkitLifecycleOptions options, ProviderFactory factory);
  ~WheelkitInputLifecycle();
  WheelkitInputLifecycle(const WheelkitInputLifecycle&) = delete;
  WheelkitInputLifecycle& operator=(const WheelkitInputLifecycle&) = delete;

  // Unique increasing host-frame IDs. One pending slot keeps the newest frame;
  // overwritten notifications are counted, never replayed as a catch-up burst.
  bool NotifyFrame(std::uint64_t frame);
  // Immediate delivery gate. A focus-loss latch survives a rapid loss/refocus
  // pair and requires a resolved host restart before any new live publication.
  void SetFocused(bool focused);
  std::shared_ptr<const WheelkitSnapshot> Read(std::int64_t now_us) const;
  WheelkitLifecycleStatus Status() const;
  bool WaitForFrame(std::uint64_t frame, std::chrono::milliseconds timeout) const;

  // Neutralizes immediately, then waits for the input thread against one bound.
  // A timed-out thread keeps all its own state; retry Stop to collect its result.
  WheelkitLifecycleStop Stop(std::chrono::milliseconds timeout);

 private:
  struct State;
  static void Run(std::shared_ptr<State> state);
  void RequestStop();
  std::shared_ptr<State> state_;
  std::thread owner_;
  std::mutex stop_mutex_;
};

}  // namespace pinyon_shift::input
