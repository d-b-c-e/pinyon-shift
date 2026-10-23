#pragma once

#include "telemetry/forza_dash.h"

namespace pinyon_shift::telemetry {

// Host setup/configuration thread only; endpoint resolution never runs in Send.
void Initialize();
void Configure();
// Best-effort inactive datagram, then close. Queued Configure cannot reopen it.
void Shutdown();

// Whether telemetry_udp is on, so the per-frame car read runs for it.
bool Wanted();
// telemetry_udp_moving_only: send "race off" while the car stands still.
bool MovingOnly();

// Sends one sample as a Forza Data Out Dash packet to telemetry_udp_target
// (#481). Called once per frame from the guest task that reads the car.
void Send(const CarSample& sample);

}  // namespace pinyon_shift::telemetry
