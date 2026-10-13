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

1. **CPU threads (next, started 2026-10-13).** The user's question: why do only about two
   threads work on a 4-core/8-thread CPU (Ryzen 5 2500U), when a PS4 runs the game at 60 FPS?
   What does the PS4 do, how does shadPS4 handle it, and what is the equivalent here? The user's
   rule: **search online before any optimization**, and prefer a better design over
   micro-tuning.
   - Known (docs/unicorn/HANDOFF.md): `shadPS4:GpuComm` (PM4 decode, stage A) and `vw:DrawRec`
     (draw recording, stage B) saturate, each about 6 µs per draw, with ~6000 draws per UO
     battle frame. 60 FPS needs < ~2.7 µs per draw.
   - Why the PS4 is different: on the console the game's own CPU threads build the command
     buffers, and the GPU's hardware command processor (the CP, plus ACE queues for compute)
     parses the PM4 packets at no CPU cost. The emulator does that parsing, plus the Vulkan
     translation (state tracking, pipeline lookup, binding), in software. That is work the PS4
     CPU never did, and it is serial because register state carries from draw to draw.
   - Research so far:
     - Upstream shadPS4 processes a submission on its GPU thread, using coroutines for
       interleaving queues ([issue #1512](https://github.com/shadps4-emu/shadPS4/issues/1512),
       [DeepWiki: Liverpool CP](https://deepwiki.com/shadps4-emu/shadPS4/4.2-liverpool-gpu-command-processor)).
     - cuesta4's fork moved staging copies and command recording to helper threads
       ([releases](https://github.com/cuesta4/shadPS4/releases)); this port already has that
       (stage A/B).
     - RPCS3 offloads transfers to a second thread, for +5-10%
       ([PR #6112](https://github.com/RPCS3/rpcs3/pull/6112)).
     - Vulkan allows parallel recording into per-thread command pools and secondary buffers
       ([Vulkan docs](https://docs.vulkan.org/samples/latest/samples/performance/command_buffer_usage/README.html)).
     - No emulator found yet that splits one PM4 stream across threads.
   - **Done 2026-10-13:** the measurement showed the two stages waiting on each other, not
     saturated: `DumpConstRam` drained the draw pipe ~3500 times/s. Those drains are gone
     (vertex V#s now travel in the packet, plus a guard on dumped ranges): UO battle 31 → 39 FPS.
     See "Constant engine dumps without a drain" in `docs/parallel_gpu.md`. Stage B (draw
     recording) is now the limit at ~76-84% busy.
   - Next steps:
     0. Profile stage B now that it is the limit (`perf record` on `vw:DrawRec`).
     1. Measure per-thread CPU in a UO battle (`top -H -p $(pgrep -x vwport)` plus
        `perf record -g` per thread): how busy are the game's own threads (JobManager workers)
        and the helpers?
     2. Research more: yuzu/Ryujinx/Xenia GPU threading, and AMD PAL/RADV's own PM4
        handling.
     3. Candidate design: split a frame's draws across N recording workers. Stage A snapshots
        the register state per draw, as it already does for stage B. Workers record secondary
        command buffers per render pass, or per contiguous draw range inside one, and the
        primary executes them in order. Also: make stage A cheaper by applying only the
        registers that changed. Read `docs/parallel_gpu.md` first; it lists what has been
        tried.
2. Run the Windows script on the Windows PC and fix what it reports.
3. UO: movies (`sceVideodec2*`), `sce::Json`, saving; see its handoff.

## Build notes

- Clean the shell first: the profile exports a retcomm clang sysroot. Unset `CC`, `CXX`,
  `CMAKE_SYSROOT` and `LDFLAGS`.
- `./build.sh` (GCC, LTO) takes about 3 minutes; `./build.sh --static` builds the release
  executable in `out/static/`.
- Don't switch branches or rebuild while a game runs (`run.sh` rebuilds on launch). Stop the
  game with `pkill -x vwport`.
