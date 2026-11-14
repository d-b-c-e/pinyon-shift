// The Wheelkit runtime's loader and call sites (wheelkit_host.h). Wheelkit's fork; not part of upstream Pinyon Shift.
#include "wheelkit/wheelkit_host.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <map>
#include <mutex>
#include <set>
#include <string>

#include <rex/input/input.h>
#include <rex/input/input_driver.h>
#include <rex/input/xinputd_ffb.h>
#include <rex/ui/presenter.h>
#include <rex/ui/ui_drawer.h>
#include <rex/ui/vulkan/device.h>
#include <rex/ui/vulkan/instance.h>
#include <rex/ui/vulkan/presenter.h>
#include <rex/ui/window.h>

#include "telemetry/forza_dash.h"
#include "wheelkit.h"

namespace pinyon_shift::wheelkit {

namespace {

#define X(ret, name, args) ret(WK_CALL* p_##name) args = nullptr;
WK_FUNCTIONS(X)
#undef X

// This build's version, by Wheelkit's scheme (games/forza-horizon in the Wheelkit repository), not upstream's.
constexpr const char* kVersion = "0.4.0-alpha";
// The game's force level: what reaches the wheel at Steering strength 50 as a share of what the game asks for
// (forceTune.gain; art of rally at 50 is the reference). Set from the whole-drive measurement.
constexpr float kForceLevel = 1.0f;
// The Xbox 360 wheel's effect units, as FH1 sends them: signed bytes. Its spring and damper arrive with saturations of
// 127 and 25 and coefficients of 63 and 12, and a corner's constant force is a few tens: full scale is 127, not
// DirectInput's 10000 (2026-11-14 gate run).
constexpr float kFullScale = 127.0f;

bool g_on = false, g_panel = false, g_profile = false;
std::atomic<float> g_wheel_throttle{0.0f};   // the profile's throttle as the game got it
std::function<void()> g_open_game_menu;

void Log(const std::string& line) {
  if (p_wk_log) p_wk_log(line.c_str());
}

// The game's own settings menu stays on Back + Start and the pause menu; F6 is the panel (decision 0066), and its
// Display page opens the game's menu.
constexpr char kPages[] = R"({"settings":{"product":"Forza Horizon","version":"%s","keys":"F6 settings, F8 force feedback off","pages":{
"controls":[{"id":"controls.profile","kind":"status","label":"Wheel profile","text":"No wheel set up"}],
"ffb":[{"id":"ffb.status","kind":"status","text":"Off"},
{"id":"ffb.enabled","kind":"toggle","label":"Force feedback","default":true,"help":"The game's own wheel force. F8 turns it off."},
{"id":"ffb.steering","kind":"percent","label":"Steering strength","default":50,"help":"50%% is the tuned strength; 100%% doubles it."}],
"display":[{"id":"game.menu","kind":"action","label":"Game settings","help":"The game's own settings: graphics, sound, camera, controls."}],
"help":[{"id":"help.runtime","kind":"status","label":"Wheelkit runtime","text":"not loaded"}]}}})";

void WK_CALL LogLine(const char*, void*) {}

std::wstring ExeFolder() {
  wchar_t exe[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
  while (n && exe[n - 1] != L'\\') n--;
  return std::wstring(exe, n);
}

std::string Utf8(const std::wstring& w) {
  char out[MAX_PATH * 3] = {};
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out, sizeof out, nullptr, nullptr);
  return out;
}

// ---- the profile's wheel as player 1's Xbox 360 racing wheel, and the game's wheel-force effects ----------------

using rex::X_RESULT;
using rex::X_STATUS;
using rex::input::DeviceId;
constexpr DeviceId kWheel = static_cast<DeviceId>(0x574B5748);  // WKWH

struct GameEffect {
  rex::input::xinputd_ffb::Effect effect;
  bool running = false;
};

class WheelDriver final : public rex::input::InputDriver, public rex::input::xinputd_ffb::Sink {
 public:
  WheelDriver() : InputDriver(nullptr, 0) {
    wk_input s{};
    s.size = sizeof s;
    p_wk_input_poll(&s);
    shifter_ = (s.bound & WK_BOUND_GEARS) && s.transmission == WK_TRANSMISSION_H_PATTERN;
  }
  ~WheelDriver() override {
    if (rex::input::xinputd_ffb::GetSink() == this) rex::input::xinputd_ffb::SetSink(nullptr);
  }

