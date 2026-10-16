#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_message.hpp>

#include <atomic>
#include <algorithm>
#include <cstring>
#include <filesystem>

#include <rex/cvar.h>
#include <rex/input/input_system.h>
#include <rex/ui/presenter.h>

#include "input/wheelkit_input_driver.h"
#include "input/wheelkit_device_assignment.h"
#include "input/wheelkit_app_input.h"
#include "shim_fake_provider.hpp"

REXCVAR_DECLARE(std::string, pad_remap);
REXCVAR_DECLARE(std::string, pad_chord_debug_overlay);
REXCVAR_DECLARE(std::string, pad_chord_game_menu);
REXCVAR_DECLARE(int32_t, pad_rumble_strength);

namespace {
using namespace pinyon_shift::input;
namespace ri = rex::input;
namespace sh = dbce::shim;
using rex::X_RESULT;
using rex::X_STATUS;
using namespace std::chrono_literals;
const std::string Device = "{d71b8350-61b7-11f1-8001-444553540000}";
const std::string Product = "{0006346e-0000-0000-0000-504944564944}";
const char* Buttons[] = {"clutch", "handbrake", "shiftUp", "shiftDown", "confirm",
    "back", "start", "navUp", "navDown", "navLeft", "navRight", "camera",
    "lookBack", "select", "neutral", "reverse", "reset", "horn", "modMenu",
    "muteFfb", "gear1", "gear2", "gear3", "gear4", "gear5", "gear6", "gear7", "gear8"};

std::int64_t Now() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
std::string Profile() {
  const std::string identity = " dev=" + Device + " prod=" + Product;
  std::string text = "[Controls]\nSchema = 2\n"
      "steer = axis 0" + identity +
      " range=0..65535 rest=32768 travel=+1 cal=0..32768..65535\n"
      "throttle = axis 2" + identity + " range=0..65535 rest=0 travel=+1\n"
      "brake = axis 5" + identity + " range=0..65535 rest=0 travel=+1\n";
  for (size_t i = 0; i < std::size(Buttons); ++i)
    text += std::string(Buttons[i]) + " = button " + std::to_string(i) + identity + "\n";
  return text;
}

// Ownership facade only; the existing shared fake provider does every read.
class FakeOwner final : public sh::ProviderOwner {
 public:
  explicit FakeOwner(std::shared_ptr<sh::fake::Provider> fake, bool unresolved = false)
      : fake_(std::move(fake)), unresolved_(unresolved) {}
  std::vector<sh::DeviceInfo> enumerate() override { return fake_->enumerate(); }
  sh::OpenResult open(const sh::DeviceInfo& device) override {
    std::lock_guard lock(mutex_);
    if (stopped_) return {};
    auto result = fake_->open(device);
    if (result.ok) handles_.push_back(result.handle);
    return result;
  }
  sh::Call reacquire(sh::Handle handle) override { return fake_->reacquire(handle); }
  void close(sh::Handle handle) override {
    std::lock_guard lock(mutex_);
    fake_->close(handle);
    for (auto& owned : handles_) if (owned == handle) owned = 0;
  }
  sh::Call poll(sh::Handle handle) override { return fake_->poll(handle); }
  sh::Call state(sh::Handle handle, sh::RawState& out) override { return fake_->state(handle, out); }
  std::int64_t now_us() override { return fake_->now_us(); }
  sh::ProviderShutdownReport shutdown(int) override {
    std::lock_guard lock(mutex_);
    stopped_ = true;
    int released{};
    for (auto handle : handles_) if (handle) { fake_->close(handle); ++released; }
    handles_.clear();
    return {released, unresolved_ ? 1 : 0, !unresolved_, "fake input released"};
  }
 private:
  std::shared_ptr<sh::fake::Provider> fake_;
  std::mutex mutex_;
  bool stopped_{};
  bool unresolved_{};
  std::vector<sh::Handle> handles_;
};

struct Rig {
  std::shared_ptr<sh::fake::Provider> fake = std::make_shared<sh::fake::Provider>();
  std::shared_ptr<WheelkitInputLifecycle> lifecycle;
  std::shared_ptr<std::atomic<std::int64_t>> clock_offset =
      std::make_shared<std::atomic<std::int64_t>>(0);
  ri::InputSystem system{nullptr};
  WheelkitInputDriver* driver{};
  ri::DeviceId id{};
  std::uint64_t frame{};
  explicit Rig(std::string profile = Profile(), int initial_throttle = 0,
               std::unique_ptr<ri::InputDriver> before_wheel = {}) {
    fake->real_clock = true;
    sh::fake::Device wheel;
    wheel.listed = {Device, Product, "Driver fixture wheel"};
    wheel.caps = sh::fake::wheel_caps();
    wheel.state.axes[0] = 32768;
    wheel.state.axes[2] = initial_throttle;
    fake->add(wheel);
    const auto folder = std::filesystem::temp_directory_path() /
        ("fh1-driver-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(folder);
    lifecycle = std::make_shared<WheelkitInputLifecycle>(
        WheelkitLifecycleOptions{folder.wstring(), std::move(profile)},
        [provider = fake] { return std::make_shared<FakeOwner>(provider); });
    auto owned = std::make_unique<WheelkitInputDriver>(lifecycle,
        [offset = clock_offset] { return Now() + offset->load(); });
    driver = owned.get();
    std::vector<ri::DeviceInfo> devices;
    driver->EnumerateDevices(devices);
    REQUIRE(devices.size() == 1);
    REQUIRE(devices[0].synthetic);
    id = devices[0].id;
    if (before_wheel) system.AddDriver(std::move(before_wheel));
    system.AddDriver(std::move(owned));
    system.SetDeviceAssignment(std::make_unique<ri::SharedAssignment>());
  }
  ~Rig() { (void)lifecycle->Stop(2s); }
  void Tick() {
    REQUIRE(lifecycle->NotifyFrame(++frame));
    REQUIRE(lifecycle->WaitForFrame(frame, 2s));
  }
  void Ready() {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline) {
      Tick();
      if (lifecycle->Read(Now())->actions[sh::action_index("steer")].status == sh::Status::Live)
        return;
      std::this_thread::yield();
    }
    FAIL("shared fake wheel did not open");
  }
  ri::X_INPUT_STATE Read() {
    ri::X_INPUT_STATE state{};
    REQUIRE(system.GetState(0, &state) == X_ERROR_SUCCESS);
    return state;
  }
  void SetButtons(std::initializer_list<size_t> buttons) {
    std::lock_guard lock(fake->m);
    std::memset(fake->at(0).state.buttons, 0, sizeof(fake->at(0).state.buttons));
    for (auto index : buttons) fake->at(0).state.buttons[index] = 0x80;
  }
};

class OrdinaryFakeDriver : public ri::InputDriver {
 public:
  OrdinaryFakeDriver(bool keyboard, X_RESULT rumble) : InputDriver(nullptr, 0),
      keyboard_(keyboard), rumble_(rumble) { gamepad.buttons = ri::X_INPUT_GAMEPAD_A; }
  X_STATUS Setup() override { return X_STATUS_SUCCESS; }
  bool is_keyboard_and_mouse() const override { return keyboard_; }
  void EnumerateDevices(std::vector<ri::DeviceInfo>& out) override {
    ri::DeviceInfo device;
    device.id = id;
    device.synthetic = keyboard_;
    out.push_back(device);
  }
  X_RESULT GetDeviceState(ri::DeviceId, ri::X_INPUT_STATE* out) override {
    *out = {};
    out->gamepad = gamepad;
    return X_ERROR_SUCCESS;
  }
  X_RESULT GetDeviceCapabilities(ri::DeviceId, uint32_t, ri::X_INPUT_CAPABILITIES* out) override {
    *out = {};
    out->sub_type = subtype;  // distinguish ordinary selection from Wheelkit
    return X_ERROR_SUCCESS;
  }
  X_RESULT SetDeviceVibration(ri::DeviceId, ri::X_INPUT_VIBRATION* vibration) override {
    ++vibration_calls;
    if (vibration) last_vibration = *vibration;
    return rumble_;
  }
  X_RESULT GetDeviceKeystroke(ri::DeviceId, uint32_t, ri::X_INPUT_KEYSTROKE*) override {
    return X_ERROR_EMPTY;
  }
  ri::DeviceId id{static_cast<ri::DeviceId>(51)};
  ri::X_INPUT_GAMEPAD gamepad{};
  uint8_t subtype{7};
  int vibration_calls{};
  ri::X_INPUT_VIBRATION last_vibration{};
 private:
  bool keyboard_{};
  X_RESULT rumble_{};
};

struct SettingsGuard {
  std::string remap = REXCVAR_GET(pad_remap);
  std::string debug = REXCVAR_GET(pad_chord_debug_overlay);
  std::string menu = REXCVAR_GET(pad_chord_game_menu);
  int32_t rumble = REXCVAR_GET(pad_rumble_strength);
  ~SettingsGuard() {
    REXCVAR_SET(pad_remap, remap);
    REXCVAR_SET(pad_chord_debug_overlay, debug);
    REXCVAR_SET(pad_chord_game_menu, menu);
    REXCVAR_SET(pad_rumble_strength, rumble);
  }
};
bool Neutral(const ri::X_INPUT_GAMEPAD& pad) {
  const ri::X_INPUT_GAMEPAD zero{};
  return std::memcmp(&pad, &zero, sizeof(pad)) == 0;
}
}  // namespace

