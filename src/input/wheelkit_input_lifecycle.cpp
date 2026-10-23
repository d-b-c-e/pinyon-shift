#include "input/wheelkit_input_lifecycle.h"

#include <condition_variable>
#include <stdexcept>
#include <utility>

#include "provider_host.hpp"

namespace pinyon_shift::input {
namespace hosting = dbce::shim::hosting;
namespace {
struct NoCapture {
  void finish() {}
  struct Drained { hosting::RecOutcome outcome; bool joined; };
  Drained drain(hosting::Clock::time_point) {
    return {hosting::RecOutcome::Complete, true};
  }
};
using Host = hosting::ProviderHost<NoCapture>;
}  // namespace

struct WheelkitInputLifecycle::State {
  WheelkitLifecycleOptions options;
  ProviderFactory factory;
  mutable std::mutex mutex;
  std::condition_variable cv;
  bool stopping{}, focused{true}, reset_required{};
  std::optional<std::uint64_t> pending;
  std::uint64_t last_received{}, focus_generation{};
  std::shared_ptr<WheelkitSnapshotStore> snapshots =
      std::make_shared<WheelkitSnapshotStore>();
  WheelkitLifecycleStatus status;
};

WheelkitInputLifecycle::WheelkitInputLifecycle(WheelkitLifecycleOptions options,
                                               ProviderFactory factory)
    : state_(std::make_shared<State>()) {
  if (!factory) throw std::invalid_argument("provider factory required");
  state_->options = std::move(options);
  state_->factory = std::move(factory);
  owner_ = std::thread(&WheelkitInputLifecycle::Run, state_);
}

WheelkitInputLifecycle::~WheelkitInputLifecycle() {
  RequestStop();
  // No unbounded join/destruction of a provider still inside a call. The worker
  // retains State, the host and provider until their own bounded teardown ends.
  if (owner_.joinable()) owner_.detach();
}

bool WheelkitInputLifecycle::NotifyFrame(std::uint64_t frame) {
  std::lock_guard lock(state_->mutex);
  if (state_->stopping || state_->status.done || !state_->status.failure.empty() ||
      frame <= state_->last_received)
    return false;
  state_->last_received = frame;
  if (!state_->focused) return true;  // no background catch-up
  if (state_->pending) ++state_->status.coalesced_frames;
  state_->pending = frame;
  state_->cv.notify_all();
  return true;
}

void WheelkitInputLifecycle::SetFocused(bool focused) {
  std::lock_guard lock(state_->mutex);
  if (state_->stopping || state_->status.done) return;
  focused = focused || state_->options.allow_background;
  if (state_->focused == focused) return;
  state_->focused = focused;
  ++state_->focus_generation;
  if (!focused) {
    state_->reset_required = true;
    state_->pending.reset();
    state_->snapshots->Stop();
  }
  state_->cv.notify_all();
}

std::shared_ptr<const WheelkitSnapshot> WheelkitInputLifecycle::Read(
    std::int64_t now_us) const {
  std::lock_guard lock(state_->mutex);
  auto snapshot = state_->snapshots->Read(now_us);
  if (state_->options.admission && !state_->options.admission()) {
    auto neutral = std::make_shared<WheelkitSnapshot>(*snapshot);
    neutral->validity = SnapshotValidity::Disconnected;
    for (auto& action : neutral->actions) action.value = 0;
    return neutral;
  }
  if (state_->focused || state_->stopping) return snapshot;
  auto neutral = std::make_shared<WheelkitSnapshot>(*snapshot);
  neutral->validity = SnapshotValidity::Unfocused;
  for (auto& action : neutral->actions) action.value = 0;
  return neutral;
}

WheelkitLifecycleStatus WheelkitInputLifecycle::Status() const {
  std::lock_guard lock(state_->mutex);
  return state_->status;
}

bool WheelkitInputLifecycle::WaitForFrame(
    std::uint64_t frame, std::chrono::milliseconds timeout) const {
  std::unique_lock lock(state_->mutex);
  state_->cv.wait_for(lock, timeout, [&] {
    return state_->status.last_frame >= frame || state_->status.done;
  });
  return state_->status.last_frame >= frame;
}

bool WheelkitInputLifecycle::WaitPrepared(std::chrono::milliseconds timeout) const {
  std::unique_lock lock(state_->mutex);
  state_->cv.wait_for(lock, timeout, [&] {
    return state_->status.prepared || state_->status.done;
  });
  return state_->status.prepared && !state_->stopping && state_->status.failure.empty();
}

void WheelkitInputLifecycle::RequestStop() {
  std::lock_guard lock(state_->mutex);
  state_->stopping = true;
  state_->pending.reset();
  state_->snapshots->Stop();
  state_->cv.notify_all();
}

WheelkitLifecycleStop WheelkitInputLifecycle::Stop(
    std::chrono::milliseconds timeout) {
  std::lock_guard stop_lock(stop_mutex_);
  const auto deadline = hosting::Clock::now() + timeout;
  RequestStop();
  const bool joined = hosting::join_by(owner_, deadline);
  return {joined, Status()};
}

void WheelkitInputLifecycle::Run(std::shared_ptr<State> state) {
  std::unique_ptr<Host> host;
  std::shared_ptr<dbce::shim::ProviderOwner> prepared;
  // All host entry points, including final tick/stop/destruction, stay here.
  auto close_host = [&] {
    if (!host) return true;
    try {
      host->close_control_admission();
      (void)host->tick();  // shared host's final input-thread drain
    } catch (...) {
      std::lock_guard lock(state->mutex);
      state->status.failure = "final host tick failed";
    }
    const auto report = host->stop();  // existing P21 per-phase budgets
    host.reset();
    std::lock_guard lock(state->mutex);
    state->status.host_stop = report;
    if (!report.resolved) state->status.failure = "shared host stop unresolved";
    return report.resolved && state->status.failure.empty();
  };
  try {
    if (state->options.prepare_on_start) {
      prepared = state->factory();
      if (!prepared) throw std::runtime_error("provider preparation failed");
      std::lock_guard lock(state->mutex);
      state->status.prepared = true;
      state->cv.notify_all();
    }
    for (;;) {
      std::unique_lock lock(state->mutex);
      state->cv.wait(lock, [&] {
        return state->stopping || state->reset_required ||
               (state->focused && state->pending.has_value());
      });
      if (state->stopping) break;
      if (state->reset_required) {
        state->reset_required = false;
        lock.unlock();
        if (!close_host()) break;  // never reopen over an unresolved host
        continue;
      }
      const auto frame = *state->pending;
      const auto generation = state->focus_generation;
      state->pending.reset();
      lock.unlock();
      if (state->options.admission && !state->options.admission())
        throw std::runtime_error("native identity admission invalidated");
      if (!host) {
        auto provider = prepared ? std::move(prepared) : state->factory();
        host = std::make_unique<Host>(state->options.report_folder, "fh1-input",
            std::move(provider), state->options.profile_bytes, false,
            std::make_unique<NoCapture>(), dbce::shim::Capability{},
            WheelkitSnapshotStore::kFreshnessUs, 2);
        lock.lock();
        ++state->status.hosts_started;
        if (!state->stopping && generation == state->focus_generation)
          state->snapshots = std::make_shared<WheelkitSnapshotStore>();
        lock.unlock();
      }
      const auto& resolved = host->tick();
      lock.lock();
      ++state->status.frame_ticks;
      state->status.last_frame = frame;
      // A loss/refocus racing a read cannot publish from the old Core epoch.
      if (!state->stopping && state->focused &&
          generation == state->focus_generation && !state->reset_required)
        state->snapshots->Publish(resolved, SnapshotValidity::Live);
      state->cv.notify_all();
    }
  } catch (const std::exception& error) {
    std::lock_guard lock(state->mutex);
    state->status.failure = error.what();
  } catch (...) {
    std::lock_guard lock(state->mutex);
    state->status.failure = "input owner failed";
  }
  {
    std::lock_guard lock(state->mutex);
    state->snapshots->Stop();
  }
  try {
    (void)close_host();
    if (prepared) {
      const auto report = prepared->shutdown(100);
      if (!report.input_released || report.held) {
        std::lock_guard lock(state->mutex);
        state->status.failure = "prepared provider stop unresolved";
      }
      prepared.reset();
    }
  } catch (...) {
    std::lock_guard lock(state->mutex);
    state->status.failure = "shared host stop threw";
  }
  {
    std::lock_guard lock(state->mutex);
    state->status.done = true;
  }
  state->cv.notify_all();
}

}  // namespace pinyon_shift::input