  X_STATUS Setup() override { return X_STATUS_SUCCESS; }
  void OnWindowAvailable(rex::ui::Window* window) override {
    // After SDL's driver, which registers itself for the game's wheel force: the runtime takes it (one force path).
    rex::input::xinputd_ffb::SetSink(this);
    if (window) p_wk_set_window(window->GetNativeWindowHandle());
    Log("forza-horizon: the runtime has the game's wheel force (XInputdFF effects)");
  }
  void EnumerateDevices(std::vector<rex::input::DeviceInfo>& out) override {
    rex::input::DeviceInfo info;
    info.id = kWheel;
    info.name = "Wheelkit wheel";
    info.family = rex::input::PadFamily::kXbox;
    out.push_back(std::move(info));
  }

  X_RESULT GetDeviceCapabilities(DeviceId id, uint32_t, rex::input::X_INPUT_CAPABILITIES* c) override {
    if (id != kWheel) return X_ERROR_DEVICE_NOT_CONNECTED;
    if (c) {
      // As upstream's SDL wheel: FH1 takes its wheel path for a wheel that reports force feedback, reads
      // sThumbLX == 900 as its linear 900-degree steering, and with thumb_rx other than 0 or 1 reads the H shifter's
      // gear from the raw thumb_rx (pinyon-shift PR #605).
      *c = {};
      c->type = 0x01;
      c->sub_type = 0x02;  // XINPUT_DEVSUBTYPE_WHEEL
      c->flags = uint16_t(rex::input::X_INPUT_CAPS_FFB_SUPPORTED);
      c->gamepad.buttons = 0xF3FF;
      c->gamepad.left_trigger = 0xFF;
      c->gamepad.right_trigger = 0xFF;
      c->gamepad.thumb_lx = int16_t(900);
      c->gamepad.thumb_ly = static_cast<int16_t>(0xFFFFu);
      c->gamepad.thumb_rx = shifter_ ? static_cast<int16_t>(0xFFFFu) : int16_t(0);
      c->gamepad.thumb_ry = static_cast<int16_t>(0xFFFFu);
      c->vibration.left_motor_speed = 0xFFFFu;
      c->vibration.right_motor_speed = 0xFFFFu;
    }
    return X_ERROR_SUCCESS;
  }

  X_RESULT GetDeviceState(DeviceId id, rex::input::X_INPUT_STATE* out) override {
    if (id != kWheel) return X_ERROR_DEVICE_NOT_CONNECTED;
    std::lock_guard lock(mutex_);
    Poll();
    if (out) {
      out->packet_number = packet_;
      out->gamepad = pad_;
    }
    return X_ERROR_SUCCESS;
  }
  X_RESULT SetDeviceVibration(DeviceId id, rex::input::X_INPUT_VIBRATION* v) override {
    if (id != kWheel) return X_ERROR_DEVICE_NOT_CONNECTED;
    if (v) SetRumble(v->left_motor_speed, v->right_motor_speed);
    return X_ERROR_SUCCESS;
  }
  X_RESULT GetDeviceKeystroke(DeviceId id, uint32_t, rex::input::X_INPUT_KEYSTROKE*) override {
    return id == kWheel ? X_ERROR_EMPTY : X_ERROR_DEVICE_NOT_CONNECTED;
  }