TEST_CASE("Wheelkit driver composes Core lifecycle snapshots with actual InputSystem", "[wheelkit-driver]") {
  SettingsGuard restore;
  REXCVAR_SET(pad_remap, "A=Y,B=X,X=B,LB=RB,RB=LB,LT=RT,RT=LT");
  REXCVAR_SET(pad_chord_debug_overlay, "LS+RS");
  REXCVAR_SET(pad_chord_game_menu, "BACK+START");
  REXCVAR_SET(pad_rumble_strength, 50);
  Rig rig;
  REQUIRE(rig.driver->Setup() == X_STATUS_SUCCESS);
  REQUIRE(rig.driver->resolves_own_actions());
  REQUIRE_FALSE(rig.driver->is_keyboard_and_mouse());
  REQUIRE(Neutral(rig.Read().gamepad));  // not started
  rig.Ready();

  const uint16_t expected[] = {ri::X_INPUT_GAMEPAD_LEFT_SHOULDER, ri::X_INPUT_GAMEPAD_A,
      ri::X_INPUT_GAMEPAD_B, ri::X_INPUT_GAMEPAD_X, ri::X_INPUT_GAMEPAD_A,
      ri::X_INPUT_GAMEPAD_B, ri::X_INPUT_GAMEPAD_START, ri::X_INPUT_GAMEPAD_DPAD_UP,
      ri::X_INPUT_GAMEPAD_DPAD_DOWN, ri::X_INPUT_GAMEPAD_DPAD_LEFT,
      ri::X_INPUT_GAMEPAD_DPAD_RIGHT, ri::X_INPUT_GAMEPAD_RIGHT_SHOULDER};
  for (size_t i = 0; i < std::size(expected); ++i) {
    INFO("mapped action: " << Buttons[i]);
    rig.SetButtons({i});
    rig.Tick();
    REQUIRE(uint16_t(rig.Read().gamepad.buttons) == expected[i]);
  }
  rig.SetButtons({2});
  rig.Tick();
  const auto held = rig.Read();
  REQUIRE(uint16_t(held.gamepad.buttons) == ri::X_INPUT_GAMEPAD_B);
  int polls{};
  { std::lock_guard lock(rig.fake->m); polls = rig.fake->at(0).polls; }
  for (int i = 0; i < 8; ++i) {
    const auto repeated = rig.Read();
    REQUIRE(uint16_t(repeated.gamepad.buttons) == ri::X_INPUT_GAMEPAD_B);
    REQUIRE(uint32_t(repeated.packet_number) == uint32_t(held.packet_number));
  }
  { std::lock_guard lock(rig.fake->m); REQUIRE(rig.fake->at(0).polls == polls); }
  rig.Tick();
  REQUIRE(uint16_t(rig.Read().gamepad.buttons) == ri::X_INPUT_GAMEPAD_B);

  // All unsupported/unknown actions are really resolved by Core but cannot
  // leak a guessed guest button, including host settings/modMenu.
  rig.SetButtons({12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27});
  rig.Tick();
  REQUIRE(rig.lifecycle->Read(Now())->actions[sh::action_index("lookBack")].value == 1);
  REQUIRE(Neutral(rig.Read().gamepad));
  rig.SetButtons({});
  {
    std::lock_guard lock(rig.fake->m);
    rig.fake->at(0).state.axes[0] = 65535;
    rig.fake->at(0).state.axes[2] = 65535;
    rig.fake->at(0).state.axes[5] = 65535;
  }
  rig.Tick();
  auto axes = rig.Read();
  REQUIRE(int16_t(axes.gamepad.thumb_lx) == 32767);
  REQUIRE(axes.gamepad.left_trigger == 255);
  REQUIRE(axes.gamepad.right_trigger == 255);
  {
    std::lock_guard lock(rig.fake->m);
    rig.fake->at(0).state.axes[0] = 0;
    rig.fake->at(0).state.axes[2] = 32768;
    rig.fake->at(0).state.axes[5] = 16384;
  }
  rig.Tick();
  axes = rig.Read();
  REQUIRE(int16_t(axes.gamepad.thumb_lx) == -32768);
  REQUIRE(axes.gamepad.right_trigger == 128);
  REQUIRE(axes.gamepad.left_trigger == 64);
  rig.clock_offset->store(200'000);
  auto stale = rig.Read();
  REQUIRE(Neutral(stale.gamepad));
  REQUIRE(uint32_t(stale.packet_number) != uint32_t(axes.packet_number));
  rig.clock_offset->store(0);
  rig.Tick();
  REQUIRE_FALSE(Neutral(rig.Read().gamepad));
  rig.system.SetActiveCallback([] { return false; });
  REQUIRE(Neutral(rig.Read().gamepad));
  rig.system.SetActiveCallback({});

  ri::X_INPUT_CAPABILITIES caps{};
  REQUIRE(rig.system.GetCapabilities(0, 0, &caps) == X_ERROR_SUCCESS);
  REQUIRE(caps.type == ri::XINPUT_DEVTYPE_GAMEPAD);
  REQUIRE(caps.sub_type == 1);
  REQUIRE(uint16_t(caps.flags) == 0);
  REQUIRE(uint16_t(caps.vibration.left_motor_speed) == 0);
  ri::X_INPUT_VIBRATION vibration{};
  vibration.left_motor_speed = 40000;
  vibration.right_motor_speed = 20000;
  REQUIRE(rig.system.SetState(0, &vibration) == X_ERROR_SUCCESS);
  REQUIRE(rig.driver->SetDeviceVibration(rig.id, nullptr) == X_ERROR_SUCCESS);
  REQUIRE(rig.driver->GetDeviceKeystroke(rig.id, 0, nullptr) == X_ERROR_EMPTY);
  REQUIRE(rig.driver->GetDeviceState(ri::DeviceId::kInvalid, nullptr) == X_ERROR_DEVICE_NOT_CONNECTED);
  REQUIRE(rig.driver->SetDeviceVibration(ri::DeviceId::kInvalid, &vibration) == X_ERROR_DEVICE_NOT_CONNECTED);

  // Existing pad remapping/keyboard and synthetic-rumble rules stay in force.
  {
    std::lock_guard lock(rig.fake->m);
    rig.fake->at(0).state.axes[0] = 32768;
    rig.fake->at(0).state.axes[2] = rig.fake->at(0).state.axes[5] = 0;
  }
  rig.Tick();
  auto ordinary = std::make_unique<OrdinaryFakeDriver>(false, X_ERROR_DEVICE_NOT_CONNECTED);
  auto* pad = ordinary.get();
  rig.system.AddDriver(std::move(ordinary));
  REQUIRE(uint16_t(rig.Read().gamepad.buttons) == ri::X_INPUT_GAMEPAD_Y);
  REQUIRE(rig.system.GetCapabilities(0, 0, &caps) == X_ERROR_SUCCESS);
  REQUIRE(caps.sub_type == 7);  // ordinary active-device policy, no wheel override
  REQUIRE(rig.system.SetState(0, &vibration) == X_ERROR_DEVICE_NOT_CONNECTED);
  REQUIRE(pad->vibration_calls == 1);
  REQUIRE(uint16_t(pad->last_vibration.left_motor_speed) == 20000);
  REQUIRE(uint16_t(pad->last_vibration.right_motor_speed) == 10000);
  auto keyboard = std::make_unique<OrdinaryFakeDriver>(true, X_ERROR_SUCCESS);
  keyboard->id = static_cast<ri::DeviceId>(52);
  rig.system.AddDriver(std::move(keyboard));
  REQUIRE(uint16_t(rig.Read().gamepad.buttons) == (ri::X_INPUT_GAMEPAD_Y | ri::X_INPUT_GAMEPAD_A));
  ri::X_INPUT_STATE host{};
  REQUIRE(rig.system.GetHostPadState(0, &host) == X_ERROR_SUCCESS);
  REQUIRE(uint16_t(host.gamepad.buttons) == ri::X_INPUT_GAMEPAD_A);  // raw pad only, no keyboard

  // Check the new driver's unavailable/stop projection independently of
  // the unrelated native pad/keyboard states which correctly remain merged.
  {
    std::lock_guard lock(rig.fake->m);
    rig.fake->at(0).attached = false;
    rig.fake->at(0).script.push_back({sh::Call::Gone, sh::Call::NotCalled});
  }
  rig.Tick();
  ri::X_INPUT_STATE direct{};
  REQUIRE(rig.driver->GetDeviceState(rig.id, &direct) == X_ERROR_SUCCESS);
  REQUIRE(Neutral(direct.gamepad));
  REQUIRE(rig.lifecycle->Stop(2s).resolved());
  REQUIRE(rig.driver->GetDeviceState(rig.id, &direct) == X_ERROR_SUCCESS);
  REQUIRE(Neutral(direct.gamepad));
  REQUIRE(rig.system.GetState(0, &host) == X_ERROR_SUCCESS);
  REQUIRE(uint16_t(host.gamepad.buttons) == (ri::X_INPUT_GAMEPAD_Y | ri::X_INPUT_GAMEPAD_A));
}

TEST_CASE("Wheelkit driver publishes a changed neutral packet on focus loss", "[wheelkit-driver]") {
  Rig rig;
  rig.Ready();
  rig.SetButtons({2});
  rig.Tick();
  const auto live = rig.Read();
  REQUIRE(uint16_t(live.gamepad.buttons) == ri::X_INPUT_GAMEPAD_B);
  rig.lifecycle->SetFocused(false);
  const auto neutral = rig.Read();
  REQUIRE(Neutral(neutral.gamepad));
  REQUIRE(uint32_t(neutral.packet_number) != uint32_t(live.packet_number));
  REQUIRE(rig.lifecycle->Stop(2s).resolved());
}

TEST_CASE("Title preference overrides only admitted bound wheel axes and capabilities",
          "[wheelkit-priority]") {
  SettingsGuard restore;
  REXCVAR_SET(pad_remap, "A=Y");
  Rig rig;
  auto pad = std::make_unique<OrdinaryFakeDriver>(false, X_ERROR_SUCCESS);
  pad->gamepad.thumb_lx = 25000;
  pad->gamepad.thumb_ly = 12000;
  pad->gamepad.thumb_rx = 8000;
  pad->gamepad.thumb_ry = -13000;
  pad->gamepad.left_trigger = 180;
  pad->gamepad.right_trigger = 220;
  rig.system.AddDriver(std::move(pad));
  auto keyboard = std::make_unique<OrdinaryFakeDriver>(true, X_ERROR_SUCCESS);
  keyboard->id = static_cast<ri::DeviceId>(52);
  keyboard->subtype = 9;
  keyboard->gamepad.buttons = ri::X_INPUT_GAMEPAD_X;
  keyboard->gamepad.thumb_lx = -30000;
  keyboard->gamepad.thumb_ly = -20000;
  keyboard->gamepad.thumb_rx = -15000;
  keyboard->gamepad.thumb_ry = 18000;
  keyboard->gamepad.left_trigger = 255;
  rig.system.AddDriver(std::move(keyboard));
  auto assign = [&](bool enabled, ri::DeviceId preferred) {
    rig.system.SetDeviceAssignment(std::make_unique<WheelkitDeviceAssignment>(
        std::make_unique<ri::SharedAssignment>(), preferred, enabled));
  };
  auto caps = [&] {
    ri::X_INPUT_CAPABILITIES result{};
    REQUIRE(rig.system.GetCapabilities(0, 0, &result) == X_ERROR_SUCCESS);
    return result.sub_type;
  };
  assign(true, rig.id);
  REQUIRE(int16_t(rig.Read().gamepad.thumb_lx) == -30000);
  REQUIRE(caps() == 9);  // not started: fall back to the ordinary active device
  rig.Ready();

  // Plain/default-disabled policy preserves ordinary merge and activity rules.
  assign(false, rig.id);
  auto ordinary = rig.Read();
  REQUIRE(int16_t(ordinary.gamepad.thumb_lx) == -30000);
  REQUIRE(ordinary.gamepad.left_trigger == 255);
  REQUIRE(ordinary.gamepad.right_trigger == 220);
  REQUIRE(caps() == 9);
  assign(true, static_cast<ri::DeviceId>(999));  // preference outside assignment
  REQUIRE(int16_t(rig.Read().gamepad.thumb_lx) == -30000);
  REQUIRE(caps() == 9);

  assign(true, rig.id);
  const auto priority = rig.Read();
  REQUIRE(int16_t(priority.gamepad.thumb_lx) == -30000);  // keyboard passes through
  REQUIRE(priority.gamepad.left_trigger == 255);
  REQUIRE(priority.gamepad.right_trigger == 0);
  REQUIRE(int16_t(priority.gamepad.thumb_ly) == -20000);
  REQUIRE(int16_t(priority.gamepad.thumb_rx) == -15000);
  REQUIRE(int16_t(priority.gamepad.thumb_ry) == 18000);
  REQUIRE(uint16_t(priority.gamepad.buttons) == (ri::X_INPUT_GAMEPAD_Y | ri::X_INPUT_GAMEPAD_X));
  REQUIRE(caps() == 1);  // preference beats active keyboard without subtype 2
  ri::DeviceStatePolicy metadata;
  ri::X_INPUT_STATE direct{};
  REQUIRE(rig.driver->GetDeviceStateWithPolicy(rig.id, &direct, &metadata) == X_ERROR_SUCCESS);
  REQUIRE(metadata.valid);
  REQUIRE(metadata.bound_axes == (ri::DeviceStatePolicy::kLeftX |
      ri::DeviceStatePolicy::kLeftTrigger | ri::DeviceStatePolicy::kRightTrigger));
  ri::X_INPUT_STATE host{};
  REQUIRE(rig.system.GetHostPadState(0, &host) == X_ERROR_SUCCESS);
  REQUIRE(int16_t(host.gamepad.thumb_lx) == 25000);  // no guest policy on host reads
  REQUIRE(host.gamepad.left_trigger == 180);
  REQUIRE(host.gamepad.right_trigger == 220);
  REQUIRE(uint16_t(host.gamepad.buttons) == ri::X_INPUT_GAMEPAD_A);

  // Routing is delegated; the wheel preference never spreads to guest user 1.
  auto second = std::make_unique<OrdinaryFakeDriver>(false, X_ERROR_SUCCESS);
  second->id = static_cast<ri::DeviceId>(53);
  second->subtype = 8;
  second->gamepad.thumb_lx = -7000;
  second->gamepad.right_trigger = 91;
  rig.system.AddDriver(std::move(second));
  rig.system.SetDeviceAssignment(std::make_unique<WheelkitDeviceAssignment>(
      std::make_unique<ri::SlotAssignment>(), rig.id, true));
  REQUIRE(int16_t(rig.Read().gamepad.thumb_lx) == -30000);
  ri::X_INPUT_STATE user1{};
  REQUIRE(rig.system.GetState(1, &user1) == X_ERROR_SUCCESS);
  REQUIRE(int16_t(user1.gamepad.thumb_lx) == -7000);
  REQUIRE(user1.gamepad.right_trigger == 91);
  ri::X_INPUT_CAPABILITIES user1_caps{};
  REQUIRE(rig.system.GetCapabilities(1, 0, &user1_caps) == X_ERROR_SUCCESS);
  REQUIRE(user1_caps.sub_type == 8);

  rig.clock_offset->store(200'000);
  REQUIRE(int16_t(rig.Read().gamepad.thumb_lx) == -30000);
  REQUIRE(caps() == 9);
  REQUIRE(rig.driver->GetDeviceStateWithPolicy(rig.id, &direct, &metadata) == X_ERROR_SUCCESS);
  REQUIRE_FALSE(metadata.valid);
  REQUIRE(metadata.bound_axes == 0);
  rig.clock_offset->store(0);
  rig.Tick();
  REQUIRE(int16_t(rig.Read().gamepad.thumb_lx) == -30000);
  REQUIRE(caps() == 1);
  rig.lifecycle->SetFocused(false);
  REQUIRE(int16_t(rig.Read().gamepad.thumb_lx) == -30000);
  REQUIRE(caps() == 9);
  REQUIRE(rig.lifecycle->Stop(2s).resolved());
  REQUIRE(int16_t(rig.Read().gamepad.thumb_lx) == -30000);
  REQUIRE(caps() == 9);
}

TEST_CASE("Core status preserves unbound and unavailable channel fallback",
          "[wheelkit-priority]") {
  auto profile = Profile();
  auto begin = profile.find("brake = ");
  profile.erase(begin, profile.find('\n', begin) - begin + 1);
  const auto throttle = profile.find("throttle = ");
  const auto device = profile.find(Device, throttle);
  profile.replace(device, Device.size(), "{d71b8350-61b7-11f1-8002-444553540000}");
  Rig rig(profile);
  rig.Ready();
  auto pad = std::make_unique<OrdinaryFakeDriver>(false, X_ERROR_SUCCESS);
  pad->gamepad.thumb_lx = 25000;
  pad->gamepad.left_trigger = 180;
  pad->gamepad.right_trigger = 220;
  rig.system.AddDriver(std::move(pad));
  rig.system.SetDeviceAssignment(std::make_unique<WheelkitDeviceAssignment>(
      std::make_unique<ri::SharedAssignment>(), rig.id, true));
  const auto snapshot = rig.lifecycle->Read(Now());
  REQUIRE(snapshot->actions[sh::action_index("brake")].status == sh::Status::Unbound);
  REQUIRE(snapshot->actions[sh::action_index("throttle")].status == sh::Status::DeviceMissing);
  ri::DeviceStatePolicy metadata;
  ri::X_INPUT_STATE direct{};
  REQUIRE(rig.driver->GetDeviceStateWithPolicy(rig.id, &direct, &metadata) == X_ERROR_SUCCESS);
  REQUIRE(metadata.valid);
  REQUIRE(metadata.bound_axes == ri::DeviceStatePolicy::kLeftX);
  auto state = rig.Read();
  REQUIRE(int16_t(state.gamepad.thumb_lx) == 0);
  REQUIRE(state.gamepad.left_trigger == 180);
  REQUIRE(state.gamepad.right_trigger == 220);
  {
    std::lock_guard lock(rig.fake->m);
    rig.fake->at(0).state.axes[0] = 49152;
  }
  rig.Tick();
  REQUIRE(int16_t(rig.Read().gamepad.thumb_lx) == 16384);  // smaller than pad wins
  {
    std::lock_guard lock(rig.fake->m);
    rig.fake->at(0).attached = false;
    rig.fake->at(0).script.push_back({sh::Call::Gone, sh::Call::NotCalled});
  }
  rig.Tick();
  REQUIRE(rig.driver->GetDeviceStateWithPolicy(rig.id, &direct, &metadata) == X_ERROR_SUCCESS);
  REQUIRE_FALSE(metadata.valid);
  REQUIRE(metadata.bound_axes == 0);
  REQUIRE(int16_t(rig.Read().gamepad.thumb_lx) == 25000);
  ri::X_INPUT_CAPABILITIES caps{};
  REQUIRE(rig.system.GetCapabilities(0, 0, &caps) == X_ERROR_SUCCESS);
  REQUIRE(caps.sub_type == 7);
  REQUIRE(rig.lifecycle->Stop(2s).resolved());
}

TEST_CASE("Core rearming zero remains an eligible bound priority axis", "[wheelkit-priority]") {
  Rig rig(Profile(), 65535);
  rig.Ready();
  REQUIRE(rig.lifecycle->Read(Now())->actions[sh::action_index("throttle")].status ==
      sh::Status::Rearming);
  auto pad = std::make_unique<OrdinaryFakeDriver>(false, X_ERROR_SUCCESS);
  pad->gamepad.right_trigger = 200;
  rig.system.AddDriver(std::move(pad));
  rig.system.SetDeviceAssignment(std::make_unique<WheelkitDeviceAssignment>(
      std::make_unique<ri::SharedAssignment>(), rig.id, true));
  REQUIRE(rig.Read().gamepad.right_trigger == 0);
  ri::DeviceStatePolicy metadata;
  ri::X_INPUT_STATE direct{};
  REQUIRE(rig.driver->GetDeviceStateWithPolicy(rig.id, &direct, &metadata) == X_ERROR_SUCCESS);
  REQUIRE(metadata.bound_axes & ri::DeviceStatePolicy::kRightTrigger);
  REQUIRE(rig.lifecycle->Stop(2s).resolved());
}

TEST_CASE("Wheel versus pad priority preserves keyboard driving in either enumeration order",
          "[wheelkit-keyboard-priority]") {
  SettingsGuard restore;
  REXCVAR_SET(pad_remap, "");
  for (const bool keyboard_first : {true, false}) {
    INFO("keyboard enumerated before wheel: " << keyboard_first);
    auto keyboard = std::make_unique<OrdinaryFakeDriver>(true, X_ERROR_SUCCESS);
    auto* keys = keyboard.get();
    keys->id = static_cast<ri::DeviceId>(52);
    keys->gamepad.buttons = ri::X_INPUT_GAMEPAD_X;
    std::unique_ptr<ri::InputDriver> before;
    if (keyboard_first) before = std::move(keyboard);
    Rig rig(Profile(), 0, std::move(before));
    if (!keyboard_first) rig.system.AddDriver(std::move(keyboard));
    rig.Ready();
    auto pad = std::make_unique<OrdinaryFakeDriver>(false, X_ERROR_SUCCESS);
    pad->gamepad.thumb_lx = 30000;
    pad->gamepad.left_trigger = 200;
    pad->gamepad.right_trigger = 220;
    rig.system.AddDriver(std::move(pad));
    auto assign = [&](bool enabled) {
      rig.system.SetDeviceAssignment(std::make_unique<WheelkitDeviceAssignment>(
          std::make_unique<ri::SharedAssignment>(), rig.id, enabled));
    };
    assign(false);
    auto state = rig.Read();
    REQUIRE(int16_t(state.gamepad.thumb_lx) == 30000);
    REQUIRE(state.gamepad.left_trigger == 200);
    REQUIRE(state.gamepad.right_trigger == 220);
    assign(true);
    state = rig.Read();
    REQUIRE(int16_t(state.gamepad.thumb_lx) == 0);
    REQUIRE(state.gamepad.left_trigger == 0);
    REQUIRE(state.gamepad.right_trigger == 0);  // pad loses to bound resting wheel

    keys->gamepad.thumb_lx = -17000;
    keys->gamepad.left_trigger = 81;
    keys->gamepad.right_trigger = 93;
    state = rig.Read();
    REQUIRE(int16_t(state.gamepad.thumb_lx) == -17000);
    REQUIRE(state.gamepad.left_trigger == 81);
    REQUIRE(state.gamepad.right_trigger == 93);  // keyboard remains usable
    REQUIRE(uint16_t(state.gamepad.buttons) == (ri::X_INPUT_GAMEPAD_A | ri::X_INPUT_GAMEPAD_X));

    // With the keyboard released, a smaller wheel value still beats the pad.
    keys->gamepad.thumb_lx = 0;
    keys->gamepad.left_trigger = keys->gamepad.right_trigger = 0;
    {
      std::lock_guard lock(rig.fake->m);
      rig.fake->at(0).state.axes[0] = 49152;
      rig.fake->at(0).state.axes[2] = 32768;
      rig.fake->at(0).state.axes[5] = 16384;
    }
    rig.Tick();
    state = rig.Read();
    REQUIRE(int16_t(state.gamepad.thumb_lx) == 16384);
    REQUIRE(state.gamepad.right_trigger == 128);
    REQUIRE(state.gamepad.left_trigger == 64);
    assign(false);
    state = rig.Read();
    REQUIRE(int16_t(state.gamepad.thumb_lx) == 30000);
    REQUIRE(state.gamepad.right_trigger == 220);
    REQUIRE(state.gamepad.left_trigger == 200);
    REQUIRE(rig.lifecycle->Stop(2s).resolved());
  }
}

namespace {
const std::string WheelPath = R"(\\?\HID#VID_346E&PID_0006#EXACT-WHEEL-INSTANCE)";
class NativePathFake final : public OrdinaryFakeDriver {
 public:
  NativePathFake() : OrdinaryFakeDriver(false, X_ERROR_SUCCESS) {}
  bool ConfigureDevicePaths(const ri::DevicePathPolicy& next) override {
    if (attached) return false;
    policy = next;
    ++configured;
    return true;
  }
  void OnWindowAvailable(rex::ui::Window*) override { attached = true; }
  bool Open(std::string path) {
    if (!policy.Allows(path)) return false;
    paths.push_back(std::move(path));
    return true;
  }
  void EnumerateDevices(std::vector<ri::DeviceInfo>& out) override {
    if (!paths.empty()) OrdinaryFakeDriver::EnumerateDevices(out);
  }
  ri::DevicePathPolicy policy;
  std::vector<std::string> paths;
  int configured{};
  bool attached{};
};
struct InertDrawContext : rex::ui::UIDrawContext {
  InertDrawContext() : UIDrawContext(1, 1) {}
};
std::shared_ptr<sh::fake::Provider> IntegrationFake() {
  auto fake = std::make_shared<sh::fake::Provider>();
  fake->real_clock = true;
  sh::fake::Device device;
  device.listed = {Device, Product, "inert integration wheel", WheelPath};
  device.caps = sh::fake::wheel_caps();
  device.state.axes[0] = 32768;
  fake->add(std::move(device));
  return fake;
}
WheelkitAppInputOptions IntegrationOptions() {
  const auto folder = std::filesystem::temp_directory_path() /
      ("fh1-integration-" + std::to_string(GetCurrentProcessId()));
  std::filesystem::create_directories(folder);
  WheelkitAppInputOptions options;
  options.enabled = true;
  options.lifecycle = {folder.wstring(), Profile()};
  return options;
}
}

TEST_CASE("Private app integration prepares before native attachment and preserves unrelated input",
          "[wheelkit-integration]") {
  SettingsGuard restore;
  REXCVAR_SET(pad_remap, "");
  auto fake = IntegrationFake();
  std::vector<std::thread::id> creators;
  WheelkitAppInput app(IntegrationOptions(), [fake, &creators] {
    creators.push_back(std::this_thread::get_id());
    return std::make_shared<FakeOwner>(fake);
  }, &Now);
  REQUIRE(app.ready());
  REQUIRE(creators.size() == 2);  // preflight closed before sampling epoch
  REQUIRE(creators[0] == creators[1]);
  REQUIRE(creators[0] != std::this_thread::get_id());
  {
    std::lock_guard lock(fake->m);
    REQUIRE(fake->at(0).opens == 1);
    REQUIRE(fake->at(0).closes == 1);
    REQUIRE(fake->at(0).polls == 0);
    REQUIRE(fake->at(0).state_reads == 0);
  }
  ri::InputSystem system(nullptr);
  auto native = std::make_unique<NativePathFake>();
  auto* pads = native.get();
  pads->gamepad = {};
  pads->gamepad.thumb_lx = 30000;
  system.AddDriver(std::move(native));
  auto keyboard = std::make_unique<OrdinaryFakeDriver>(true, X_ERROR_SUCCESS);
  auto* keys = keyboard.get();
  keys->id = static_cast<ri::DeviceId>(52);
  keys->gamepad = {};
  system.AddDriver(std::move(keyboard));
  REQUIRE(app.Attach(system));
  REQUIRE(pads->configured == 1);
  REQUIRE_FALSE(pads->Open(WheelPath));
  std::string upper = WheelPath;
  std::transform(upper.begin(), upper.end(), upper.begin(), [](char c) {
    return c >= 'a' && c <= 'z' ? char(c - ('a' - 'A')) : c;
  });
  REQUIRE_FALSE(pads->Open(upper));
  REQUIRE_FALSE(pads->Open(""));  // candidate quarantine, no wheel invalidation
  REQUIRE(pads->Open(R"(\\?\HID#OTHER-PAD)"));
  system.AttachWindow(nullptr);  // fake attachment only, no native window
  REQUIRE(system.ConfigureDevicePaths({}) == 0);  // driver refuses late edits
  InertDrawContext context;
  std::uint64_t frame = 0;
  auto pump = [&] {
    app.Draw(context);
    return app.lifecycle()->WaitForFrame(++frame, 2s);
  };
  auto live = [&] {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline) {
      REQUIRE(pump());
      if (app.lifecycle()->Read(Now())->actions[0].status == sh::Status::Live) return;
      std::this_thread::yield();
    }
    FAIL("integration fake did not become live");
  };
  live();
  ri::X_INPUT_STATE state{};
  REQUIRE(system.GetState(0, &state) == X_ERROR_SUCCESS);
  REQUIRE(int16_t(state.gamepad.thumb_lx) == 0);  // bound resting wheel wins pad
  keys->gamepad.thumb_lx = -12000;
  REQUIRE(system.GetState(0, &state) == X_ERROR_SUCCESS);
  REQUIRE(int16_t(state.gamepad.thumb_lx) == -12000);  // keyboard remains usable
  keys->gamepad = {};
  {
    std::lock_guard lock(fake->m);
    fake->at(0).state.axes[0] = 49152;
  }
  REQUIRE(pump());
  REQUIRE(pads->Open(R"(\\?\HID#NEW-UNRELATED-PAD)"));
  REQUIRE(system.GetState(0, &state) == X_ERROR_SUCCESS);
  REQUIRE(int16_t(state.gamepad.thumb_lx) == 16384);
  pads->paths.pop_back();  // unrelated unplug leaves wheel admitted
  REQUIRE(system.GetState(0, &state) == X_ERROR_SUCCESS);
  REQUIRE(int16_t(state.gamepad.thumb_lx) == 16384);
  REQUIRE(app.reason().empty());

  app.SetFocused(false);
  REQUIRE(app.lifecycle()->Read(Now())->validity == SnapshotValidity::Unfocused);
  REQUIRE(app.lifecycle()->Read(Now())->actions[0].value == 0);
  REQUIRE(system.GetState(0, &state) == X_ERROR_SUCCESS);
  REQUIRE(int16_t(state.gamepad.thumb_lx) == 30000);  // ordinary pad fallback
  app.SetFocused(true);
  live();
  REQUIRE(system.GetState(0, &state) == X_ERROR_SUCCESS);
  REQUIRE(int16_t(state.gamepad.thumb_lx) == 16384);

  // Same exact path reconnect remains admitted; Core owns the fresh epoch.
  { std::lock_guard lock(fake->m); fake->at(0).script.push_back({sh::Call::Gone, sh::Call::NotCalled}); }
  REQUIRE(pump());
  live();
  REQUIRE(app.reason().empty());
  // The same DInput instance reopening under a different path cannot publish.
  {
    std::lock_guard lock(fake->m);
    fake->at(0).opened = {Device, Product, "changed path", R"(\\?\HID#REPLACED-WHEEL-PATH)"};
    fake->at(0).script.push_back({sh::Call::Gone, sh::Call::NotCalled});
  }
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (app.reason().empty() && std::chrono::steady_clock::now() < deadline) {
    (void)pump();
    std::this_thread::yield();
  }
  REQUIRE(app.reason().find("path/identity changed") != std::string::npos);
  REQUIRE(app.lifecycle()->Read(Now())->validity != SnapshotValidity::Live);
  pads->gamepad = {};
  REQUIRE(system.GetState(0, &state) == X_ERROR_SUCCESS);
  REQUIRE(Neutral(state.gamepad));
  (void)app.Stop();  // identity failure is reported, not relabeled a clean stop
  REQUIRE(app.lifecycle()->Status().done);
  (void)app.Stop();  // shared cleanup entry used by both app shutdown paths
  const auto ticks = app.lifecycle()->Status().frame_ticks;
  app.Draw(context);
  REQUIRE(app.lifecycle()->Status().frame_ticks == ticks);
  REQUIRE(fake->misuse == 0);
}

TEST_CASE("Private integration refuses unsafe bootstrap without changing ordinary native input",
          "[wheelkit-integration]") {
  int factories = 0;
  WheelkitAppInput disabled({}, [&]() -> std::shared_ptr<sh::ProviderOwner> {
    ++factories;
    throw std::runtime_error("disabled path must not create a provider");
  }, &Now);
  REQUIRE_FALSE(disabled.ready());
  REQUIRE(factories == 0);
  REQUIRE(disabled.Stop());
  for (const std::string fault : {"missing", "duplicate", "product", "path", "close", "unsupported"}) {
    INFO(fault);
    auto fake = IntegrationFake();
    {
      std::lock_guard lock(fake->m);
      if (fault == "missing") fake->at(0).attached = false;
      if (fault == "duplicate") fake->devices.push_back(fake->at(0));
      if (fault == "product") fake->at(0).listed.prod = "{00000000-0000-0000-0000-000000000001}";
      if (fault == "path") fake->at(0).listed.interface_path.clear();
    }
    WheelkitAppInput app(IntegrationOptions(), [fake, unresolved = fault == "close"] {
      return std::make_shared<FakeOwner>(fake, unresolved);
    }, &Now);
    ri::InputSystem system(nullptr);
    auto native = std::make_unique<NativePathFake>();
    auto* pads = native.get();
    if (fault == "unsupported") pads->attached = true;
    system.AddDriver(std::move(native));
    REQUIRE_FALSE(app.Attach(system));
    REQUIRE_FALSE(app.reason().empty());
    REQUIRE(pads->Open(WheelPath));  // no successful filter installation
    { std::lock_guard lock(fake->m);
      REQUIRE(fake->at(0).polls == 0);
      REQUIRE(fake->at(0).state_reads == 0); }
    REQUIRE(app.Stop() == (fault == "unsupported"));
  }
}
