# Forza Horizon (Pinyon Shift) — agent guide

Portfolio rules: `E:\Source\dbce-project-mgmt\HOW-WE-WORK.md` (read first; it replaces older process, lease, review and testing instructions). Current focus: `E:\Source\dbce-project-mgmt\NOW.md`. Feature status lives in the portfolio grid (`python E:\Source\dbce-project-mgmt\tools\portfolio_grid.py plan fh1`), not in this file. Grid id: `fh1`.

## Project

Pinyon Shift: the Xbox 360 Forza Horizon recompiled to native C++ (ShiftGlue, a ReXGlue fork) with a Vulkan renderer written for FH1. Windows, Linux, Steam Deck, macOS, Android alpha. Fork of `arcanite24/pinyon-shift`; remote `d-b-c-e/pinyon-shift` (visibility unconfirmed, so do not push). BSD-3-Clause for project files only. The repo holds no game, assets, generated translations or binaries (`docs/LEGAL.md`).

## Build

`.\tools\setup-preview.ps1 -IsoPath <your disc.iso>` (verifies the ISO, installs pinned tools, extracts, generates, builds `out/build/win-amd64-release/pinyon_shift.exe`). Details: `docs/BUILDING.md`. Needs VS 2022 Build Tools and about 30 GB free.

## Test

- `python -m unittest discover -s tools\tests -p "test_*.py"`
- Scripted render tests: take a read-only seed with `tools/create-render-seed.py`, then run `tools/run-fh1-render-test.py --state-root <seed>`. The runner copies the seed per run.
- The live save is `%LOCALAPPDATA%\PinyonShift\source\<version>\.local\preview\user\...\ForzaProfile\ForzaProfile`. Never move, delete, reset or overwrite it; back it up before a run.

## Install / run on the rig

`.\tools\launch-preview.ps1 -StateRoot "$env:LOCALAPPDATA\PinyonShift\source\0.1.0\.local\preview"` (use the newest installed version's `.local\preview`). Check that `$StateRoot\user` has a `ForzaProfile` file and no `pinyon_shift` process is running. Do not use `%LOCALAPPDATA%\PinyonShift` itself as the state root.

## Shared code

No vendored DBCE toolkit copy found here (`thirdparty/` has only `shiftglue-sdk`). Reusable code goes in the toolkit. Change shared code in the toolkit and re-sync; never edit the vendored copy.

## Project-specific rules

- Commits use Conventional Commits (`type(scope): imperative summary`, at most 72 characters, body wrapped at 72, `Tests:` line when run). No AI attribution trailers in commits, PRs or release notes (maintainer rule for this repo).
- Keep the docs site (`site/pages/`, built with `.local/site-venv/Scripts/python site/build.py`) in step with player-visible or measured changes.
- Record reverse-engineering findings (address, name, how established, confidence) in `config/mod/fh1-symbols.toml`, `config/mod/fh1-offsets-v4.toml` or `docs/REVERSE_ENGINEERING.md`, including negative results. Our own analysis only: no disassembly, decompiled code or game data.

## Key docs

- `README.md`, `docs/BUILDING.md`, `docs/ROADMAP.md`
- `docs/LEGAL.md`, `CONTRIBUTING.md`
- `BUGS.md`


<!-- BEGIN project-file-location -->
## Project files and temporary work

Keep project-related research, evidence, scripts, packages, and other working files inside the relevant project folder whenever practical. Keep disposable or machine-specific outputs git-ignored when they do not belong in version control; never ignore files that should be committed merely to hide them. When a project folder is unsuitable, use `E:\Source\temp\<project>` instead of creating scattered drive-root folders. Record the location of material needed by later work and update references when relocating it.

Clean up temporary files and disposable copies promptly once they are no longer useful. Preserve active work, user data, settings, saves, required evidence, backups, rollback artifacts, and worktrees until their retention or replacement is established. Follow existing project, rig, and data-loss safeguards; do not remove files merely because they are old or untracked.
<!-- END project-file-location -->
