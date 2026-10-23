# FH1 0.6.1 wheel and telemetry source candidate

Branch `nepo/fh1-v061-wheel-source-20261023`, isolated worktree
`E:/Source/games/.worktrees/nepo-fh1-v061-wheel-source-20261023`.
The containing merge commit is the candidate identity; canonical main is
unchanged at `fdcb05b2d377b6f67c214e9502c6f0731b42cd21`.

## Current upstream and source reconciliation

Independently checked GitHub's repository, release and commit APIs plus
`git ls-remote` on October 23. [Upstream 0.6.1](https://github.com/arcanite24/pinyon-shift/releases/tag/v0.6.1)
was published October 21 and remains the latest release. Main/tag resolve to
`5bdaef53c4c0c9b0dca97680935bafd81e7e2e0e`. This agrees with Muzu's independent
reconnaissance; it replaces the old 0.5.1 baseline for this source candidate.

The import adds upstream's raw wheel/separate USB device input, H-shifter,
XInputdFF/SDL force-feedback and expanded FM7/Horizon telemetry paths. Bindings
are edited through upstream's launcher Wheel screen and in-game settings. No
R12 or DS-8X device identifier, axis, button, force sign or gain was guessed.
The original shared Wheelkit profile adapter remains a separate unimplemented
design, not an alternate implementation silently combined with upstream input.

The sole merge conflict was the startup default-config helper in
`src/pinyon_shift_app.cpp`. Resolve it to upstream's exact version: upstream
now includes the fork's no-schema default repair and its own later migrations.
All app/launcher/source, site, release/toolchain configuration and existing
tests match upstream byte for byte. Fork changes relative to upstream are only
the existing ignore exception/design notes and this handoff/status clarification.

The paired ShiftGlue gitlink, release-toolchain revision and private SDK
checkout are all `59bc11f546a6ea67eefcad974d0c14041b2ab20d`. The SDK is a detached
worktree, not a change to the canonical SDK checkout at `1e8968f7`. The existing
`vunu/fh1-v060-20261017` and `feat/wheelkit-snapshot` candidates are unchanged.
The SDK's `docs/RACING_WHEELS.md` is the source contract for the native path.

## Focused checks

Ignored logs and the exact test commands are under `.local/nepo-v061/`:

- `focused-initial.log`: actual startup gate/config/file services compiled
  using the retained LLVM20.1.8/MSVC x64 environment; all 14 existing
  `test_startup_config.py` tests pass, with no skips. The existing
  `tests/telemetry/forza_dash_test.cpp` plus production encoder also compile
  and pass (FM7/Horizon layouts, controls, wheels and inactive state).
- The initial isolated wheel link omitted the effect decoder and failed;
  this was a test-command dependency error, retained in that original log.
  `run-wheel.ps1` adds production `src/input/xinputd_ffb.cpp` and its existing
  decoder tests. `wheel-retry.log`: 195 assertions, 21 cases pass, no skips.
  Tests use the exact SDK wheel mapping/decoder sources and existing unit
  tests. Pure pad-name/parser helpers are extracted verbatim from
  `pad_remap.cpp` to avoid the unrelated cvar runtime; no SDL/device is loaded.
  Catch2 `88abf9bf` amalgamation and fmt `407c905e` headers are read from the
  matching clean donor checkouts. No SDK source/test was edited.
- `release-contract.log`: three existing focused tests pass for SDK wiring,
  literal CMake package inputs and packaged native tool sources. The SDK
  gitlink/release/working-checkout identities were also compared directly.
- `site-build.log`: 37 pages built using the existing pinned site requirements
  in a private virtual environment. The generated playing page's
  `racing-wheels` and `telemetry-out` anchors resolve. Tracked Markdown links
  pass. The owned diff passes whitespace checks. Two imported upstream blank
  lines at EOF remain in the garage render route and `site/findings/audio.toml`.

These are focused source/fixture checks, not a complete game or SDK build.
The full SDK/unit host target, native launcher, rendered controls, real device
delivery, force direction/strength, telemetry receiver and triples are untested.

## Custody and next step

No game, window, hardware, rig lease, install or owner settings/save was used.
All owned check processes exited; no process/resource custody remains.
The installed executable still hashes to
`38E9C7235057CA7F63447291907485EB01D80CD8DA5131AC7E8433C74CCEBAB1` and its
build receipt to
`A597DD901275EB777FAF925FE6CE466DD7A60C98E66B4B3071781B2230617A08`, matching
the earlier owner handoff. No private dependency, generated game data, test
binary or owner config is included in this commit.

Next: a separately coordinated full build of this exact source/SDK pair,
followed by install disposition and a save-safe, force-muted runtime handoff.
Real device bindings and attended force-sign/strength acceptance remain open.
Upstream itself labels wheel FFB experimental; this source import does not
qualify a direct-drive base. Linked-console triples remain research only.
