# Private Wheelkit adapter on the FH1 0.6.1 source candidate

October 23 source integration, branch/worktree
`nepo/fh1-wheelkit-v061-integrated-20261023`. It combines upstream-integration
candidate `f39a3d1502c9d2090cda0ee88895548c76258d08` with preserved adapter
`46a129ec` and the pinned-shim/fixture successor `e47c3546`.

The isolated candidate's SDK gitlink and release revision now name local
`4329f0e12d8c75012b9fb38322d72369618da581`. The paired SDK checkout is a
detached worktree under `thirdparty/shiftglue-sdk`; owner main, original SDK
checkout, installed binaries/configs and saves are untouched. SDK source remains
local only: its sole remote is upstream, which does not carry these private
prerequisites. No portable-source or public-release claim is made.

## Behavior retained

- Ordinary builds keep `PINYON_SHIFT_PRIVATE_WHEELKIT` off. The existing build
  route accepts explicit `-PrivateWheelkit` for a private candidate and records
  that compiled capability plus the shim pin/hash in its build metadata.
- Even that build creates no adapter without `PINYON_SHIFT_WHEELKIT=1`, an
  explicit profile and an existing scratch report directory. Tool mode and
  scripted render routes keep their previous paths. The profile/schema 2 reader,
  calibration, raw bytes and owner-thread immutable snapshot behavior are reused;
  no profile writer, migration or renamed persisted key was added.
- The existing factory wrapper now lives in `wheelkit_app_input.cpp`. Its one
  app caller supplies native preparation; fixtures supply the existing fake
  provider. Preparation precedes the base input factory and path admission
  precedes window attachment. Refusal keeps ordinary input and stops its owned
  adapter candidate. SDL gamepad and raw-wheel exclusions use the paired SDK.
- Focus, presenter-frame notification, close, shutdown and destructor hooks are
  integrated. The merge retains 0.6.1 HDR recovery, frame-inspector lifecycle,
  telemetry/DualSense feedback and save/shutdown work. Native wheel/FFB defaults
  and guest configuration files are unchanged from f39a3d1.
- The exact 16-file `ee8014d4` shared-shim closure, MIT/provenance and package
  metadata are retained from [the paired priority candidate](WHEELKIT-V061-PRIORITY.md).
  F6 standalone shared-profile writing and host settings-command delivery remain
  open; this adapter source does not claim those features.

## Focused gate

The new factory tag plus retained final-feedback regression pass **80 assertions
in two cases** against current SDK InputSystem and the pinned host/Core. Cases
cover disabled/tool mode, bootstrap exception, admission before native window
attachment, refusal after attachment, a null base result, an absent factory,
unrelated pad preservation and resolved candidate shutdown. No real SDL backend,
DirectInput device, game, window or owner profile is instantiated by this fixture.

Three exact-closure/build-switch checks and two existing source-package contract
checks pass. The pin-to-HEAD check must run after this integration commit: before
commit, HEAD still records f39a3d1's `59bc11f5` gitlink while the staged candidate
and paired checkout record `4329f0e`. The build scripts and local runner parse.

The old optional full-app object fixture stopped at a retained generated-header
API mismatch: older `pinyon_shift_pch.h` declares a mutable `PPCFuncMappings`,
while the current SDK and generator template declare it const. Owner-generated
files were not edited or substituted. The queued coherent build must compile
the current generator and regenerate all three private guest module trees before
its SDK/app compile. This object check is not reported as passing.

Evidence is in `.local/nepo-integrated-fixture/`: configure/build logs and the
factory/feedback pass. Earlier 1,270-assertion priority/driver/integration and
87-check pre-open results remain in their original worktrees with their original
scope; they were not relabeled as an integrated game build.

## One coherent Windows build

The prepared local recipe is `.local/nepo-integrated-build/Invoke-CoherentBuild.ps1`
with `Build-Payload.ps1` and pinned shared process-identity/lease helpers. Enter
only after the coordinator confirms exact predecessor EXIT, cleanup and the new
slot. It atomically acquires the existing shared lease and refuses an existing
lease; no old lock is deleted or reclaimed. An existing output attempt is retained.

Within that lease the payload makes independent dependency checkouts at the SDK's
exact Git pins from the already populated local object stores, reads the retained
toolchain through a private junction, and copies only the three required XEX inputs
into ignored private paths. It neither copies nor opens an owner save. It runs:

```powershell
./tools/build-preview.ps1 -Configuration Release -CpuBaseline sse4.1 `
  -Parallel 4 -PrivateWheelkit -JsonEvents
```

The existing route builds the SDK generator, regenerates private guest code,
builds the SDK/runtime/plugins and app coherently, then writes exact source,
executable, SDK, shim, compile-activation and guest-input provenance. The wrapper
uses the shared owned-process census, retains custody on uncertain cleanup,
and releases only its exact lease after verified quiescence. No game, screens,
input injection, device use or install is part of this build.

## Qualification handoff after build

No build or runtime acceptance is implied by the source gate. After a successful
build, record executable/runtime/manifest hashes and the exact candidate, then
obtain the fresh coordinator/owner screen handoff for a save-safe qualification.
Use an isolated state copy/seed through the existing launcher; never pass the
owner's live save as a writable test state. Record a read-only schema 2 controls
profile copy and dedicated scratch reports. Keep unattended physical output muted
(`wheel_ffb_strength=0`, `wheel_ffb_effects=false`, no auxiliary force/motion);
attended force assessment requires its own explicit handoff. Verify actual
enumeration/arrival isolation, neutral/rearm and delivered steering/pedal behavior
before making device or driving claims. Preserve owner display/config/save state
and release owned processes/handles before releasing the runtime lease.
