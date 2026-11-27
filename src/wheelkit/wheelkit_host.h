#pragma once
// The Wheelkit runtime's loader and call sites (Wheelkit's fork; not part of upstream Pinyon Shift). wheelkit.dll,
// beside the program, owns the wheel, the force, telemetry and the F6 panel: the rig profile's wheel, pedals and
// shifter reach the game as player 1's Xbox 360 racing wheel, the game's own wheel-force effects go to the runtime's
// force layer, and the car the host already reads goes out as telemetry. Without wheelkit.dll every function here
// does nothing and the game is upstream's.

#include <functional>
#include <memory>
#include <string>

namespace rex::input {
class InputDriver;
class DeviceAssignment;
}
namespace rex::ui {
class Presenter;
class Window;
namespace vulkan {
class VulkanDevice;
}
}
namespace pinyon_shift::telemetry {
struct CarSample;
}

namespace pinyon_shift::wheelkit {

// Loads wheelkit.dll beside the program and starts the runtime (before the window and SDL). False without it.
bool Start();
bool On();
// The rig profile's devices as SDL_GAMECONTROLLER_IGNORE_DEVICES wants them ("0xVVVV/0xPPPP,..."), so SDL in the
// game never opens a device the runtime reads (one reader per device: SDL opening a force wheel resets its force).
// "" without a profile.
std::string SdlIgnoreList();
// Player 1's racing wheel from the rig profile, and the game's wheel-force effects. Null without a profile.
std::unique_ptr<rex::input::InputDriver> CreateDriver();
// Player 1's pads with a profile: the wheel and keyboard/mouse emulation only, no other pad merged in. Null without
// a profile (the game's own shared assignment stays).
std::unique_ptr<rex::input::DeviceAssignment> CreateAssignment();
// Once the presenter and the window exist (UI thread): the F6 panel on the presenter's Vulkan frames, on the
// graphics provider's Vulkan device. open_game_menu opens the game's own settings menu (decision 0066: F6 is
// Wheelkit's panel, which has a row for the game's menu).
void Attach(rex::ui::Presenter* presenter, const rex::ui::vulkan::VulkanDevice* device, rex::ui::Window* window,
            std::function<void()> open_game_menu);
// The player's car, once per frame from the host's guest task (telemetry/telemetry_out.cpp). True: the runtime sent
// it, and upstream's own sender stays quiet.
bool Telemetry(const telemetry::CarSample& sample);
// With three views on the rig's screens: the vertical field of view (radians) for a game camera whose own is game_fov_y,
// by the lens of the view on screen (decisions 0073 and 0092, Wheelkit's cameras; F7 switches it, the numpad zooms):
// the seat's true view (Realistic: the cockpit and bumper views, and every camera outside the five views), or the
// game's own (Game: the chase and hood views), plus the view's zoom. False: the game's own, untouched (no three views).
bool ViewFovY(double game_fov_y, double& fov_y);
// True once the game draws three views over the rig's screens (ShiftGlue's wk_three_views on a spanned window): the
// title's culling then keeps what the side screens see.
bool ThreeViews();
// The player's car's body vectors from its dynamics (+2832, +2848, +2864, +2880, +2896: x, y, z each).
void Body(const float* vectors);
// The game's view while the player drives (its camera controller's mode: 0 to 4 the player's views), or -1.
void Camera(int mode);
// Test switch ai-drive.on (unattended runs only): the game's own AI drives the player's car.
bool AiDrive();
// Test switch gearbox-trial.on: try the candidate transmission assist field.
bool GearboxTrial();
// The game's Shifting assist the rig profile implies: 1 Manual (a hand-shift profile), 0 Automatic, -1 leave it.
int WantedShifting();
// One line into the runtime's log.
void Note(const std::string& line);
void Stop();

}  // namespace pinyon_shift::wheelkit
