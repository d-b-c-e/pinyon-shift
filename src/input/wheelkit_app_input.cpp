#include "input/wheelkit_app_input.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include <rex/ui/presenter.h>
#include <rex/logging.h>
#include <rex/runtime.h>

#include "input/wheelkit_device_assignment.h"

namespace pinyon_shift::input {
namespace sh = dbce::shim;
namespace ri = rex::input;
using namespace std::chrono_literals;

struct WheelkitAppInput::Admission {
  std::atomic<bool> allowed{false};
  mutable std::mutex mutex;
  std::vector<sh::DeviceInfo> identities;
  std::string failure;
  void Refuse(std::string why) {
    allowed.store(false, std::memory_order_release);
    std::lock_guard lock(mutex);
    failure = std::move(why);
  }
  // Same ASCII case folding as the SDK's exact Windows path policy.
  static bool SamePath(const std::string& a, const std::string& b) {
    ri::DevicePathPolicy policy{{a}};
    return !a.empty() && !b.empty() && !policy.Allows(b);
  }

  class Guard final : public sh::ProviderOwner {
   public:
    Guard(std::shared_ptr<sh::ProviderOwner> provider, std::shared_ptr<Admission> admission)
        : provider_(std::move(provider)), admission_(std::move(admission)) {}
    std::vector<sh::DeviceInfo> enumerate() override {
      if (!admission_->allowed.load(std::memory_order_acquire)) return {};
      auto listed = provider_->enumerate();
      std::vector<sh::DeviceInfo> bound;
      for (const auto& expected : admission_->identities) {
        int matches = 0;
        for (const auto& candidate : listed) {
          if (candidate.dev != expected.dev) continue;
          ++matches;
          if (candidate.prod != expected.prod) {
            admission_->Refuse("bound device product changed; new bootstrap required");
            return {};
          }
          bound.push_back(candidate);
        }
        if (matches > 1) {
          admission_->Refuse("bound device instance became ambiguous");
          return {};
        }
      }
      return bound;
    }
    sh::OpenResult open(const sh::DeviceInfo& listed) override {
      if (!admission_->allowed.load(std::memory_order_acquire)) return {};
      auto expected = std::find_if(admission_->identities.begin(), admission_->identities.end(),
          [&](const auto& item) { return item.dev == listed.dev && item.prod == listed.prod; });
      if (expected == admission_->identities.end()) return {};
      auto result = provider_->open(listed);
      if (result.ok && (result.opened.dev != expected->dev || result.opened.prod != expected->prod ||
                       !SamePath(result.opened.interface_path, expected->interface_path))) {
        admission_->Refuse("bound device path/identity changed; new bootstrap required");
        provider_->close(result.handle);
        result.ok = false;
        result.handle = 0;
        result.why = "identity isolation refused the reopened device";
      }
      return result;
    }
    sh::Call reacquire(sh::Handle h) override {
      return admission_->allowed.load() ? provider_->reacquire(h) : sh::Call::Gone;
    }
    void close(sh::Handle h) override { provider_->close(h); }
    sh::Call poll(sh::Handle h) override {
      return admission_->allowed.load() ? provider_->poll(h) : sh::Call::Gone;
    }
    sh::Call state(sh::Handle h, sh::RawState& out) override {
      return admission_->allowed.load() ? provider_->state(h, out) : sh::Call::Gone;
    }
    std::int64_t now_us() override { return provider_->now_us(); }
    bool source_read(sh::Handle h, sh::SourceRead& out) override {
      return provider_->source_read(h, out);
    }
    sh::ProviderShutdownReport shutdown(int ms) override { return provider_->shutdown(ms); }
   private:
    std::shared_ptr<sh::ProviderOwner> provider_;
    std::shared_ptr<Admission> admission_;
  };
};

WheelkitAppInput::WheelkitAppInput(WheelkitAppInputOptions options,
    WheelkitInputLifecycle::ProviderFactory factory, WheelkitInputDriver::Clock clock)
    : admission_(std::make_shared<Admission>()), clock_(std::move(clock)) {
  if (!options.enabled) {
    admission_->Refuse("private adapter disabled");
    return;
  }
  if (!factory || !clock_) {
    admission_->Refuse("explicit provider factory and common clock required");
    return;
  }
  const auto plan = sh::plan_profile(sh::read_profile(options.lifecycle.profile_bytes, 2));
  if (!plan.usable || plan.devices.empty()) {
    admission_->Refuse("profile has no usable explicit bound devices");
    return;
  }
  has_shifter_ = plan.bound_of[sh::action_index("reverse")] >= 0;
  for (int i = 1; i <= 8; ++i)
    has_shifter_ |= plan.bound_of[sh::action_index("gear" + std::to_string(i))] >= 0;
  options.lifecycle.prepare_on_start = true;
  options.lifecycle.admission = [gate = admission_] { return gate->allowed.load(); };
  lifecycle_ = std::make_shared<WheelkitInputLifecycle>(std::move(options.lifecycle),
      [gate = admission_, factory = std::move(factory), devices = plan.devices] {
        // This closure runs only on the existing input owner. Preflight has no
        // poll/state calls and is fully released before another epoch starts.
        if (gate->identities.empty()) {
          auto probe = factory();
          if (!probe) throw std::runtime_error("preflight provider unavailable");
          std::vector<sh::DeviceInfo> verified;
          std::string failure;
          try {
            const auto listed = probe->enumerate();
            for (const auto& expected : devices) {
              const sh::DeviceInfo* found = nullptr;
              for (const auto& candidate : listed) {
                if (candidate.dev != expected.dev) continue;
                if (found) throw std::runtime_error("bound instance ambiguous at bootstrap");
                found = &candidate;
              }
              if (!found || found->prod != expected.prod)
                throw std::runtime_error("bound instance missing or product mismatched at bootstrap");
              auto opened = probe->open(*found);
              if (!opened.ok) throw std::runtime_error("bound identity preflight open failed");
              probe->close(opened.handle);
              if (opened.opened.dev != expected.dev || opened.opened.prod != expected.prod ||
                  opened.opened.interface_path.empty())
                throw std::runtime_error("bound opened identity/path unverified");
              for (const auto& prior : verified)
                if (Admission::SamePath(prior.interface_path, opened.opened.interface_path))
                  throw std::runtime_error("bound interface path ambiguous");
              verified.push_back(std::move(opened.opened));
            }
          } catch (const std::exception& error) { failure = error.what(); }
          const auto stopped = probe->shutdown(100);
          if (!stopped.input_released || stopped.held) failure = "preflight provider close unresolved";
          if (!failure.empty()) {
            gate->Refuse(failure);
            throw std::runtime_error(failure);
          }
          probe.reset();
          { std::lock_guard lock(gate->mutex); gate->identities = std::move(verified); }
        }
        auto provider = factory();
        if (!provider) throw std::runtime_error("sampling provider unavailable");
        return std::make_shared<Admission::Guard>(std::move(provider), gate);
      });
  if (!lifecycle_->WaitPrepared(1500ms)) {
    admission_->Refuse("provider bootstrap failed or exceeded 1500 ms: " + lifecycle_->Status().failure);
    (void)lifecycle_->Stop(0ms);
  }
}

WheelkitAppInput::~WheelkitAppInput() { (void)Stop(); }
bool WheelkitAppInput::ready() const {
  return lifecycle_ && lifecycle_->Status().prepared && reason().empty() && !stopped_;
}
std::string WheelkitAppInput::reason() const {
  std::lock_guard lock(admission_->mutex);
  return admission_->failure;
}
bool WheelkitAppInput::Attach(ri::InputSystem& system) {
  if (!ready() || attached_) return false;
  ri::DevicePathPolicy policy;
  { std::lock_guard lock(admission_->mutex);
    for (const auto& device : admission_->identities) policy.excluded_paths.push_back(device.interface_path); }
  if (system.ConfigureDevicePaths(policy) != 1) {
    (void)system.ConfigureDevicePaths({});
    admission_->Refuse("exactly one unattached native path driver required");
    (void)Stop();
    return false;
  }
  auto driver = std::make_unique<WheelkitInputDriver>(lifecycle_, clock_, has_shifter_);
  const auto wheel = driver->device_id();
  system.AddDriver(std::move(driver));
  system.SetDeviceAssignment(std::make_unique<WheelkitDeviceAssignment>(
      std::make_unique<ri::SlotAssignment>(), wheel, true));
  attached_ = true;
  admission_->allowed.store(true, std::memory_order_release);
  return true;
}
void WheelkitAppInput::AttachPresenter(rex::ui::Presenter& presenter, bool focused) {
  if (!attached_ || stopped_ || presenter_) return;
  SetFocused(focused);
  presenter_ = &presenter;
  presenter_->AddUIDrawerFromUIThread(this, 0);
}
void WheelkitAppInput::SetFocused(bool focused) {
  if (lifecycle_) lifecycle_->SetFocused(focused);
}
void WheelkitAppInput::Draw(rex::ui::UIDrawContext&) {
  if (attached_ && !stopped_) (void)lifecycle_->NotifyFrame(++frame_);
}
bool WheelkitAppInput::Stop() {
  stopped_ = true;
  admission_->allowed.store(false, std::memory_order_release);
  if (presenter_) {
    presenter_->RemoveUIDrawerFromUIThread(this);
    presenter_ = nullptr;
  }
  if (!lifecycle_) return true;
  const auto stopped = lifecycle_->Stop(2s);
  if (!stopped.resolved() && reason().empty())
    admission_->Refuse(stopped.status.failure.empty()
        ? "input owner stop deadline exceeded" : stopped.status.failure);
  return stopped.resolved();
}

void WrapWheelkitInputFactory(rex::RuntimeConfig& config,
    std::shared_ptr<WheelkitAppInput>& attached,
    std::function<std::shared_ptr<WheelkitAppInput>()> prepare) {
  if (!config.input_factory) return;
  config.input_factory = [&attached, prepare = std::move(prepare),
                         base = config.input_factory](bool tool_mode) {
    std::shared_ptr<WheelkitAppInput> candidate;
    if (!tool_mode) {
      try {
        candidate = prepare();
      } catch (const std::exception& error) {
        REXLOG_WARN("Private Wheelkit bootstrap refused: {}", error.what());
      }
    }
    auto input = base(tool_mode);
    if (candidate) {
      auto* system = dynamic_cast<ri::InputSystem*>(input.get());
      if (system && candidate->Attach(*system)) {
        attached = std::move(candidate);
      } else {
        REXLOG_WARN("Private Wheelkit activation refused: {}", candidate->reason());
        (void)candidate->Stop();
      }
    }
    return input;
  };
}

}  // namespace pinyon_shift::input
