# Handoff (2026-10-19)

Read this first. The port's history and reverse-engineering notes are in
[unicorn/HANDOFF.md](unicorn/HANDOFF.md), the GPU pipeline in [parallel_gpu.md](parallel_gpu.md).

## Open work for the next agent (2026-10-19, evening)

Finish the first public release. In order:

1. **Pull request #18** (branch `claude/settings-checks`): removes the frame rate limit and adds
   updates from GitHub releases (below). Wait for the Linux and Windows checks, then merge it
   into `main` with a merge commit (`gh pr merge 18 --merge`); GitHub deletes the branch. The
   CI then rebuilds the rolling `latest` pre-release. A check run once got stuck (run "completed"
   while its jobs stayed queued): `gh run rerun <id>` fixed it.
2. **Test the Windows package from `latest`** (`Unicorn-Overlord-PC-windows.zip`): first-start
   setup, title, the Options PC section, a battle, save and load. Nobody has run the Windows
   **update swap** (`--finish-update`, `scripts/updates.install`) on Windows yet: after step 3,
   start an older build (the `latest` zip counts as older than every release) and take the
   offered update; check that `data/` (vw.ini, saves, game) survives and the new version starts.
3. **Publish v0.1.0:** `git tag -a v0.1.0 <tested main commit>` (the message is the release notes
   players see in the update dialog), `git push origin v0.1.0`. The tag workflow builds and
   publishes the release with `SHA256SUMS` ([../RELEASING.md](../RELEASING.md)). Never move or
   delete a published tag. Decide whether to mark it a pre-release in the GitHub UI (the updater
   offers pre-releases too; GitHub's "Latest" badge only shows normal releases).
4. The owner asked whether the Insert menu is still needed. Only it has: button remapping
   (keyboard and controller), the GPU readback switches (the fix for wrong graphics), frames
   queued, background shader compilation, overlay layout, Change game folder, Apply and
   restart, and image information. Shrinking it to remapping + troubleshooting is an open option.

### What landed today

| Change | Where | Checked |
|---|---|---|
| PC section in System → Options (#17, in `main`): Display Mode, VSync, Render Resolution, FSR 1, Sharpening, Strength, Button Icons, Performance Overlay, Check for Updates | `src/unicorn_native_settings.h`, `gpu/shim/vw_settings.cpp` | title and in-game screens, every row kind, Restore Defaults, `vw.ini` |
| Frame rate limit removed (#18): the game paces itself at 60 Hz | `driver.cpp`, `vw_settings.*`, `vw_overlay.cpp` | no limiter 60.0 FPS / 17 ms worst; a 60 limiter 59.6–59.8 / 20–25 ms; 120/240 = none |
| Render resolution and FSR 1 checked | | 720p after restart: `Render scale: 1280x720`; FSR 1 at 720p in a 1432×806 window: sharper (edge energy 25.3 → 31.7) |
| Updates (#18): offer, download, SHA-256 check, Windows swap with rollback, AppImage replace | `scripts/updates.py`, `launcher/vw_launcher.py`, `tests/test_updates.py` | 10 unit tests, live GitHub API read; not on Windows |
| Tag releases (#18): `vX.Y.Z` tag → release with `SHA256SUMS`; versions from `v*` tags only | `.github/workflows/build.yml` | not run yet (runs on the first tag) |

How the updater decides: `scripts/updates.py` docstring. The running version is
`_internal/runtime/VERSION` (`git describe --match 'v[0-9]*'`); `vX.Y.Z-N-g<hash>` is newer than
vX.Y.Z, a version without a `v` tag (today's builds) is older than every release. `check_updates=0`
in `vw.ini` or `VW_UPDATES=0` turns the check off; "Skip this version" is kept in
`data/launcher.json` (`skipped_update`).

## State

Unicorn Overlord (CUSA36381 v01.04) boots, plays the prologue and chapter 1's first battle,
saves and loads. Test laptop: Ryzen 5 2500U (4c/8t), RX 560X, CachyOS; a Windows desktop
(7800X3D, RX 9070 XT) runs it too. Linux and Windows builds come from CI
(`.github/workflows/build.yml`); `main` is protected by the "Protect main" ruleset (pull
requests only, Linux and Windows checks required, no force-push or deletion).

Done on 2026-10-19 (pull request #16):

| Change | Where | Checked |
|---|---|---|
| Image preparation cached between starts | `scripts/prepare_cache.py`, `run.py` | launch → game code 3.2 s → 0.75 s (source), 0.86 s (package) |
| One-folder package (no self-unpacking) | `packaging/package.py`, `packaging/linux/appimage.sh` | ~470 → ~90 ms per program start; CI builds both |
| Setup installs the dump into `data/game` | `launcher/vw_launcher.py` | real dump hard-linked in 0.26 s; copy/cancel/resume in tests |
| 720p and 900p render sizes | `src/probe.c` (`unicorn_render_size`) | 720p: logos, opening, battle map framed as at 1080p |
| Xbox and Nintendo button icons | `src/unicorn_button_icons.h`, `src/probe.c`, `src/runtime_pad.c` | Xbox LS prompt in the tutorial; set at start |
| PR #13 merged (notices, diagnostics, native CI tests) | `packaging/notices.py`, `scripts/diagnostics.py` | 99 Python tests, `build.sh --test`, 15 GPU tests |
| Odin Sphere removed | everything | builds and tests pass |

Done later on 2026-10-19 (branch `claude/native-settings`, on top of #16):

| Change | Where | Checked |
|---|---|---|
| PC section in the game's System → Options (10 rows) | `src/unicorn_native_settings.h`, `gpu/shim/vw_settings.cpp` | title and in-game screens; choice, switch, slider, close/reopen, Restore Defaults; `vw.ini` |
| Ghidra image: base-relative pointers were all written as 0 (vtables empty) | `tools/re/make_ghidra_elf.py` | slider vtable `+0xe8` now `0x73add0` |

Details: [unicorn/NATIVE_SETTINGS.md](unicorn/NATIVE_SETTINGS.md). A Ghidra project made
before this fix has no vtables or function tables: re-import a new `eboot_ghidra.elf` (about
15 minutes, [../../tools/re/README.md](../../tools/re/README.md)).

## Findings worth knowing

- **Benchmarks: run with `VW_VSYNC=0`.** With VSync on, KWin throttles presentation of a
  window it considers hidden (another desktop, covered): the game then shows 20 FPS and every
  per-draw cost looks 9× higher (stage B waits on present, stage A spins on stage B). The game
  paces itself at 60 Hz either way.
- **Benchmark scene:** New Game → the opening (cavalry charge in the rain): ~4,100-4,400
  draws/frame, 51-52 FPS on the laptop, stage A ~3.9 µs/draw (~85% busy), stage B ~77% busy.
  Light scene: Continue → Palevia battle map, ~1,100 draws/frame, 60 FPS.
- **Boot:** the game decompresses ~1.5 GiB (≈300 files) of CRILAYLA data while its warning and
  logo screens show, on one thread (`CRI FS Data Decompress`). A native decoder was 1.28× faster
  in a benchmark but equal in the game (memory-bound); it was removed. What would help is
  decompressing the known boot files ahead of time on several threads.
- **Opening "banding":** every pass of the opening draws into RGBA8 targets, as on a PS4; frames
  show no lost precision. A deband filter in the presenter would be the only fix.
- **Button icons:** the sprite file has four variants per button (Switch, PS4 dark, PS4 light,
  Xbox; rufaswan's Web2D_Games documents the format). Changing the setting while playing:
  icons already on screen keep theirs; whether new prompts follow was not checked.
- **bbport (bloodborne_pc 0.6) replaced shadPS4** with its own command processor, shader
  translator and renderer (2026-10-13/15). It is still a PM4 → Vulkan translator with the same
  two-stage pipeline; its measured CPU gains on 8-thread CPUs come from fewer draw-prep workers,
  and its constant "resolve workers" gave no FPS there. Its button prompt swap (textures at bind
  time) is how live icon switching could be done here.

## Tried and dropped (2026-10-19)

- Command-stream prefetch in `ProcessGraphics` (the header load is its hottest instruction):
  3.92 → 3.89 µs/draw, same FPS. Reverted.
- One draw-prep worker instead of four on 8 threads (bbport's finding): within noise here.

## Next steps

1. **Play further** (world map, chapter 2+): nothing past chapter 1's first battle is tested.
   A 1.0 needs a full playthrough on Linux and Windows, ideally by testers on other hardware.
2. **CPU: both GPU stages are near full in busy scenes.** Stage A (decode 35%, packet building
   ~26%: constant/vertex ring copies) and stage B (flat profile: bindings, vertex inputs,
   begin rendering). Plan, in order:
   - move the ring copies to resolve workers (bbport `BB_RESOLVE_WORKERS`: stage A 4.4 → 3.3 µs
     per packet there); the copies must finish before the packet's end-of-pipe signal and before
     a later constant-engine dump that writes their bytes (`BeforeConstRamDump`);
   - then stage B's pure lookups (buffers, vertex/index, textures) to stage A's workers, with
     generation checks on stage B (bbport's unimplemented "stage 2").
   Measure in the opening with `VW_FRAME_STATS=1 VW_VSYNC=0` (us/draw, idle, EOP latency).
3. **Native settings, what is left:** run it on Windows (CI builds it; the hooks use the
   game's System V calling convention, as the button icon hook does); a restart prompt for
   Render Resolution in the game's screen (now it applies at the next start); a Japanese system
   language (the PC texts are English only).
4. **Live button icon switching:** swap the icon texture at bind time (bbport
   `vk_button_prompts.cpp`) instead of relying on the sprite table being re-read.
5. Optional: deband filter; parallel boot decompression; ultrawide (see unicorn/HANDOFF.md).

## Working on it

- Linux build: `bash build.sh` (unset a cross toolchain's CC/CXX/CMAKE_SYSROOT/LDFLAGS first).
  Tests: `python -m unittest discover -s tests`, `bash build.sh --test`, the GPU tests
  (`packaging/test-native.sh` lists them).
- Run: `VW_GAME_DIR=<dump> VW_DATA_DIR=run_uo python3 run.py`; scripted input
  `VW_PAD_FILE=<file>` (button names), frame dumps `VW_FRAME_DUMP_TRIGGER=<file>` +
  `VW_DUMP_DIR`, draw lists `VW_CAPTURE_TRIGGER`, decompression stats `VW_CRILAYLA_STATS=1`.
- Title menu from boot: ~40 s, Cross (skip the opening), Cross (Press Any Button), then
  Continue / New Game (Down) / Load Game.
- Ghidra: [../../tools/re/README.md](../../tools/re/README.md).
- Commits on the public repository use the GitHub noreply address
  (`88461786+hachi23@users.noreply.github.com`).
