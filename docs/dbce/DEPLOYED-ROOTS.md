# Plain Play deployed-root fallback

October 23 isolated source branch `nepo/fh1-deployed-roots-20261023`, forked
directly from coherent built source `abd3962d5c2ae101180fbb8d6124344c8da371ae`.
SDK gitlink/release pin remains `e939542634798fe620d254f56382f6b4e8cae9a3`.
This branch is separate from the native direct-gear successor. Existing built
artifacts, owner checkout, installation and saves are unchanged.

Wheelkit's plain Play starts `bin/pinyon_shift.exe` without LaunchBox's root
environment. Previously diagnostics chose `bin/pinyon_shift_state`, while the
SDK had no explicit game-data root. A complete deployed layout already has the
correct `game/` and `state/` beside `bin/`.

Early diagnostics now resolves the deployed root once, before creating any state
subdirectories, only when both `PINYON_SHIFT_GAME_ROOT` and
`PINYON_SHIFT_STATE_ROOT` are absent. The sibling `game/default.xex` must be a
regular file and sibling `state/` an existing directory. Both roots are then
selected together. Either explicit variable disables the whole fallback, so a
game-only override retains the previous default state root and a state-only
override leaves game-root selection with the SDK. An incomplete, inaccessible or
wrong-type layout keeps the original behavior. The existing environment-path
reader still handles normalization. The game root is passed through the existing
`OnConfigurePaths`; `LongHostPath` on the user root is retained.

The probe neither creates nor migrates sibling state. Normal diagnostics may
create its usual subdirectories after choosing the root. Existing
`bin/pinyon_shift_state` data, if any, remains untouched and is not merged into
the selected deployed state. Muzu's read-only deployment receipt found no such
legacy tree in the current owner install. No install was read or changed by this
fixture run, and no automatic Wheelkit activation was added: without the private
adapter environment, plain Play still uses ordinary input.

## Focused inert check

```powershell
python tests/deployed_roots_test.py `
  --cxx 'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/Llvm/x64/bin/clang++.exe' `
  --out .local/deployed-roots-fixture
```

**17 checks pass** against the real resolver and extracted production early-root
initializer. Only executable location and environment lookup are faked. Checks
cover complete/incomplete layouts, explicit roots individually/together, present
empty environment, file/directory marker types, no state creation and unchanged
scratch sentinel. All files are under one runner-owned temporary directory;
no game data, actual environment, native provider or application is used.
`OnConfigurePaths` wiring and retained `LongHostPath` are checked statically.
Log: `.local/deployed-roots-fixture/check.log`.
The documentation site builds 37 pages and includes the source-handoff link.

An initial scratch fixture terminated in a C++ filesystem cleanup operation after
16 successful checks. The final fixture creates independent layouts and leaves
cleanup to Python's temporary-directory owner. Production performs only the
read-only regular-file/directory probes; no remove/rename operation was added.

## D3D12 tearing and delivery gate

The LaunchBox bat sets `REX_D3D12_ALLOW_VARIABLE_REFRESH_RATE_AND_TEARING=false`.
SDK `src/ui/d3d12/d3d12_presenter.cpp` uses that flag when selecting/recreating
the D3D12 swap chain. It is not used by Vulkan. App default configuration already
writes `gpu_backend="vulkan"` and
`d3d12_allow_variable_refresh_rate_and_tearing=false`; Muzu also reported those
values in the existing deployed config. Thus the missing bat-only variable does
not affect current Vulkan plain Play. A user who explicitly enables tearing and
selects D3D12 can see a difference from the bat's forced-false launch; this patch
preserves the selected configuration rather than forcing a renderer flag.

This is source/fixture evidence only. A coherent app build and explicit redeploy
are still required; the frozen `abd3962d/e939542` executable does not contain this
fix. No product build, game launch, device/display operation, owner-state write or rig
lease was performed. Build admission and future owner-noticed Play/wheel
qualification remain separate gates.
