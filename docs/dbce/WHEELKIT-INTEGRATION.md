# Private FH1 input integration candidate

This source slice connects the reviewed driver/lifecycle to the existing app.
It has not been installed or run with a game, native provider, wheel, or UI.
The owner SDK gitlink, toolkit shipping pin and public refs are unchanged.

`PINYON_SHIFT_PRIVATE_WHEELKIT` defaults **OFF**. A private Windows build must
explicitly supply `REXSDK_DIR` and `PINYON_SHIFT_WHEELKIT_SOURCE`. Even that build
does nothing unless `PINYON_SHIFT_WHEELKIT=1` and explicit profile/report paths
are supplied through `PINYON_SHIFT_WHEELKIT_PROFILE` and
`PINYON_SHIFT_WHEELKIT_REPORT_DIR`. The report directory must already exist and
be disposable. These are private qualification inputs, not a distribution or
standalone F6 settings route. Tool mode, scripted render tests and alternate
native input backends retain their existing factory.

The wrapper prepares before invoking the existing input factory. The same input
owner thread creates the shared provider, enumerates, opens/closes only
`plan_profile`'s explicit bound instances, and verifies product plus nonempty,
unique opened interface paths. It performs no poll/state read. A bounded,
resolved provider shutdown is required before the sampling epoch is created.
Readiness uses the existing lifecycle condition variable and bounded stop path.

The SDK's optional per-driver path policy is installed before `AttachWindow`.
Its SDL implementation checks exact case-insensitive interface paths before
`SDL_OpenGamepad`; it refuses installation after attachment. It does not compare
names, SDL model GUIDs, or VID/PID. With isolation enabled a pathless SDL candidate
is withheld individually. Verified nonmatching paths remain ordinary SDL input.
There is no blanket invalidation for unrelated controller topology changes.

The title adapter adds its synthetic driver and user-0 preference while retaining
the default slot routing, keyboard, host input capture and chord paths. The
existing touch wrapper runs afterward. Presenter `UIDrawer::Draw` supplies only
an increasing frame notification; provider access stays on the existing shared
host/input workers. Focus uses `OnWindowFocusChanged`. Both `OnShutdown` and the
SDK's hard-exit `OnWindowCloseRequested` path detach the frame drawer, neutralize
delivery and perform bounded cleanup. Initialization failure/destruction uses the
same cleanup. An unresolved stop is reported, not treated as resolved.

Native provider and driver use the same system QPC origin, frequency and integer
microsecond conversion. The provider guard checks every reopened bound instance's
product and interface path before admitting new samples. Same-path disconnects
retain Core's epoch/rearm logic. Changed, missing or ambiguous bound identity/path
closes sampling and snapshot admission and requires a new bootstrap.

The guard does not give SDL an independent mapping from an unknown new path
to a DirectInput instance. Detection at the guarded provider's opened epoch must
not be described as proof of classification at an earlier SDL arrival. A snapshot
already returned to a caller also remains immutable. This ordering is part of the
remaining runtime gate, not an acceptance claim. Source review (Sana,
hcom 195942) accepted this private object/fake stage with that limitation;
real-device activation/release is blocked until changed-path ordering is
qualified. The shared source commit is `2c427444238c3b4be10f55284b736d0ae1506fcb`,
isolated SDK branch `feat/title-input-priority`. It changes the driver virtual
interface and therefore requires a coherent rebuilt SDK/consumer, never an old
runtime binary mixed with the new driver in a game.

## Focused verification

Prepared target: `pinyon_wheelkit_app_compile` compiles the actual changed app
hook, native bridge, shared provider and SDL backend against existing read-only
generated FH1 headers. It does not link/run the game or regenerate guest code.
The focused executable rebuilds the current InputSystem and adapter sources and
uses the existing scripted fake provider plus inspected ancillary SDK services.
Only `[wheelkit-integration]` is selected; earlier completed suites are retained.

The two new cases cover default-disabled behavior, owner-thread preflight with
no polling, filter-before-attach ordering, candidate quarantine, native keyboard
and unrelated pad preservation, focus/rearm, same-path reconnect, changed bound
opened-path refusal, bootstrap identity/path/close failures, and unavailable
native-path policy. The affected ClangCL 19.1.5 `/W4` Release objects and focused
executable built successfully. The selected check passed **928 assertions in
two cases** (0.08 s); readiness-loop assertions vary with scheduling. No earlier
suite was rerun. Warnings included existing unused parameters/deprecated
`sscanf` and two fixture aggregate initializers defaulting the new admission
callback; this was not a warning-free build.

Original evidence in the app worktree:

- `.local/wheelkit-integration-checks/build.stdout.log` and `result.json`;
  owned child processes quiescent, actual lease release 21:36:16 UTC.
- `.local/wheelkit-integration-build/Testing/Temporary/LastTest.log`, SHA-256
  `404F56407ECE8499194FCA4A998F60F92A466738BA844508EF974591040D6523`.
- Fake executable SHA-256
  `FE6F1FFF4CDAF1766247336458EB9B71E204B82B50BC81ED996338218B2BCF48`.
  Import inspection found no native provider/backend constructor imports.
- Two initial harness include failures (`renderdoc_app.h`, then `pinyon_mod.h`)
  are preserved in `first-failure/` and `second-failure/` under the checks folder.
  Neither reached a fake test; the include list was corrected before the pass.

The object target compiled the actual `pinyon_shift_app.cpp`, native bridge,
shared `dinput_provider.cpp` and changed SDK `sdl_input_driver.cpp`; it did not
link an application or claim the ancillary runtime used by fake tests is a
rebuilt integrated SDK. The production CMake option remains OFF by default.

Full rebuilt SDK/game linkage, distribution pinning, standalone F6 writer,
settings-command queue, attended identity/hotplug ordering and actual driving
remain unqualified. Force output remains unavailable; vibration is not torque.