  // The game's wheel-force calls (guest threads).
  bool HasForceFeedback() override { return true; }
  void SetEffect(const rex::input::xinputd_ffb::Effect& e) override {
    std::lock_guard lock(mutex_);
    effects_[e.id].effect = e;
    if (created_.insert(e.type).second)
      Log("forza-horizon: the game created its " + std::string(rex::input::xinputd_ffb::EffectTypeName(e.type)) +
          " effect: " + rex::input::xinputd_ffb::DescribeEffect(e));
  }
  void UpdateEffect(const rex::input::xinputd_ffb::Effect& e, uint32_t) override {
    std::lock_guard lock(mutex_);
    effects_[e.id].effect = e;
  }
  void EffectOperation(uint32_t id, uint32_t op) override {
    std::lock_guard lock(mutex_);
    auto it = effects_.find(id);
    if (it != effects_.end()) it->second.running = op != 3;
  }
  void DeviceControl(uint32_t command) override {
    std::lock_guard lock(mutex_);
    using namespace rex::input::xinputd_ffb;
    if (command == kStopAll || command == kDisableActuators)
      for (auto& [id, g] : effects_) g.running = false;
    else if (command == kReset)
      effects_.clear();
  }
  void SetDeviceGain(uint32_t gain) override {
    std::lock_guard lock(mutex_);
    device_gain_ = gain <= 255 ? float(gain) / 255.0f : std::min(float(gain), kFullScale) / kFullScale;
  }
  void SetRumble(uint16_t left, uint16_t right) override {
    rumble_.store(float(std::max(left, right)) / 65535.0f, std::memory_order_relaxed);
  }

 private:
  // The newest snapshot into the pad (at most every 4 ms), and the force for the game's running effects.
  void Poll() {
    const auto now = std::chrono::steady_clock::now();
    if (now - last_poll_ < std::chrono::milliseconds(4)) return;
    last_poll_ = now;
    wk_input s{};
    s.size = sizeof s;
    if (!p_wk_input_poll(&s)) return;
    if (rex::input::xinputd_ffb::GetSink() != this && window_seen_) rex::input::xinputd_ffb::SetSink(this);
    rex::input::X_INPUT_GAMEPAD pad{};
    if ((s.flags & (WK_INPUT_PROFILE | WK_INPUT_INJECTED)) && !(s.flags & WK_INPUT_PANEL)) {
      using namespace rex::input;
      auto held = [&](int action) { return s.held[action] != 0; };
      uint16_t buttons = 0;
      // FH1's wheel layout 1 (upstream's launcher, BindingModel.cs): RB shift up, LB down, B handbrake, A clutch
      // button and confirm in menus, Y rewind, BACK the view, START pause, LS horn, RS look back, the D-pad.
      if (held(WK_ACTION_CONFIRM)) buttons |= X_INPUT_GAMEPAD_A;
      if (held(WK_ACTION_BACK) || held(WK_ACTION_HANDBRAKE) || s.handbrake > 0.5f) buttons |= X_INPUT_GAMEPAD_B;
      if (held(WK_ACTION_START)) buttons |= X_INPUT_GAMEPAD_START;
      if (held(WK_ACTION_CAMERA)) buttons |= X_INPUT_GAMEPAD_BACK;
      if (held(WK_ACTION_SHIFT_UP)) buttons |= X_INPUT_GAMEPAD_RIGHT_SHOULDER;
      if (held(WK_ACTION_SHIFT_DOWN)) buttons |= X_INPUT_GAMEPAD_LEFT_SHOULDER;
      if (held(WK_ACTION_RESET)) buttons |= X_INPUT_GAMEPAD_Y;
      if (held(WK_ACTION_HORN)) buttons |= X_INPUT_GAMEPAD_LEFT_THUMB;
      if (held(WK_ACTION_LOOK_BACK)) buttons |= X_INPUT_GAMEPAD_RIGHT_THUMB;
      if (held(WK_ACTION_NAV_UP)) buttons |= X_INPUT_GAMEPAD_DPAD_UP;
      if (held(WK_ACTION_NAV_DOWN)) buttons |= X_INPUT_GAMEPAD_DPAD_DOWN;
      if (held(WK_ACTION_NAV_LEFT)) buttons |= X_INPUT_GAMEPAD_DPAD_LEFT;
      if (held(WK_ACTION_NAV_RIGHT)) buttons |= X_INPUT_GAMEPAD_DPAD_RIGHT;
      pad.buttons = buttons;
      pad.thumb_lx = int16_t(std::lround(std::clamp(s.steer, -1.0f, 1.0f) * 32767.0f));
      pad.right_trigger = uint8_t(std::lround(std::clamp(s.throttle, 0.0f, 1.0f) * 255.0f));
      g_wheel_throttle.store(s.throttle, std::memory_order_relaxed);
      pad.left_trigger = uint8_t(std::lround(std::clamp(s.brake, 0.0f, 1.0f) * 255.0f));
      pad.thumb_ry = int16_t(std::lround(std::clamp(s.clutch, 0.0f, 1.0f) * 32767.0f));
      // The H shifter's gear: -1 reverse, 0 neutral, 1 and up the gear.
      pad.thumb_rx = shifter_ ? int16_t(std::clamp(s.gear, -1, 7)) : int16_t(0);
      if (!moved_ && (s.steer != 0 || s.throttle > 0 || s.brake > 0 || buttons)) {
        moved_ = true;
        Log("forza-horizon: the profile's input reached player 1's racing wheel");
      }
    }
    if (std::memcmp(&pad, &pad_, sizeof pad) != 0) {
      pad_ = pad;
      ++packet_;
    }
    Force(now, s.steer);
  }

