# Cleanup and release handoff (2026-10-13)

The "rename, clean up, package" pass on both games is finished. This file records what it changed
and what is open. `docs/odin/HANDOFF.md` and `docs/unicorn/HANDOFF.md` cover the ports themselves.

## Branches

- `odin-sphere`: the shared engine and Odin Sphere. Shared changes go here first.
- `unicorn-overlord`: merges `odin-sphere` and adds Unicorn Overlord. Keep merging in that
  direction (odin-sphere → unicorn-overlord).
- `stash@{0}` "uo session 2026-10-13" holds the rejected UO performance experiment (fetch-V#
  snapshot, draw-merge diagnostics). Keep it for reference only; don't apply it.

## What changed

- **GoW3 removed:** cheats, temporal upscalers (FSR 3/4, DLSS, TAA, motion vectors), the GoW3
  launcher, packaging, docs and patches; then the leftovers: unused toggle bits (47, 48, 50-55,
  57), the per-draw `buffers_hash`, live-preset GPU checks, GoW3 text in code, tests and docs.
  `README.pt-BR.md` was deleted; `README.md` describes vwport and both games.
- **Neutral naming:** `gow3` → `vw` (`out/vwport`, `libvwgpu`, `vw.ini`, `VW_*`).
- **Bugs fixed on the way:**
  - The rename broke hidden save copies: `temp_name()` compared 9 characters with the
    7-character `.vwsave`, so crash-safe save copies showed in the game's directory listings.
  - The removal of the temporal stack also removed the per-frame hook: `VW_GPU_PROFILE`
    printed nothing and `VwStats::gpu_frames` stayed 0. `Rasterizer::NoteFrameStart` now runs
    at each flip.
  - `test_sema` asserted the opposite of its own setup; the mp3 test lacked `-lm`.
    `bash build.sh --test` passes again.
  - Mods only accepted GoW3's `dvdroot_ps4/`; they now replace any top-level entry of the game
    folder (`*.CPK`, `game_data/`). See `docs/MODS.md`.
- **One launch path:** `run.sh` sets up the environment, builds, and runs `run.py`. Linux now gets
  the graphics options, save backups and in-game restart. Unicorn Overlord therefore runs with
  deferred and stale readbacks on by default; the battle renders correctly with them.
- **Per-game profiles:** `scripts/game_profiles.py` (Odin: disabled optimizations and readback
  settings; UO: its native PSN modules, formerly `TITLE_MODULES`). `VW_FIXED_TIMESTEP` was dead
  and is gone.
- **Unicorn Overlord render resolution:** `VW_RENDER_SCALE=1440p|1800p|4K` uses the game's PS4 Pro
  path. Checked at 4K and at 1440p (title, menu, battle). The Ghidra survey of the game's
  graphics options is in `docs/unicorn/HANDOFF.md`. The only switch is the Pro check, which
  also loads 4K fonts and UI art.
- **Launcher:** `launcher/vw_launcher.py` is themed per game (`GAMES`). The entry scripts are
  `odin_launcher.py` and `unicorn_launcher.py` (UO: navy and gold, the war-table backdrop). It has
  resolution, display mode, VSync, FPS counter, FSR 1 and sharpening options.
- **Packages:** `python3 packaging/package.py odin|unicorn` writes four files on Linux: the
  launcher (with run.py, scripts and patches inside), a static `vwport` (`build.sh --static`,
  GPU library linked in), README.txt and LICENSE. `vw-gpu-capabilities` became
  `vwport --gpu-device`. Both packages were run from a clean folder.
- **Windows:** `bash packaging/windows/build.sh odin|unicorn` in the MSYS2 CLANG64 shell installs
  the packages, builds and packages (packaging needs a python.org Python). It hasn't been run since
  the cleanup. The GoW3 icon was replaced by `packaging/windows/vwport.ico`.

## Tests (2026-10-13)

- `python -m unittest discover -s tests`: 78 OK, 14 skipped.
- `bash build.sh --test`: all pass.
- `graphics-settings-test` and `perf-overlay-test`: pass.
- Games: Odin Sphere title screen (dev build, static build and package). UO title, battle at
  native and 1440p, and the opening from the package.

## Open

1. **CPU threads (2026-10-13/14).** The question was why only about two threads work on a
   4-core/8-thread CPU when a PS4 runs the game at 60 FPS. On the console the GPU's command
   processor parses the PM4 packets in hardware; the port does that parsing plus the Vulkan
   translation in software, and it is serial because register state carries from packet to
   packet. Research: upstream shadPS4 does it on one GPU thread
   ([issue #1512](https://github.com/shadps4-emu/shadPS4/issues/1512)); cuesta4's fork moved
   copies and recording to helper threads, which this port has (stages A/B); RPCS3 gets 5-10%
   from a transfer thread ([PR #6112](https://github.com/RPCS3/rpcs3/pull/6112)); no emulator
   found that splits one PM4 stream across threads.
   - Done (UO first battle map 31 → ~53 FPS, `docs/parallel_gpu.md` from "Constant engine
     dumps without a drain"): the two stages were waiting on each other (`DumpConstRam`
     drains), then a series of per-draw cuts on both stages.
   - Now: the game paces its frames on end-of-pipe interrupts, and stage A reaches them ~9 ms
     after the game submits; stage A's serial work (~15 ms per frame) sets the frame rate.
   - Next: split stage A. The draw scanner and workers already replay the register state per
     chunk of 48 draws; candidate: workers build most of each draw's packet (register blocks,
     user data, pipeline) and stage A only decodes, verifies checksums and copies guest
     memory in order. Measure with the `EOP latency` and `Guest equeue waits` lines.
   - Removed in the cleanup (2026-10-14): the opt-in texture binding helper (with early draw
     inputs, bit 35), hot pages and async fences, measured neutral or slower.
2. Run the Windows script on the Windows PC and fix what it reports.
3. UO: all imports are implemented and saving works (2026-10-13); next is playing further
   (first real battle to the world map, loading a save, a movie); see its handoff.

## Build notes

- Clean the shell first: the profile exports a retcomm clang sysroot. Unset `CC`, `CXX`,
  `CMAKE_SYSROOT` and `LDFLAGS`.
- `./build.sh` (GCC, LTO) takes about 3 minutes; `./build.sh --static` builds the release
  executable in `out/static/`.
- Don't switch branches or rebuild while a game runs (`run.sh` rebuilds on launch). Stop the
  game with `pkill -x vwport`.
