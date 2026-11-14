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
// Once the presenter and the window exist (UI thread): the F6 panel on the presenter's Vulkan frames, on the
// graphics provider's Vulkan device. open_game_menu opens the game's own settings menu (decision 0066: F6 is
// Wheelkit's panel, which has a row for the game's menu).
void Attach(rex::ui::Presenter* presenter, const rex::ui::vulkan::VulkanDevice* device, rex::ui::Window* window,
            std::function<void()> open_game_menu);
// The player's car, once per frame from the host's guest task (telemetry/telemetry_out.cpp). True: the runtime sent
// it, and upstream's own sender stays quiet.
bool Telemetry(const telemetry::CarSample& sample);
// The player's vehicle pose (position and forward axis, world), from the title's pose hook.
void VehiclePose(float x, float y, float z, float forward_x, float forward_y, float forward_z);
// The game's view while the player drives (its camera controller's mode: 0 to 4 the player's views), or -1.
void Camera(int mode);
void Stop();

}  // namespace pinyon_shift::wheelkit