  // The game's running effects, every poll: the constant force as the force, the spring and the damper as the
  // wheel's own effects, the rumble as a cue. Friction and inertia have no entry in the runtime: logged, not sent.
  void Force(std::chrono::steady_clock::time_point now, float steer) {
    using namespace rex::input::xinputd_ffb;
    float force = 0, spring = 0, damper = 0, friction = 0, inertia = 0;
    for (const auto& [id, g] : effects_) {
      if (!g.running) continue;
      const Effect& e = g.effect;
      const float gain = float(std::min<uint32_t>(e.gain, 255)) / 255.0f * device_gain_;
      const float coefficient = float(std::max(e.positive_coefficient, e.negative_coefficient)) / kFullScale * gain;
      switch (e.type) {
        // Positive pushes the rim toward positive steer, as the runtime's: in an injected corner the game's constant
        // force opposed the steering as it is (2026-11-14 gate run; negated it pushed along the steering).
        case kConstant: force += float(e.magnitude) / kFullScale * gain; break;
        case kSpring: spring = std::max(spring, coefficient); break;
        case kDamper: damper = std::max(damper, coefficient); break;
        case kFriction: friction = std::max(friction, coefficient); break;
        case kInertia: inertia = std::max(inertia, coefficient); break;
        default: break;
      }
    }
    force = std::clamp(force, -1.0f, 1.0f);
    p_wk_ffb_set_force(force);
    p_wk_ffb_set_spring(std::clamp(spring, 0.0f, 1.0f));
    // Friction resists the rim's motion as the damper does: the runtime has one damper, so the stronger of the two.
    p_wk_ffb_set_damper(std::clamp(std::max(damper, friction), 0.0f, 1.0f));
    const float rumble = rumble_.load(std::memory_order_relaxed);
    if (rumble > 0.02f && now - last_cue_ >= std::chrono::milliseconds(50)) {
      last_cue_ = now;
      p_wk_ffb_cue(WK_CUE_RUMBLE, std::min(rumble, 1.0f) * 0.3f, 100);
    }
    // The log: the range of what the game asked for, every 5 s for the first ten minutes (the units are traced here)
    low_ = std::min(low_, force), high_ = std::max(high_, force);
    top_spring_ = std::max(top_spring_, spring), top_damper_ = std::max(top_damper_, damper);
    top_friction_ = std::max(top_friction_, friction), top_inertia_ = std::max(top_inertia_, inertia);
    if (now - last_force_log_ >= std::chrono::seconds(5) && force_logs_ < 120) {
      last_force_log_ = now;
      if (high_ > low_ || top_spring_ > 0 || top_damper_ > 0) {
        ++force_logs_;
        char line[260];
        std::snprintf(line, sizeof line,
                      "forza-horizon: the game's force in the last 5 s: constant %.3f to %.3f, spring %.3f, damper %.3f, "
                      "friction %.3f, inertia %.3f, rumble %.2f, steer %.2f",
                      low_, high_, top_spring_, top_damper_, top_friction_, top_inertia_, rumble, steer);
        Log(line);
      }
      low_ = high_ = top_spring_ = top_damper_ = top_friction_ = top_inertia_ = 0;
    }
  }

