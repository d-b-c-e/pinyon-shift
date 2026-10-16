#include "input/wheelkit_snapshot.h"
#include "shim_fake_provider.hpp"

#include <atomic>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>

using namespace pinyon_shift::input;
namespace shim = dbce::shim;
namespace {
int checks = 0;
void Check(bool condition, const char* message) {
  ++checks;
  if (!condition) throw std::runtime_error(message);
}
std::size_t Action(const char* name) {
  return static_cast<std::size_t>(shim::action_index(name));
}
bool Neutral(const WheelkitSnapshot& snapshot) {
  for (const auto& action : snapshot.actions) {
    if (action.value != 0) return false;
  }
  return true;
}

void TestBoundary() {
  WheelkitSnapshotStore store;
  Check(store.Read(0)->validity == SnapshotValidity::NotStarted, "starts inert");
  shim::Frame frame;
  frame.seq = 1;
  frame.tick_us = 1'000'000;
  frame.action[Action("steer")] = {0.75f, shim::Status::Live};
  frame.action[Action("throttle")] = {1, shim::Status::Live};
  frame.action[Action("brake")] = {1, shim::Status::DeviceMissing};
  frame.action[Action("shiftUp")] = {1, shim::Status::Live};
  store.Publish(frame, SnapshotValidity::Live);
  auto held = store.Read(frame.tick_us);
  Check(held->actions[Action("steer")].value == 0.75f, "resolved steering copied");
  Check(held->actions[Action("brake")].value == 0 &&
            held->actions[Action("brake")].status == shim::Status::DeviceMissing,
        "unavailable action neutral without losing status");
  Check(store.Read(frame.tick_us) == held &&
            held->actions[Action("shiftUp")].value == 1,
        "repeated read preserves held shift; no edge consumption");
  Check(store.Read(frame.tick_us + 100'000)->validity == SnapshotValidity::Live,
        "100ms boundary stays fresh");
  auto stale = store.Read(frame.tick_us + 100'001);
  Check(stale->validity == SnapshotValidity::Stale && Neutral(*stale),
        "stale snapshot neutralizes every action");
  Check(stale->sequence == 1 && stale->tick_us == frame.tick_us,
        "neutral read keeps original sequence and timestamp");
  Check(store.Read(frame.tick_us - 1)->validity == SnapshotValidity::InvalidClock &&
            Neutral(*store.Read(std::numeric_limits<std::int64_t>::min())),
        "clock reversal fails neutral");
  Check(Neutral(*store.Read(std::numeric_limits<std::int64_t>::max())),
        "large clock age fails neutral without overflow");
  for (auto reason : {SnapshotValidity::Disconnected, SnapshotValidity::Unfocused}) {
    store.Publish(frame, reason);
    Check(store.Read(frame.tick_us)->validity == reason &&
              Neutral(*store.Read(frame.tick_us)), "owner validity forces neutral");
  }
  for (int i = 0; i < 100; ++i) {
    ++frame.seq;
    frame.action[Action("shiftUp")].value = 0;
    store.Publish(frame, SnapshotValidity::Live);
  }
  Check(held->sequence == 1 && held->actions[Action("shiftUp")].value == 1,
        "slow reader remains immutable after writer wraps many times");
  store.Stop();
  store.Publish(frame, SnapshotValidity::Live);
  Check(store.Read(frame.tick_us)->validity == SnapshotValidity::Stopped &&
            Neutral(*store.Read(frame.tick_us)), "late publication cannot revive stop");
}

void TestRealCore() {
  const std::string device = "{d71b8350-61b7-11f1-8001-444553540000}";
  const std::string product = "{0006346e-0000-0000-0000-504944564944}";
  const std::string identity = " dev=" + device + " prod=" + product;
  shim::fake::Provider provider;
  shim::fake::Device wheel;
  wheel.listed = {device, product, "Scripted wheel"};
  wheel.caps = shim::fake::wheel_caps();
  wheel.state.axes[0] = 32768;
  wheel.state.buttons[0] = 0x80;  // held at startup: Core must rearm
  provider.add(wheel);
  shim::CoreConfig config;
  config.missing_rescan_us = 0;
  shim::Core core(provider, config);
  auto profile = shim::read_profile(
      "[Controls]\nSchema = 2\n"
      "steer = axis 0" + identity + " range=0..65535 rest=32768 travel=+1\n"
      "throttle = axis 2" + identity + " range=0..65535 rest=0 travel=+1\n"
      "shiftUp = button 0" + identity + "\n", 2);
  Check(profile.ok && profile.section.error.empty(), "real shared schema-2 reader");
  core.set_profile(profile);
  WheelkitSnapshotStore store;
  auto tick = [&] {
    core.service_devices();
    provider.advance(10'000);
    const auto& frame = core.tick();
    store.Publish(frame, SnapshotValidity::Live);
    return store.Read(provider.now_us());
  };
  auto first = tick();
  Check(first->actions[Action("shiftUp")].value == 0, "Core startup held input rearmed");
  provider.at(0).state.buttons[0] = 0;
  tick();
  provider.at(0).state.buttons[0] = 0x80;
  provider.at(0).state.axes[0] = 65535;
  provider.at(0).state.axes[2] = 65535;
  auto pressed = tick();
  Check(pressed->actions[Action("shiftUp")].value == 1 &&
            pressed->actions[Action("steer")].value == 1 &&
            pressed->actions[Action("throttle")].value == 1,
        "real provider resolved levels and axes cross boundary");
  auto continued = tick();
  Check(continued->actions[Action("shiftUp")].value == 1,
        "held level survives later Core frame without another down edge");
  provider.at(0).state.buttons[0] = 0;
  Check(tick()->actions[Action("shiftUp")].value == 0, "release copied");
  provider.at(0).script.push_back({shim::Call::Gone, shim::Call::NotCalled});
  Check(Neutral(*tick()), "device loss yields Core neutral levels");
  provider.at(0).state.buttons[0] = 0x80;
  tick();
  Check(tick()->actions[Action("shiftUp")].value == 0,
        "Core reconnect keeps held shift rearmed");
  provider.at(0).state.buttons[0] = 0;
  tick();
  provider.at(0).state.buttons[0] = 0x80;
  Check(tick()->actions[Action("shiftUp")].value == 1,
        "reconnect release then repress is delivered");
  Check(provider.misuse == 0, "no fake provider handle misuse");
}

void TestConcurrentReaders() {
  WheelkitSnapshotStore store;
  // Even if one thread runs to completion before the other is scheduled, the
  // reader must inspect a live frame rather than pass on NotStarted alone.
  shim::Frame initial;
  initial.tick_us = 1'000'000;
  store.Publish(initial, SnapshotValidity::Live);
  std::atomic<bool> finished{false};
  std::atomic<bool> coherent{true};
  std::thread reader([&] {
    do {
      auto snapshot = store.Read(1'000'000);
      if (snapshot->validity == SnapshotValidity::Live) {
        const float expected = snapshot->sequence % 2 ? 1.0f : 0.0f;
        for (const auto& action : snapshot->actions) {
          if (action.value != expected) coherent = false;
        }
      }
    } while (!finished.load());
  });
  for (std::uint64_t seq = 1; seq <= 1000; ++seq) {
    shim::Frame frame;
    frame.seq = seq;
    frame.tick_us = 1'000'000;
    for (auto& action : frame.action) {
      action.value = seq % 2 ? 1.0f : 0.0f;
      action.status = shim::Status::Live;
    }
    store.Publish(frame, SnapshotValidity::Live);
  }
  finished = true;
  reader.join();
  Check(coherent, "reader sees one complete frame across concurrent publications");
}
}  // namespace

int main() {
  try {
    TestBoundary();
    TestRealCore();
    TestConcurrentReaders();
    std::printf("PASS wheelkit snapshot: %d checks\n", checks);
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
  }
}
