#pragma once

#include "telemetry/forza_dash.h"

namespace pinyon_shift::telemetry {

// Host setup/configuration thread only. They post the target and return: a resolver thread looks it up and opens the
// socket, Send adopts it; neither these nor Send ever wait for a lookup.
void Initialize();
void Configure();
// Best-effort inactive datagram, then close. Queued Configure cannot reopen it; a lookup still in progress is not
// waited for and only frees what it made.
void Shutdown();

// Whether telemetry_udp is on, so the per-frame car read runs for it.
bool Wanted();
// telemetry_udp_moving_only: send "race off" while the car stands still.
bool MovingOnly();

// Sends one sample as a Forza Data Out Dash packet to telemetry_udp_target
// (#481). Called once per frame from the guest task that reads the car.
void Send(const CarSample& sample);

}  // namespace pinyon_shift::telemetry