  std::mutex mutex_;
  bool shifter_ = false, moved_ = false, window_seen_ = true;
  uint32_t packet_ = 0;
  rex::input::X_INPUT_GAMEPAD pad_{};
  std::chrono::steady_clock::time_point last_poll_{}, last_cue_{}, last_force_log_{};
  std::map<uint32_t, GameEffect> effects_;
  std::set<uint32_t> created_;
  float device_gain_ = 1.0f;
  std::atomic<float> rumble_{0.0f};
  float low_ = 0, high_ = 0, top_spring_ = 0, top_damper_ = 0, top_friction_ = 0, top_inertia_ = 0;
  int force_logs_ = 0;
};

// ---- the F6 panel on the presenter's Vulkan frames -----------------------------------------------------------------

class PanelDrawer final : public rex::ui::UIDrawer {
 public:
  PanelDrawer(const rex::ui::vulkan::VulkanDevice& device, rex::ui::Window& window)
      : device_(device), window_(window) {}
  void Draw(rex::ui::UIDrawContext& context) override {
    auto& vk = static_cast<rex::ui::vulkan::VulkanUIDrawContext&>(context);
    const auto* device = &device_;
    const unsigned long long pass = reinterpret_cast<unsigned long long>(vk.render_pass());
    if (attached_ && (pass != render_pass_ || unsigned(vk.render_pass_format()) != format_)) {
      p_wk_panel_vk_detach();
      attached_ = false;
    }
    if (!attached_) {
      if (failed_) return;
      wk_panel_vk_host host{};
      host.size = sizeof host;
      host.window = window_.GetNativeWindowHandle();
      host.instance = device->vulkan_instance()->instance();
      host.physical_device = device->physical_device();
      host.device = device->device();
      host.queue_family = device->queue_family_graphics_compute();
      {
        auto queue = device->AcquireQueue(host.queue_family, 0);
        host.queue = queue.queue();
      }
      host.image_count = 3;
      host.color_format = unsigned(vk.render_pass_format());
      host.render_pass = pass;
      attached_ = p_wk_panel_vk_attach(&host) != 0;
      if (!attached_) {
        failed_ = true;
        Log(std::string("forza-horizon: the F6 panel could not attach to the game's Vulkan frames: ") + p_wk_last_error());
        return;
      }
      render_pass_ = pass;
      format_ = host.color_format;
    }
    Events();
    // The panel may upload its font through the queue: the presenter's other users hold the same lock.
    auto queue = device->AcquireQueue(device->queue_family_graphics_compute(), 0);
    p_wk_panel_vk_frame(vk.draw_command_buffer(), 0, int(vk.render_target_width()), int(vk.render_target_height()));
  }

 private:
  void Events() {
    wk_panel_event e{};
    e.size = sizeof e;
    while (p_wk_panel_poll_event(&e)) {
      if (e.type == WK_PANEL_ACTION && std::string(e.id) == "game.menu" && g_open_game_menu) {
        p_wk_panel_set_open(0);
        window_.app_context().CallInUIThreadDeferred([] { g_open_game_menu(); });
      }
    }
    if (tick_++ % 30 == 0) {
      wk_ffb_status st{};
      st.size = sizeof st;
      if (p_wk_ffb_get_status(&st))
        p_wk_panel_set_text("ffb.status", st.text, st.state == WK_FFB_ON ? WK_TONE_GOOD : WK_TONE_NORMAL);
      wk_input s{};
      s.size = sizeof s;
      if (p_wk_input_poll(&s)) {
        const int now = !(s.flags & WK_INPUT_PROFILE) ? 0 : (s.flags & WK_INPUT_DEVICES) ? 2 : 1;
        if (now != shown_) {
          shown_ = now;
          std::string text = now == 0 ? "No wheel set up: Wheelkit sets it from your rig profile when you press Play on "
                                        "Forza Horizon (the keyboard and gamepad work)"
                                      : std::string(p_wk_input_profile()) + (now == 1 ? ": its wheel is not connected" : "");
          p_wk_panel_set_text("controls.profile", text.c_str(), now == 2 ? WK_TONE_GOOD : WK_TONE_WARNING);
          Log("forza-horizon: wheel profile: " + text);
        }
      }
    }
  }

  const rex::ui::vulkan::VulkanDevice& device_;
  rex::ui::Window& window_;
  bool attached_ = false, failed_ = false;
  unsigned long long render_pass_ = 0;
  unsigned format_ = 0;
  unsigned tick_ = 0;
  int shown_ = -1;
};

