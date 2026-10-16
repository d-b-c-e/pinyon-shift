#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_message.hpp>

#include <atomic>
#include <cstring>
#include <filesystem>

#include <rex/cvar.h>
#include <rex/input/input_system.h>

#include "input/wheelkit_input_driver.h"
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
  explicit FakeOwner(std::shared_ptr<sh::fake::Provider> fake) : fake_(std::move(fake)) {}
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
    return {released, 0, true, "fake input released"};
  }
 private:
  std::shared_ptr<sh::fake::Provider> fake_;
  std::mutex mutex_;
  bool stopped_{};
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
  Rig() {
    fake->real_clock = true;
    sh::fake::Device wheel;
    wheel.listed = {Device, Product, "Driver fixture wheel"};
    wheel.caps = sh::fake::wheel_caps();
    wheel.state.axes[0] = 32768;
    fake->add(wheel);
    const auto folder = std::filesystem::temp_directory_path() /
        ("fh1-driver-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(folder);
    lifecycle = std::make_shared<WheelkitInputLifecycle>(
        WheelkitLifecycleOptions{folder.wstring(), Profile()},
        [provider = fake] { return std::make_shared<FakeOwner>(provider); });
    auto owned = std::make_unique<WheelkitInputDriver>(lifecycle,
        [offset = clock_offset] { return Now() + offset->load(); });
    driver = owned.get();
    std::vector<ri::DeviceInfo> devices;
    driver->EnumerateDevices(devices);
    REQUIRE(devices.size() == 1);
    REQUIRE(devices[0].synthetic);
    id = devices[0].id;
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

class OrdinaryFakeDriver final : public ri::InputDriver {
 public:
  OrdinaryFakeDriver(bool keyboard, X_RESULT rumble) : InputDriver(nullptr, 0),
      keyboard_(keyboard), rumble_(rumble) {}
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
    out->gamepad.buttons = ri::X_INPUT_GAMEPAD_A;
    return X_ERROR_SUCCESS;
  }
  X_RESULT GetDeviceCapabilities(ri::DeviceId, uint32_t, ri::X_INPUT_CAPABILITIES* out) override {
    *out = {};
    out->sub_type = 7;  // distinguish ordinary selection from Wheelkit
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
