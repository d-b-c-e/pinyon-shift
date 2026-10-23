#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include <rex/input/input_system.h>
#include <rex/ui/ui_drawer.h>

#include "input/wheelkit_input_driver.h"

namespace rex::ui { class Presenter; }
namespace rex { struct RuntimeConfig; }

namespace pinyon_shift::input {

struct WheelkitAppInputOptions {
  bool enabled{};
  bool native_direct_gears{};
  WheelkitLifecycleOptions lifecycle;
};

// Private integration. Prepare runs before the base input factory; Attach runs
// before AttachWindow. Neither the default-disabled path nor tool mode creates
// a provider. Sampling starts only after both admission and a presenter frame.
class WheelkitAppInput final : public rex::ui::UIDrawer {
 public:
  WheelkitAppInput(WheelkitAppInputOptions options,
                  WheelkitInputLifecycle::ProviderFactory factory,
                  WheelkitInputDriver::Clock clock);
  ~WheelkitAppInput();
  bool ready() const;
  std::string reason() const;
  bool Attach(rex::input::InputSystem& system);
  void AttachPresenter(rex::ui::Presenter& presenter, bool focused);
  void SetFocused(bool focused);
  void Draw(rex::ui::UIDrawContext&) override;
  bool Stop();
  std::shared_ptr<WheelkitInputLifecycle> lifecycle() const { return lifecycle_; }

 private:
  struct Admission;
  std::shared_ptr<Admission> admission_;
  std::shared_ptr<WheelkitInputLifecycle> lifecycle_;
  WheelkitInputDriver::Clock clock_;
  bool native_direct_gears_{};
  rex::ui::Presenter* presenter_{};  // UI thread only
  bool attached_{}, stopped_{};
  std::uint64_t frame_{};
};

// Native entry point is compiled only in the private Windows build. Uses the
// shared DInputProvider and its exact system QPC microsecond conversion.
std::shared_ptr<WheelkitAppInput> PrepareNativeWheelkitInput(void* window);
std::int64_t WheelkitQpcMicroseconds();

void WrapWheelkitInputFactory(rex::RuntimeConfig& config,
    std::shared_ptr<WheelkitAppInput>& attached,
    std::function<std::shared_ptr<WheelkitAppInput>()> prepare);

}  // namespace pinyon_shift::input