std::unique_ptr<PanelDrawer> g_drawer;
rex::ui::Presenter* g_presenter = nullptr;

// The heading from the pose hook: radians, clockwise seen from above (the runtime's yaw).
std::atomic<float> g_heading{0.0f}, g_px{0.0f}, g_py{0.0f}, g_pz{0.0f};
std::atomic<bool> g_have_pose{false};
std::atomic<bool> g_driving{false};

// The game's own views, in its camera controller's modes 0 to 4 (the two chase views first, as the camera editor
// finds them). The runtime never replaces one; it names the view on screen and keeps its framing.
const wk_camera_view kViews[] = {{"ChaseFar", "Chase far", 0}, {"ChaseNear", "Chase near", 0}, {"Hood", "Hood", 1},
                                 {"Bumper", "Bumper", 1}, {"Cockpit", "Cockpit", 1}};

}  // namespace

bool Start() {
  const std::wstring dir = ExeFolder();
  HMODULE m = LoadLibraryW((dir + L"wheelkit.dll").c_str());
  if (!m) return false;
  bool all = true;
#define X(ret, name, args)                                                     \
  p_##name = reinterpret_cast<ret(WK_CALL*) args>(GetProcAddress(m, #name)); \
  all = all && p_##name;
  WK_FUNCTIONS(X)
#undef X
  if (!all || p_wk_abi_version() != WK_ABI_VERSION) return false;
  const std::string settings = Utf8(dir + L"wheelkit.ini");
  wk_identity id{};
  id.size = sizeof id;
  id.game_id = "forza-horizon";
  id.game_name = "Forza Horizon";
  id.game_version = kVersion;
  id.settings_file = settings.c_str();
  id.log = LogLine;
  if (!p_wk_start(&id)) return false;
  g_on = true;
  p_wk_ffb_set_level(kForceLevel);
  g_profile = std::string(p_wk_input_profile()).size() > 0 || (p_wk_switches() & WK_SWITCH_INJECT);
  char pages[sizeof kPages + 32];
  const int length = std::snprintf(pages, sizeof pages, kPages, kVersion);
  g_panel = p_wk_panel_configure(pages, size_t(length)) != 0;
  if (g_panel) p_wk_panel_set_text("help.runtime", p_wk_runtime_version(), WK_TONE_NORMAL);
  p_wk_cameras_configure(kViews, int(std::size(kViews)));
  Log(g_profile ? "forza-horizon: the runtime has the wheel: player 1 is the rig profile's racing wheel"
                : "forza-horizon: no rig profile: the game's own controls");
  return true;
}

bool On() { return g_on; }

std::string SdlIgnoreList() {
  if (!g_on) return "";
  // Each binding line names its device's product GUID, {PPPPVVVV-0000-0000-0000-504944564944}.
  std::set<std::string> ids;
  for (int a = 0; a < WK_ACTION_COUNT; ++a) {
    const char* name = p_wk_action_name(a);
    char line[512];
    if (!name || !*name || !p_wk_settings_get("Controls", name, line, sizeof line)) continue;
    const char* prod = std::strstr(line, "prod={");
    if (!prod || std::strlen(prod) < 14) continue;
    const std::string pv(prod + 6, 8);
    ids.insert("0x" + pv.substr(4, 4) + "/0x" + pv.substr(0, 4));
  }
  std::string list;
  for (const auto& id : ids) list += (list.empty() ? "" : ",") + id;
  if (!list.empty()) Log("forza-horizon: SDL in the game leaves the profile's devices alone: " + list);
  return list;
}

std::unique_ptr<rex::input::InputDriver> CreateDriver() {
  if (!g_on || !g_profile) return nullptr;
  return std::make_unique<WheelDriver>();
}

void Attach(rex::ui::Presenter* presenter, const rex::ui::vulkan::VulkanDevice* device, rex::ui::Window* window,
            std::function<void()> open_game_menu) {
  if (!g_on || !window) return;
  p_wk_set_window(window->GetNativeWindowHandle());
  g_open_game_menu = std::move(open_game_menu);
  if (!g_panel || !presenter || !device) {
    Log("forza-horizon: no F6 panel (the presenter is not Vulkan)");
    return;
  }
  g_presenter = presenter;
  g_drawer = std::make_unique<PanelDrawer>(*device, *window);
  presenter->AddUIDrawerFromUIThread(g_drawer.get(), 64);   // above the game's own menus (32)
  Log("forza-horizon: the F6 panel draws on the game's Vulkan frames");
}

