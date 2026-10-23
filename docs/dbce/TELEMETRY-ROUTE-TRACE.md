# FH1 telemetry producer, SimHub consumer and remaining boundary

October 23 source/read-only trace at built app
`b7e36099e21e5352d264b4f8548864e55f183195` / SDK
`e939542634798fe620d254f56382f6b4e8cae9a3`. The frozen build tree is unchanged.
This document describes that built revision. The later, unbuilt
[transport source fix](TELEMETRY-TRANSPORT.md) addresses its blocking sender and
normal-close/disable gaps separately; it is not part of that binary.
**The four requested channels already have a built-in route.** No new bridge,
game hook, worker or custom dashboard protocol is required for speed, RPM, gear
and the existing driving/race-on flag. This is source/consumer-format evidence,
not live FH1-to-SimHub reception or physical dashboard acceptance.

## Producer to wire

| Boundary | Exact source anchor | Behavior |
| --- | --- | --- |
| Frame hook | `config/rexglue/analysis/main-xex.toml:763`, v4 `analysis-v4/main-xex.toml:765` | Hooks `PinyonShiftTraceFrameTelemetry` at base `0x823EDA10`, v4 `0x8241DAD0`. |
| Scheduled read | `src/pinyon_shift_runtime_hooks.cpp:6749`, `:6783` | Calls QueueCarTelemetry, then RunGuestTasks in the frame hook. The unrelated M4 JSON tracing environment gate is not required for UDP telemetry. |
| Enable gate | `QueueCarTelemetry:6115` | Requires a live SDK InputSystem, then DualSense effects/lightbar demand OR telemetry Wanted. Normal SDL input provides that system; a wheel or the private adapter is not required. |
| Car lookup | `:6126-6148`, existing `site/findings/objects.toml` player-car/car-dynamics records | Readable-pointer checks follow holder `0x832DF024` to manager/world, call player-slot accessor `0x8248BAE8`, then car and dynamics at car `+0x2D90`. Driver type `+11728` must be player 0. No player sends an inactive sample. |
| Values | `car_dynamics:6062`, `QueueCarTelemetry:6158-6192` | Dynamics speed `+3128` is m/s; engine speed `+324` and redline `+452` are rad/s, multiplied by 9.5492966 for RPM. Gear byte `+2240`: 0 reverse, 1..8 forward, >=9 neutral. These existing source layouts are recorded already; this trace adds no guest offset. |
| SDK consumer | `input->SetCarTelemetry(t):6184` | Feeds SDK adaptive-trigger/lightbar logic in-process. This is distinct from the external UDP sender owned by the app. |
| Packet encoder | `src/telemetry/forza_dash.cpp:65`, `EncodeDash` / `EncodeHorizon:102` | Explicit little-endian bytes, FM7 Dash 311 or Horizon 324. Horizon inserts 12 bytes at 232 and retains the trailing byte at 323. |
| Transport | `src/telemetry/telemetry_out.cpp:23-37`, `Send:124`, `Open:57` | Default OFF. The enabled guest task encodes a sample and sends one datagram to the configured target. |

Requested field mapping:

| Channel | FM7 offset | Horizon offset | Units/encoding |
| --- | ---: | ---: | --- |
| Race/driving flag | 0 | 0 | s32 IsRaceOn; 0 inactive, 1 driving |
| Engine RPM | 16 | 16 | f32 RPM; idle/redline at 12/8 |
| Speed | 244 | 256 | f32 m/s; signed forward velocity also at 40 |
| Gear | 307 | 319 | u8: reverse 0, neutral 11, forward 1..8 |

The race flag is a **driving heuristic**, not a competition-session enum. It is
true for a player car with the gameplay view, including free roam. Narrow/menu
views suppress it; ten simulation values held exactly still for 500 ms suppress
it too. Optional moving-only suppresses it below 0.5 m/s. There is no event phase,
start-light state, lap/position or real track ID in this route; untraced fields
stay zero. Nonfinite engine RPM/redline currently returns without sending.

## Existing configuration

The same SDK hot-reload cvars are used by the app sender, F6 toggle and launcher:

```toml
telemetry_udp = false                  # built-in default
telemetry_udp_format = "fm7"          # or "horizon"
telemetry_udp_target = "127.0.0.1:5300"
telemetry_udp_moving_only = false
```

F6 `src/ui/settings_menu.cpp:1068` exposes TELEMETRY OUT (UDP). The launcher's
`launcher/PinyonShift.Launcher/Wheel/Bindings/TelemetrySettings.cs` reads/writes
all four keys, with the same 5300 default. `WheelSetupView.xaml.cs:692-702` saves
the Telemetry card. The enable key is a feature opt-in, not a Wheelkit/force flag.
No config was changed here.

The existing route to use is **matching format plus matching receiver port**:
FM7/311 with SimHub's Motorsport 7 profile, or Horizon/324 with its FH4/FH5
profile. For this rig the previously accepted shared route used port 8000;
5300 is the Pinyon default, not proof of SimHub's current effective port.
Keep the default OFF and set the chosen target explicitly during a later
consumer qualification. No automatic receiver detection or extra relay is needed.

## Installed consumer, inspected without execution

REA static PE/CLI metadata and CIL inspection identifies:

