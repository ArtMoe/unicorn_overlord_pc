# Handoff (2026-10-20)

Read this first. The port's history and reverse-engineering notes are in
[unicorn/HANDOFF.md](unicorn/HANDOFF.md), the GPU pipeline in [parallel_gpu.md](parallel_gpu.md).
Development moves to the Windows desktop (7800X3D, RX 9070 XT) from here; build there with
MSYS2 CLANG64 ([../../packaging/windows/README.md](../../packaging/windows/README.md)).

## Open work for the next agent (2026-10-20)

In order:

1. **Pull request #23** (branch `claude/nvidia-pipeline-binary`, fixes issue #21): the game
   crashed at start on an NVIDIA RTX 4050 laptop (Windows 11, driver 610.88). The log's fault
   `vwport.exe+0x6fba49` resolves with the v1.0.0 Windows symbols to `Instance::CreateDevice`,
   the instruction after `vkGetPipelineKeyKHR(device, NULL, &key)`: the callee-saved `rdi`
   came back as 1 and the stack held text, so the stack was overwritten inside the driver.
   The PR leaves `VK_KHR_pipeline_binary` off on `eNvidiaProprietary`, and
   `VW_PIPELINE_BINARIES=0` now turns it off at device creation too. Checked on AMD/Linux with
   the extension off (battle map loads); not on NVIDIA. Merge after its Linux and Windows
   checks, and ask the reporter on #21 for a new log.
2. **Audio is too quiet (owner's report: low even with every volume at 100%).** Not fixed.
   Findings so far:
   - The game opens one audio port: 8 channels, float, 256 frames (`src/runtime_audio.c`).
     A 66 s dump of the boot and opening (`VW_AUDIO_DUMP=<prefix>`) has nearly everything in
     front L/R (RMS about -31 dBFS, peaks about -11 dBFS); center and LFE are silent, the
     surrounds about 27 dB lower.
   - The runtime hands those 8 channels to SDL unchanged. When SDL itself downmixes 7.1 to
     stereo (`SDL_Convert71ToStereo`, SDL 3.4.18) it keeps 0.212 of FL/FR: -13.5 dB.
   - On Linux (PipeWire, stereo sink) the recorded output of the sink monitor peaks at
     -12 dBFS, as loud as the game's own mix: there SDL seems to open the device with 8
     channels and PipeWire downmixes at unity. Not confirmed (a debug print of
     `SDL_GetAudioDeviceFormat` was started, then stopped when work moved to Windows).
   - Hypothesis to check first on Windows: WASAPI's stereo endpoint makes SDL do the
     downmix, so Windows plays about 13.5 dB too quiet. Print the device format at port open
     (`SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream), ...)`), and record the
     output level with and without a fix.
   - Fix if confirmed: when the device has fewer than 6 channels, downmix in
     `output_port()` before `SDL_PutAudioStreamData` (L = FL + 0.707 C + 0.707 SL + 0.707 BL,
     R likewise, LFE dropped, clamp to [-1, 1]) and open the stream with 2 channels. The
     queue-level control and the silence padding in `output_port()` use `bytes`: switch them
     to the size actually sent. Check for clipping in loud scenes (battles).
3. **Review for NVIDIA.** The owner asked for a pass over NVIDIA-specific risks, comparing
   with upstream shadPS4 and other emulators' NVIDIA workarounds: extensions the port uses
   that NVIDIA lacks (the #21 log lists `VK_EXT_shader_stencil_export`,
   `VK_AMD_shader_image_load_store_lod`, `VK_EXT_shader_atomic_float2` and AMD extensions as
   unavailable), driver-ID branches in `vk_instance.cpp`, and the pipeline cache path.
4. **Play test, then v1.2.0.** Play on Windows: first-start setup, title, New Game opening,
   Continue into the Palevia battle, System > Options PC rows, Insert menu with Apply and
   restart (1080p to 900p and back), save and load, Change game folder. Then merge, tag
   `v1.2.0` on the merge commit (`git tag -a`, the message is the release notes) and push the
   tag ([../RELEASING.md](../RELEASING.md)).
5. **Issue #14:** asked the reporter (2026-10-20) for a run log with Xbox icons set; the
   icons work with our dump. Their log will show `Button icons:` or `Render scale:` lines.

### What landed on 2026-10-20 (v1.1.0, pull request #22)

| Change | Where | Checked |
|---|---|---|
| AppImage crashed on Wayland (Steam Deck game mode, Plasma): children inherited PyInstaller's `LD_LIBRARY_PATH` and its old Vulkan loader (`vkCreateWaylandSurfaceKHR` null) | `launcher/vw_launcher.py` (`restore_library_path`) | v1.0.0 crashes under gamescope 1280x800; v1.1.0 release AppImage runs there |
| Apply and restart closed the game (exit 23): descriptors closed before `execl` while the GPU thread ran | `src/probe.c` (`runtime_restart_with`, close-on-exec) | restarts in about 1 s |
| 900p then 1080p rendered at 4K: the restart inherited `VW_NEO_MODE`/`VW_INTERNAL_*` | `src/probe.c` (`forget_render_scale_env`) | 1080p to 900p shows 1600x900, back shows 1920x1080 |
| Signature mismatch rendered 900p at 4K | `src/probe.c` (check before the Pro path) | forced mismatch: 1920x1080 buffers |
| Change game folder waited 30 s on Linux (own pid) | `launcher/vw_launcher.py` (`wait_for`) | 30.1 s to 0.0 s |
| `vw.ini`/`launcher.json` emptied by a crash mid-write | `gpu/shim/vw_settings.cpp`, `launcher/vw_launcher.py` | in-game change saved, no `.tmp` left |
| Restarts pushed good save backups out | `scripts/save_backup.py` | failing test first, 113 Python tests |

v1.1.0 is published (tag on `69feb5b`); the Windows zip of it was only checked by CI.

**Driving the game for tests (Linux):** `VW_PAD_FILE=<file>` takes pad tokens
(`cross`, `lx=255`, ...); the Insert overlay only reads real input, so use `ydotool` (start
`ydotoold`; Insert is key 110, the overlay navigates with the arrow keys and Space). On
Windows, an AutoHotkey or PowerShell `SendInput` script does the same. In battle, Cross on the
Command Post does nothing during the tutorial; menus accept Cross.

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
  Xbox; rufaswan's Web2D_Games documents the format). Changing the setting while playing
  switches the style at once (`Runtime: button icons: Nintendo` in the log); whether icons
  already on screen change was not checked.
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

- Windows build: MSYS2 CLANG64 shell, `bash build.sh` (development) or
  `bash packaging/windows/build.sh unicorn` (the release folder); details and the Linux/Windows
  differences in [../../packaging/windows/README.md](../../packaging/windows/README.md).
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