bool Telemetry(const telemetry::CarSample& sample) {
  if (!g_on) return false;
  wk_car car{};
  car.size = sizeof car;
  car.race_on = sample.driving ? 1 : 0;
  car.speed = sample.speed_mps;
  car.rpm = sample.rpm, car.rpm_max = sample.redline_rpm, car.rpm_idle = sample.idle_rpm;
  car.gear = sample.gear;
  car.throttle = sample.throttle, car.brake = sample.brake, car.clutch = sample.clutch;
  car.handbrake = sample.handbrake, car.steer = sample.steer;
  car.velocity[2] = sample.velocity_forward_mps;
  g_driving.store(sample.driving, std::memory_order_relaxed);
  if (sample.driving && g_have_pose.load(std::memory_order_relaxed)) {
    car.yaw = g_heading.load(std::memory_order_relaxed);
    car.position[0] = g_px.load(std::memory_order_relaxed);
    car.position[1] = g_py.load(std::memory_order_relaxed);
    car.position[2] = g_pz.load(std::memory_order_relaxed);
  }
  p_wk_telemetry_frame(&car);
  // The car takes the wheel's throttle: the player drives (the opening's scripted intro has handed the car over).
  static auto answered = std::chrono::steady_clock::time_point{};
  const float wheel = g_wheel_throttle.load(std::memory_order_relaxed);
  if (sample.driving && wheel > 0.5f && sample.throttle > 0.5f &&
      std::chrono::steady_clock::now() - answered > std::chrono::seconds(2)) {
    answered = std::chrono::steady_clock::now();
    char line[160];
    std::snprintf(line, sizeof line, "forza-horizon: the car answers the wheel (throttle %.2f, the car's %.2f)", wheel,
                  sample.throttle);
    Log(line);
  }
  static int frames = 0;
  if (sample.driving && frames++ % 120 == 0 && frames < 120 * 300) {
    char line[260];
    std::snprintf(line, sizeof line,
                  "forza-horizon: the car: %.1f m/s, gear %d, %.0f rpm, throttle %.2f, brake %.2f, steer %.2f, heading %.1f",
                  car.speed, car.gear, car.rpm, car.throttle, car.brake, car.steer, car.yaw * 57.29578f);
    Log(line);
  }
  return true;
}

void Camera(int mode) {
  if (!g_on) return;
  static int shown = -2;
  if (mode != shown) {
    shown = mode;
    Log("forza-horizon: the game's view is mode " + std::to_string(mode));
  }
  wk_camera cam{};
  cam.size = sizeof cam;
  const bool driving = g_driving.load(std::memory_order_relaxed);
  p_wk_cameras_frame(driving && mode >= 0 && mode < int(std::size(kViews)) ? mode : -1, nullptr, 0, &cam);
}

void Body(const float* v) {
  if (!g_on) return;
  // +2832 is the car's velocity in the world (x, y up, z) and +2880 its position: the heading is the direction of
  // travel, held while the car is nearly still (the title's pose hook also fires for other cars, so it is not used).
  const float vx = v[0], vz = v[2];
  if (!std::isfinite(vx) || !std::isfinite(vz)) return;
  if (vx * vx + vz * vz > 4.0f) {
    g_heading.store(std::atan2(vx, vz), std::memory_order_relaxed);
    g_have_pose.store(true, std::memory_order_relaxed);
  }
  g_px.store(v[9], std::memory_order_relaxed), g_py.store(v[10], std::memory_order_relaxed);
  g_pz.store(v[11], std::memory_order_relaxed);
}
void Stop() {
  if (!g_on) return;
  if (g_presenter && g_drawer) g_presenter->RemoveUIDrawerFromUIThread(g_drawer.get());
  p_wk_stop();   // detaches the panel while the device still exists
  g_drawer.reset();
  g_on = g_panel = false;
}

}  // namespace pinyon_shift::wheelkit
