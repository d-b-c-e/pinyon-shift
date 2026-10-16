#include "input/wheelkit_input_lifecycle.h"
#include "shim_fake_provider.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <vector>

using namespace pinyon_shift::input;
namespace sh = dbce::shim;
using namespace std::chrono_literals;
namespace {
int checks{};
void Check(bool value, const char* message) {
  ++checks;
  if (!value) throw std::runtime_error(message);
}
std::int64_t Now() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
const std::string Device = "{d71b8350-61b7-11f1-8001-444553540000}";
const std::string Product = "{0006346e-0000-0000-0000-504944564944}";
struct Script {
  std::atomic<bool> pressed{true}, wrong_input_thread{false};
  std::atomic<int> factories{}, reads{}, shutdowns{};
  std::mutex mutex;
  std::condition_variable cv;
  bool block_read{}, read_entered{}, release_read{};
  bool block_shutdown{}, shutdown_entered{}, release_shutdown{};
  std::thread::id owner;
};

// Lifetime/instrumentation facade only: all enumeration/open/read behavior is
// delegated to the existing scripted provider, not a new resolver/provider law.
class OwnedFake final : public sh::ProviderOwner {
 public:
  explicit OwnedFake(std::shared_ptr<Script> script) : script_(std::move(script)) {
    input_ = std::this_thread::get_id();
    script_->owner = input_;
    ++script_->factories;
    fake_.real_clock = true;
    sh::fake::Device wheel;
    wheel.listed = {Device, Product, "Scripted lifecycle wheel"};
    wheel.caps = sh::fake::wheel_caps();
    wheel.state.axes[0] = 32768;
    fake_.add(wheel);
  }
  std::vector<sh::DeviceInfo> enumerate() override { return fake_.enumerate(); }
  sh::OpenResult open(const sh::DeviceInfo& device) override {
    std::lock_guard lock(handles_mutex_);
    if (stopped_) return {};
    auto result = fake_.open(device);
    if (result.ok) handles_.push_back(result.handle);
    return result;
  }
  sh::Call reacquire(sh::Handle handle) override { return fake_.reacquire(handle); }
  void close(sh::Handle handle) override {
    std::lock_guard lock(handles_mutex_);
    fake_.close(handle);
    for (auto& entry : handles_) if (entry == handle) entry = 0;
  }
  sh::Call poll(sh::Handle handle) override {
    RecordThread();
    return fake_.poll(handle);
  }
  sh::Call state(sh::Handle handle, sh::RawState& out) override {
    RecordThread();
    {
      std::unique_lock lock(script_->mutex);
      if (script_->block_read) {
        script_->block_read = false;
        script_->read_entered = true;
        script_->cv.notify_all();
        script_->cv.wait(lock, [&] { return script_->release_read; });
      }
    }
    ++script_->reads;
    const auto result = fake_.state(handle, out);
    if (result == sh::Call::Ok) out.buttons[0] = script_->pressed ? 0x80 : 0;
    return result;
  }
  std::int64_t now_us() override { return fake_.now_us(); }
  sh::ProviderShutdownReport shutdown(int) override {
    {
      std::unique_lock lock(script_->mutex);
      script_->shutdown_entered = true;
      script_->cv.notify_all();
      if (script_->block_shutdown)
        script_->cv.wait(lock, [&] { return script_->release_shutdown; });
    }
    std::lock_guard lock(handles_mutex_);
    stopped_ = true;
    int released{};
    for (auto handle : handles_) {
      if (handle) { fake_.close(handle); ++released; }
    }
    handles_.clear();
    ++script_->shutdowns;
    script_->cv.notify_all();
    return {released, 0, true, "scripted provider released"};
  }
 private:
  void RecordThread() {
    if (std::this_thread::get_id() != input_) script_->wrong_input_thread = true;
  }
  std::shared_ptr<Script> script_;
  sh::fake::Provider fake_;
  std::thread::id input_;
  std::mutex handles_mutex_;
  bool stopped_{};
  std::vector<sh::Handle> handles_;
};

WheelkitLifecycleOptions Options(const std::string& name, bool background = false) {
  auto folder = std::filesystem::temp_directory_path() /
      ("fh1-lifecycle-" + std::to_string(GetCurrentProcessId())) / name;
  std::filesystem::create_directories(folder);
  return {folder.wstring(),
      "[Controls]\nSchema = 2\nshiftUp = button 0 dev=" + Device +
          " prod=" + Product + "\n", background};
}
auto Factory(std::shared_ptr<Script> script) {
  return [script] { return std::make_shared<OwnedFake>(script); };
}
float Shift(WheelkitInputLifecycle& lifecycle) {
  return lifecycle.Read(Now())->actions[sh::action_index("shiftUp")].value;
}
void Frame(WheelkitInputLifecycle& lifecycle, std::uint64_t& frame) {
  Check(lifecycle.NotifyFrame(++frame) && lifecycle.WaitForFrame(frame, 2s),
        "notified frame completes");
}
void Ready(WheelkitInputLifecycle& lifecycle, std::uint64_t& frame) {
  const auto until = std::chrono::steady_clock::now() + 2s;
  do {
    Frame(lifecycle, frame);
    const auto snapshot = lifecycle.Read(Now());
    auto status = snapshot->actions[sh::action_index("shiftUp")].status;
    if (status == sh::Status::Rearming || status == sh::Status::Live) return;
    // The shared host's existing device worker opens asynchronously. No adapter
    // timer is added; this fixture supplies another explicit frame notification.
    std::this_thread::yield();
  } while (std::chrono::steady_clock::now() < until);
  throw std::runtime_error("shared fake provider did not open");
}
bool WaitRead(std::shared_ptr<Script> script) {
  std::unique_lock lock(script->mutex);
  return script->cv.wait_for(lock, 2s, [&] { return script->read_entered; });
}
void UnblockRead(std::shared_ptr<Script> script) {
  std::lock_guard lock(script->mutex);
  script->release_read = true;
  script->cv.notify_all();
}

void TestFramesAndFocus() {
  auto script = std::make_shared<Script>();
  WheelkitInputLifecycle lifecycle(Options("focus"), Factory(script));
  Check(script->factories == 0, "no host/provider created without a frame");
  std::uint64_t frame{};
  Ready(lifecycle, frame);
  Check(Shift(lifecycle) == 0, "new Core rearms startup-held shift");
  script->pressed = false;
  Frame(lifecycle, frame);
  script->pressed = true;
  Frame(lifecycle, frame);
  Check(Shift(lifecycle) == 1, "Core release/repress becomes live");
  const auto ticks = lifecycle.Status().frame_ticks;
  const auto reads = script->reads.load();
  for (int i = 0; i < 20; ++i) (void)lifecycle.Read(Now());
  Check(script->reads == reads && !lifecycle.WaitForFrame(frame + 1, 20ms) &&
            lifecycle.Status().frame_ticks == ticks,
        "UI reads and elapsed time do not tick the provider");
  Check(!lifecycle.NotifyFrame(frame), "duplicate frame rejected");
  {
    std::lock_guard lock(script->mutex);
    script->block_read = true;
  }
  lifecycle.NotifyFrame(++frame);
  Check(WaitRead(script), "old epoch read held across focus transition");
  lifecycle.SetFocused(false);
  Check(lifecycle.Read(Now())->validity == SnapshotValidity::Unfocused &&
            Shift(lifecycle) == 0, "focus loss gates immediately");
  lifecycle.SetFocused(true);  // latch must survive this rapid pair
  Check(Shift(lifecycle) == 0, "rapid refocus cannot reopen delivery of blocked old epoch");
  UnblockRead(script);
  Ready(lifecycle, frame);
  Check(script->factories == 2 && Shift(lifecycle) == 0,
        "rapid refocus uses a new host/Core and held shift stays rearmed");
  script->pressed = false;
  Frame(lifecycle, frame);
  script->pressed = true;
  Frame(lifecycle, frame);
  Check(Shift(lifecycle) == 1, "refocused Core accepts release/repress");
  auto stopped = lifecycle.Stop(2s);
  Check(stopped.resolved() && stopped.status.host_stop &&
            stopped.status.host_stop->resolved && script->shutdowns == 2,
        "each host uses resolved shared bounded shutdown");
  Check(script->owner != std::this_thread::get_id() && !script->wrong_input_thread,
        "factory and every poll/state call belong to the adapter owner thread");
  Check(!lifecycle.NotifyFrame(++frame) && Shift(lifecycle) == 0 &&
            lifecycle.Read(Now())->validity == SnapshotValidity::Stopped,
        "stop rejects late frames and delivers neutral");
  Check(lifecycle.Stop(1ms).resolved(), "completed stop is repeatable");
}

void TestCoalescingAndBlockedTick() {
  auto script = std::make_shared<Script>();
  WheelkitInputLifecycle lifecycle(Options("blocked-tick"), Factory(script));
  std::uint64_t frame{};
  Ready(lifecycle, frame);
  const auto before = lifecycle.Status();
  {
    std::lock_guard lock(script->mutex);
    script->block_read = true;
  }
  lifecycle.NotifyFrame(++frame);
  Check(WaitRead(script), "scripted input call blocks on owner thread");
  lifecycle.NotifyFrame(++frame);
  lifecycle.NotifyFrame(++frame);
  UnblockRead(script);
  Check(lifecycle.WaitForFrame(frame, 2s) &&
            lifecycle.Status().frame_ticks == before.frame_ticks + 2 &&
            lifecycle.Status().coalesced_frames == before.coalesced_frames + 1,
        "one pending newest frame; no stale catch-up burst");
  {
    std::lock_guard lock(script->mutex);
    script->block_read = true;
    script->read_entered = false;
    script->release_read = false;
  }
  lifecycle.NotifyFrame(++frame);
  Check(WaitRead(script), "second scripted read is blocked");
  const auto start = std::chrono::steady_clock::now();
  auto stopped = lifecycle.Stop(25ms);
  Check(!stopped.owner_joined &&
            std::chrono::steady_clock::now() - start < 500ms && Shift(lifecycle) == 0,
        "blocked tick returns bounded unresolved stop and immediately neutralizes");
  UnblockRead(script);
  Check(lifecycle.Stop(2s).resolved() && script->shutdowns == 1,
        "late tick completes on retained state and shutdown result is collectible");
}

void TestBackgroundAndFailure() {
  auto script = std::make_shared<Script>();
  WheelkitInputLifecycle lifecycle(Options("background", true), Factory(script));
  std::uint64_t frame{};
  Ready(lifecycle, frame);
  lifecycle.SetFocused(false);
  Frame(lifecycle, frame);
  Check(script->factories == 1 &&
            lifecycle.Read(Now())->validity == SnapshotValidity::Live,
        "explicit background policy preserves existing host");
  Check(lifecycle.Stop(2s).resolved(), "background host stops resolved");
  WheelkitInputLifecycle failed(Options("factory-failure"), []()
      -> std::shared_ptr<sh::ProviderOwner> { throw std::runtime_error("factory failed"); });
  failed.NotifyFrame(1);
  Check(!failed.WaitForFrame(1, 2s), "factory exception does not complete a frame");
  auto result = failed.Stop(2s);
  Check(result.owner_joined && !result.resolved() &&
            result.status.failure == "factory failed" &&
            failed.Read(Now())->validity == SnapshotValidity::Stopped,
        "factory exception is reported with neutral joined owner");
}

void TestUnresolvedHost() {
  auto script = std::make_shared<Script>();
  script->block_shutdown = true;
  WheelkitInputLifecycle lifecycle(Options("unresolved-host"), Factory(script));
  std::uint64_t frame{};
  Ready(lifecycle, frame);
  lifecycle.SetFocused(false);
  lifecycle.SetFocused(true);
  Check(!lifecycle.WaitForFrame(frame + 1, 2s), "unresolved host ends owner loop");
  auto result = lifecycle.Stop(2s);
  Check(result.owner_joined && !result.resolved() && result.status.host_stop &&
            !result.status.host_stop->resolved && script->factories == 1 &&
            !lifecycle.NotifyFrame(++frame),
        "unresolved P21 shutdown prevents reopening another provider");
  {
    std::lock_guard lock(script->mutex);
    script->release_shutdown = true;
    script->cv.notify_all();
  }
  std::unique_lock lock(script->mutex);
  Check(script->cv.wait_for(lock, 2s, [&] { return script->shutdowns == 1; }),
        "late provider release completes on host-owned retained state");
}
}  // namespace

int main() {
  try {
    TestFramesAndFocus();
    TestCoalescingAndBlockedTick();
    TestBackgroundAndFailure();
    TestUnresolvedHost();
    std::printf("PASS wheelkit lifecycle: %d checks\n", checks);
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
  }
}
