// Included by direct_gears_seam.py after the actual production functions.
int checks = 0;
void check(bool value, const char* why) {
  ++checks;
  if (!value) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); }
}
void set(WheelkitSnapshot& snapshot, const char* name, float value,
         sh::Status status = sh::Status::Live) {
  snapshot.actions[sh::action_index(name)] = {value, status};
}
WheelkitSnapshot live() {
  WheelkitSnapshot snapshot;
  snapshot.validity = SnapshotValidity::Live;
  for (const auto* name : kDirectGearActions) set(snapshot, name, 0);
  return snapshot;
}
// Static guest findings 82BF8F28 / 82BFCDD8 / 82487650, not guest execution.
int guest_gear(const ri::X_INPUT_CAPABILITIES& caps, int16_t raw) {
  if (caps.sub_type != 2 || !(caps.flags & ri::X_INPUT_CAPS_FFB_SUPPORTED) ||
      caps.gamepad.thumb_rx != -1) return 10;
  if (raw == -1) return 0;
  if (raw == 0) return 9;
  return raw > 0 && raw < 9 ? raw : 10;
}
int main() {
  using P = ri::DeviceStatePolicy;
  ri::CapsDriver driver;
  ri::X_INPUT_CAPABILITIES caps{};
  driver.GetDeviceCapabilities(7, 0, &caps);
  check(caps.sub_type == 1 && caps.flags == 0 && caps.gamepad.thumb_rx == 0,
        "default caps stay gamepad without shifter/FFB");
  for (int raw = -1; raw <= 10; ++raw)
    check(guest_gear(caps, int16_t(raw)) == 10, "default caps cannot override gears");
  driver.native_direct_gears_ = true;
  check(driver.GetDeviceCapabilities(8, 0, &caps) == ri::X_ERROR_DEVICE_NOT_CONNECTED,
        "wrong device refused");
  driver.GetDeviceCapabilities(7, 0, &caps);
  check(caps.sub_type == 2 && caps.flags == ri::X_INPUT_CAPS_FFB_SUPPORTED &&
        caps.gamepad.thumb_rx == -1 && caps.gamepad.thumb_ry == -1 && caps.gamepad.thumb_lx == -1,
        "native SDL wheel/shifter/clutch caps without guessed rotation");
  check(caps.vibration.left_motor_speed == 0 && caps.vibration.right_motor_speed == 0,
        "no vibration motors");
  for (int negative = 0; negative < 3; ++negative) {
    auto bad = caps;
    if (negative == 0) bad.sub_type = 1;
    if (negative == 1) bad.flags = 0;
    if (negative == 2) bad.gamepad.thumb_rx = 0;
    check(guest_gear(bad, -1) == 10 && guest_gear(bad, 3) == 10,
          "negative capability cannot select reverse or forward");
  }
  auto snapshot = live();
  check(DirectGear(snapshot) == 0, "released available gates are neutral");
  check(guest_gear(caps, DirectGear(snapshot)) == 9, "raw neutral becomes internal neutral");
  for (int i = 0; i < 10; ++i) {
    snapshot = live();
    set(snapshot, kDirectGearActions[i], 1);
    const auto pad = Project(snapshot, true, true);
    check(pad.thumb_rx == i - 1, "all explicit reverse/neutral/gear1..8 levels");
    check(guest_gear(caps, pad.thumb_rx) == (i == 0 ? 0 : i == 1 ? 9 : i - 1),
          "exact guest raw-to-internal selection");
    check(Project(snapshot, false, true).thumb_rx == 0, "default path never projects gear");
    auto policy = Policy(snapshot, true, true);
    ri::X_INPUT_STATE state{}, keyboard{};
    state.gamepad = pad;
    keyboard.gamepad.thumb_rx = INT16_MIN;
    keyboard.gamepad.thumb_ry = INT16_MAX;
    keyboard.gamepad.thumb_lx = 12000;
    keyboard.gamepad.buttons = ri::X_INPUT_GAMEPAD_START;
    auto merged = Fold(state, keyboard, policy.bound_axes, policy.exclusive_axes);
    check(merged.gamepad.thumb_rx == pad.thumb_rx && merged.gamepad.thumb_ry == pad.thumb_ry,
          "keyboard cannot corrupt raw gear or clutch");
    check(merged.gamepad.thumb_lx == 12000 && merged.gamepad.buttons == ri::X_INPUT_GAMEPAD_START,
          "keyboard steering and buttons retained");
  }
  for (auto validity : {SnapshotValidity::NotStarted, SnapshotValidity::Disconnected,
      SnapshotValidity::Unfocused, SnapshotValidity::Stopped, SnapshotValidity::Stale,
      SnapshotValidity::InvalidClock}) {
    snapshot = live(); set(snapshot, "reverse", 1); snapshot.validity = validity;
    check(Project(snapshot, true, true).thumb_rx == 10, "invalid snapshot means no override");
    check(guest_gear(caps, Project(snapshot, true, true).thumb_rx) == 10,
          "invalid held reverse never becomes a guest gear");
    check(Policy(snapshot, true, true).exclusive_axes == (P::kRightX | P::kRightY),
          "invalid snapshot excludes unrelated stick gear and clutch");
    check(Project(snapshot, false, true).thumb_rx == 0 && !Policy(snapshot, false, true).valid,
          "default invalid snapshot behavior preserved");
  }
  for (int status = int(sh::Status::Invalid); status <= int(sh::Status::ContextInactive); ++status) {
    if (status == int(sh::Status::Live)) continue;
    snapshot = live(); set(snapshot, "gear2", 0, sh::Status(status));
    check(DirectGear(snapshot) == 10, "unavailable/rearming/test-neutral gate is no override");
  }
  for (float bad : {-0.1f, 1.1f, std::numeric_limits<float>::infinity(),
                    std::numeric_limits<float>::quiet_NaN()}) {
    snapshot = live(); set(snapshot, "gear1", bad);
    check(DirectGear(snapshot) == 10, "invalid level is no override");
  }
  snapshot = live(); set(snapshot, "reverse", 1); set(snapshot, "neutral", 1);
  check(DirectGear(snapshot) == 10, "conflicting held gates do not pick reverse");
  snapshot = live(); set(snapshot, "gear1", 1); set(snapshot, "gear8", 1);
  check(DirectGear(snapshot) == 10, "conflicting forward gates do not pick an order");
  snapshot = {}; snapshot.validity = SnapshotValidity::Live; set(snapshot, "neutral", 1);
  check(DirectGear(snapshot) == 10, "neutral-only profile cannot claim a shifter");
  set(snapshot, "gear4", 0);
  check(DirectGear(snapshot) == 0, "explicit sparse gate and neutral supported");
  check(Project(snapshot, true, false).thumb_rx == 10 && Policy(snapshot, true, false).valid,
        "inactive callback emits authoritative no override");
  snapshot = live(); set(snapshot, "clutch", 1); set(snapshot, "camera", 1);
  check(Project(snapshot, false, true).buttons == (ri::X_INPUT_GAMEPAD_LEFT_SHOULDER |
        ri::X_INPUT_GAMEPAD_RIGHT_SHOULDER), "default clutch/camera unchanged");
  check(Project(snapshot, true, true).buttons == 0 && Project(snapshot, true, true).thumb_ry == 32767,
        "native clutch is analog; camera cannot become a shift");
  set(snapshot, "shiftUp", 1); set(snapshot, "shiftDown", 1);
  check(Project(snapshot, true, true).buttons == (ri::X_INPUT_GAMEPAD_LEFT_SHOULDER |
        ri::X_INPUT_GAMEPAD_RIGHT_SHOULDER), "native layout1 explicit paddles");
  check((Project(snapshot, false, true).buttons & (ri::X_INPUT_GAMEPAD_B | ri::X_INPUT_GAMEPAD_X)) ==
        (ri::X_INPUT_GAMEPAD_B | ri::X_INPUT_GAMEPAD_X), "default BX paddles preserved");
  const std::string identity = " dev={d71b8350-61b7-11f1-8001-444553540000}"
      " prod={0006346e-0000-0000-0000-504944564944}\n";
  const std::string section = "[Controls]\nSchema=1\nTransmission=H-pattern\n";
  auto profile = sh::read_profile(section, 2);
  auto plan = sh::plan_profile(profile);
  check(!HasDirectGearProfile(profile, plan), "empty profile refused");
  profile = sh::read_profile(section + "neutral = button 0" + identity, 2);
  plan = sh::plan_profile(profile);
  check(!HasDirectGearProfile(profile, plan), "neutral-only admission refused");
  profile = sh::read_profile(section + "neutral = button 0" + identity +
      "gear8 = button 1" + identity, 2);
  plan = sh::plan_profile(profile);
  check(HasDirectGearProfile(profile, plan), "explicit gear8 admits native path");
  profile.section.transmission = "Automatic";
  check(!HasDirectGearProfile(profile, plan), "transmission must explicitly opt into H-pattern");
  profile.section.transmission = "H-pattern";
  plan.base[sh::action_index("reverse")] = sh::Status::Invalid;
  check(!HasDirectGearProfile(profile, plan), "invalid reverse binding refuses admission");
  ri::X_INPUT_STATE preferred{}, keyboard{};
  preferred.gamepad.thumb_rx = 2; keyboard.gamepad.thumb_rx = -30000;
  check(Fold(preferred, keyboard, P::kRightX, 0).gamepad.thumb_rx == -30000,
        "default zero exclusive mask preserves keyboard merging");
  preferred.gamepad = {0, 1, 2, 3, 4, 5, 6};
  keyboard.gamepad = {0, 200, 201, -30000, -30000, -30000, -30000};
  for (uint32_t axis : {P::kLeftX, P::kLeftY, P::kRightX, P::kRightY,
                        P::kLeftTrigger, P::kRightTrigger}) {
    auto merged = Fold(preferred, keyboard, axis, axis);
    const auto& pad = merged.gamepad;
    switch (axis) {
      case P::kLeftX: check(pad.thumb_lx == 3, "exclusive left X"); break;
      case P::kLeftY: check(pad.thumb_ly == 4, "exclusive left Y"); break;
      case P::kRightX: check(pad.thumb_rx == 5, "exclusive right X"); break;
      case P::kRightY: check(pad.thumb_ry == 6, "exclusive right Y"); break;
      case P::kLeftTrigger: check(pad.left_trigger == 1, "exclusive left trigger"); break;
      case P::kRightTrigger: check(pad.right_trigger == 2, "exclusive right trigger"); break;
    }
  }
  std::cout << checks << " direct-gear checks passed (production projection/caps/merge, pure inputs)\n";
}