| Artifact | SHA-256 | MVID |
| --- | --- | --- |
| `GSIReader.dll` | `39046E0CFAE85D837A2307507EC3BDB8C62563EAA5EFC1AEAB8A6CE821D3AB79` | `013fa617-6302-498f-84ca-00c7c36f2312` |
| `GameReaderCommon.dll` | `7622C5E36D629780CBD4AA9BF6058CF30A2DEBFF19F7A273B8E78B1A598802C7` | `a862d4de-956b-4a2e-8495-e51fab552bd9` |

Both are under `C:/Program Files (x86)/SimHub`. In GSIReader's
`GSIReader.ForzaMotorsport7` types, build-local method tokens show:

- `FH4Manager.get_PacketLength` `0x06000856` and FH5 `0x06000858`: 324;
  FM7 `0x0600085a`: 311. `ForzaReader<T>.Forzareader_OnAcceptBytes`
  `0x06000867` checks the incoming length against the selected length.
- `ForzaManager<T>.GD_SpeedKmh` `0x060008c8` reads Dash.Speed and multiplies
  by 3.6. GD_Rpms `0x060008c6` reads Common.CurrentEngineRpm; GD_MaxRpm
  `0x060008c5` reads EngineMaxRpm.
- GD_Gear `0x060008c3` reads Dash.Gear with special branches for 0 and 11.
- IsGameInRace `0x060008cc` reads Common.IsRaceOn. IsGameInMenus and
  IsGamePaused return false; the race flag does not separately encode those
  states. IsGameRunning `0x060008d0` asks the UDP reader's IsRunning, not an
  FH1/Forza executable-name test. Global automatic profile selection and actual
  dashboard rendering were not executed or proved.

Read-only current settings: `PluginsData/GameSettings.json` has UDPPort null
for FM7/FH4/FH5 (no explicit per-game override); FH5 forwarding is enabled to
127.0.0.1:20055. `GlobalSimhubSettings.json` has AutoDetectGame true;
CustomGames is empty. Their hashes are retained locally. No SimHub process was
found in the scoped process query. Thus there is **no measured live listener or
current effective-port proof** here. Do not silently enable/forward a test feed
to a hardware consumer. The first input packet uses isolated 55301 instead.

[SimHub's official configuration/troubleshooting guide](https://github.com/SHWotever/SimHub/wiki/SimHub-Basics----Games-config-and-troubleshooting#forza-horizon-5--forza-horizon-4--motorsport-7)
documents Forza UDP support and notes that connection is normally checked in a
race. Its Windows Store AppContainer advice concerns the retail Store app;
this trace did not change exemptions, firewall, receiver settings or launch apps.

Private retained REA evidence in `.local/telemetry-trace/` has complete metadata
coverage but no CLR loading, dependency resolution, C# execution or live decode:
GSI members `ev_cb2e2b05a8e4e7d3696e1482812958714d9aedb27fe6eb328d7a701275e19b38`,
common members `ev_e1bf201bf17d24ef325989cbd9505a3806e6212f61681dca025084298f93d5ca`.
Large MCP responses used complete retained evidence exports; no truncated result
was treated as complete. Raw CIL/binaries remain private, outside tracked docs.

## Concrete gaps and smallest follow-up

There is no missing producer/packet mapping for the four requested fields.
Actual HUD/vehicle correlation, rate, menu/pause/free-roam behavior and SimHub
dashboard reception still need an authorized run. If "race state" means event
phase or lap state, that is untraced data and must not be guessed from IsRaceOn.

Two bounded transport limitations are visible in source:

1. Socket creation leaves the socket blocking; Send calls sendto synchronously
   under its mutex on the guest task. Initial getaddrinfo is synchronous too.
   A receiver's absence usually does not block UDP, but this code has no
   nonblocking guarantee under pressure. Keep resolution outside the frame send
   where practical, set nonblocking at socket creation, fail closed if setup
   fails, and drop would-block/errors without retries or a new worker.
2. Turning telemetry off returns immediately, and no shutdown/close API or
   final inactive datagram is registered. Menu samples are inactive while the
   hook runs, but disable/exit does not explicitly park the last destination.
   Receiver timeout/parking must be qualified. A small later change can issue a
   best-effort inactive sample before disabling/changing target/normal close,
   using the existing close path and nonblocking sender. Crash/kill cannot
   guarantee a final datagram. A failed target is currently sticky until the
   target string changes, as Open's comment explicitly says.

These are separate narrow sender improvements, not reasons to add a bridge.
A proposed pure fake-socket seam should compile the production Open/Send logic:
disabled does no WSA/DNS/socket work, exact 311/324 payloads, setup failure closes
owned handles, would-block drops once, changing target releases the old socket,
and disable/normal close requests one inactive sample without waiting. Do not
bind SimHub's port or use a game/device for that fixture.

The existing `tests/telemetry/forza_dash_test.cpp` plus production encoder already
covers inactive frames, requested field offsets, neutral/reverse, finite-value
handling and Horizon length. Telemetry sources/tests are byte-identical to the
previously checked `f39a3d15` integration; accepted codec checks were not repeated.
This trace changes documentation only. The coherent build also compiled these
unchanged production sources; it did not prove their live guest values.
