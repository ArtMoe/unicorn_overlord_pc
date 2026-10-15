# Odin Sphere Leifthrasir native port — handoff

Read this first, then `PLAN.md` (phases) and `RE_NOTES.md` (reverse-engineering findings).

## What this is

A fork of [gow3_pc](https://github.com/FelipePrado0/gow3_pc) (itself from bloodborne_pc) adapted to
**Odin Sphere Leifthrasir, PS4, CUSA05083 (US, UP0177), app version 01.01**. The PS4 eboot runs
natively on x86-64; `src/runtime_*.c` replaces the PS4 system libraries; `gpu/shadps4` (shadPS4
video core) translates GPU work to Vulkan. Branch: `odin-sphere`. The God of War III parts
(cheats, temporal upscalers, its launcher) were removed on 2026-10-13; see
`docs/dev/CLEANUP_HANDOFF.md`.

Game files are **not** in the repo and must never be committed (copyright; GitHub size limits).
`out_odin/` and `out/` hold dump-derived binaries and are git-ignored.

## State (2026-10-12)

**Phase 1 (first boot) — done.** The game boots through "There is no save data" → save creation
→ ATLUS / CRIWARE / Basiscape logos → title animation → "PRESS ANY BUTTON", rendering correctly
at 1920x1080, with audio output (CRI, 8 ch float via the host backend) and pad input.

Changes on top of gow3_pc:
- `src/runtime_eventflag.c` (new): kernel event flags (create/delete/set/clear/wait/poll, AND/OR,
  clear modes, timeouts, single/multi waiter). The game's render threads depend on them.
- `src/runtime_kernel.c`: `sceKernelConvertUtcToLocaltime/LocaltimeToUtc`,
  `sceKernelGetModuleInfoForUnwind` (ESRCH), `scePthreadCancel` (logged no-op; the game creates
  and cancels 5 helper threads at startup).
- `src/runtime_services.c`: stubs for SystemGesture, NpTus (signed out), ErrorDialog (closes at
  once, logs the code), Coredump, GameLiveStreaming, `sceCommonDialogIsUsed`.
- `src/runtime_savedata.c`: `sceSaveDataGetMountInfo`.
- `src/import_names.inc`: 31 NIDs added (names from shadPS4 `aerolib.inl`).
- `src/runtime_memory.c` + `run.py`: `VW_DISABLED_OPTIMIZATIONS=<mask>` disables GPU
  optimizations for the whole run; run.py sets bits **36 ConstantRing, 37 DrawPipeline** for
  CUSA05083 (bit 35, EarlyDrawInputs, was set too; it was removed with the texture binding
  helper on 2026-10-14). Found by bisecting `VW_TOGGLE_FILE`: 37 makes the window show
  black/partial frames while frame dumps are correct (presentation happens before pipelined draws
  are recorded); 35/36 flicker. Root cause not fixed yet — fixing it and re-enabling is a
  performance task for later.
- Windows only: `VW_HANG_DUMP=<seconds>` prints every guest thread's stack (unwinder for host
  frames, frame pointers for guest frames) plus runtime counters; `src/probe.c`,
  `src/runtime_thread.c`.
- `gpu/CMakeLists.txt` (MinGW only): clang 23 / libc++ needs force-included `cstdlib cmath new
  wchar.h` for vendored code, and `-Wno-unused-but-set-global` on the loader. Linux builds are
  unaffected by these.

**Phase 2 (in game, Linux) — started 2026-10-12.** On Linux (CachyOS, AMD Raven Ridge, KDE
Wayland) the game runs New Game → voice language → attic → "Valkyrie" prologue → cutscenes
(Options → SKIP ALL) → Gwendolyn's first stage (Storming Battlefields) with movement, jump,
attack, HUD, minimap and area wipes correct, all GPU optimizations except 35-37 on (35 has
since been removed).

Fixed: character sprites (Alice, Socrates, the book; every multi-part sprite) drew as scrambled
blocks. Draw preparation (toggle bits 4096/8192) read the fetch shader's V#s from the table the
user data points to, ahead of the GPU thread; Odin Sphere rewrites that table in place between
sprite parts, and only the flattened user data was verified. `BindVertexBuffers` now re-reads the
V#s and drops the prepared vertex inputs when they differ. Found by bisecting `VW_TOGGLE_FILE`.

FSR 3.1 (then the GPU default) rendered below 1080p and dropped translucent UI layers; the
temporal upscalers have since been removed. `VW_READBACK_LINEAR=1` stays in the game profile.

**Render scale (Phase 7, `src/odin_render_scale.h`, `install_odin_render_scale` in probe.c).**
`render_resolution=` in vw.ini (the in-game menu's "Render resolution": native, 1440p, 1800p,
4K; 480p/720p fall back to native; `VW_RENDER_SCALE=<factor 1-3>` overrides) renders at 1920x1080 times the factor; the 2D layout stays in 1080p units. Verified at
1440p and 4K from the title through the attic, the book intro, cutscenes and the first stage
(HUD, minimap) at 60 fps. Patched: render target/offscreen buffer/viewport immediates, the
RENDERBUFFER heap (scale^2), view viewports (FUN_006b5f80), clip scissors (FUN_006b4140), the
on-demand layer buffer (FUN_006b5900), and the texture sizes texel coordinates are divided by
(sprite batches 0x6b87cf, layer composites 0x6b917e/0x6ba130: a scaled target counts as
1920x1080 there). The last one was the 4K book-intro bug: the text layer was composited from the
top-left quarter of the 4K layer, showing a stretched strip of the title logo left in it.
`VW_RENDER_SCALE_PARTS` (bits, default 63) switches the parts off for bisecting.

Downscaling (render resolution above the window) averages each output pixel's footprint in
post_process.frag instead of one bilinear tap; "Sharpening" then applies CAS there (RCAS only runs
at the same size or upscaling).

Open: the selected item of menus (the cutscene skip menu's orange bar) pulses in a 4-frame cycle
(A, brighter, A, dimmer). The game's draws, vertices, constants and textures are identical across
those frames (captures with VW_CAPTURE_FRAMES=4); not the GPU optimizations (all 51 bits off),
linear readbacks or jitter. Next: which display buffer each flip shows (presenter logs the address
on frame dumps).

High fps: logic advances one fixed step per frame (an unlocked 120 Hz run plays at double
speed), so the "Frame Limiter - VBlank Only" patch (patches/, off) is not a usable unlock; it
needs render interpolation or frame generation.

Debug aids: `VW_CAPTURE_FRAMES=<n>` records n display frames per capture;
`VW_CAPTURE_DETAIL=1` adds per-draw primitive, viewport/scissor state, texture sharps with memory hashes and the first vertices of each stream;
`VW_DUMP_SHADERS=1` dumps newly compiled shaders to ~/.local/share/shadPS4/shader/dumps.

Open: on the plain text prompts ("There is no save data", voice language) the guest display
buffer has a grey band and text glow that the presented image lacks.

## Building on Linux

```sh
git clone --recursive -b odin-sphere <your fork URL> odin_port && cd odin_port
nix-shell shell.nix   # or install: gcc/clang, cmake, ninja, pkg-config, vulkan headers+loader,
                      # glslang, SDL3, ffmpeg, fmt, boost, robin-map, xxhash, zydis, spirv-tools/
                      # headers/cross, VulkanMemoryAllocator (shell.nix lists them)
bash build.sh         # Linux path: libvwgpu.so + vwport (GCC, LTO/PGO as upstream)
```

Copy the dump (folder `CUSA05083` with `eboot.bin`, `sce_module/`, `sce_sys/`, `*.CPK`,
`sce_discmap*.plt`) to the Linux machine yourself (USB / scp / rsync). Then:

```sh
VW_GAME_DIR=/path/to/CUSA05083 VW_DATA_DIR=$PWD/run bash run.sh   # or python3 run.py
```

`run.sh` builds, then runs `run.py`, which re-runs the preparation scripts (`scripts/prepare.py`, `link_libc.py`,
`link_modules.py`) into `$VW_DATA_DIR/out` automatically. Verify the eboot first:
sha256 `2865b6cec33f2515def173c1ed0c03afc27a1cd5fad1b0d0f516b4527aba9a88`.

Arch/CachyOS: `pacman -S boost robin-map zydis spirv-headers spirv-cross` (+ the usual vulkan,
sdl3, ffmpeg, fmt, xxhash, glslang); VulkanMemoryAllocator is not packaged, so clone it and pass
`CMAKE_INCLUDE_PATH=<VMA>/include` to `build.sh`. xbyak comes from `gpu/third_party/xbyak` on all
platforms. A shell that exports another toolchain (CC/CXX/CMAKE_SYSROOT/LDFLAGS) breaks the
build; unset those first. Real-screen capture at 60 fps (frame dumps do not show presentation
problems): `gpu-screen-recorder -w eDP-1 -f 60 -o rec.mkv`.

Things to check on Linux that were only verified on Windows:
- `runtime_eventflag.c` waits use `CLOCK_MONOTONIC` there (Windows uses `CLOCK_REALTIME`).
- `scripts/game_profiles.py` sets `VW_DISABLED_OPTIMIZATIONS` for CUSA05083.
- The Linux watchdog (`--timeout`, thread dumps on SIGALRM) replaces `VW_HANG_DUMP`.
- `tests/`: `python -m unittest discover -s tests` (4 tests failed on Windows only: symlink
  privilege, CRLF run.sh, nix paths — expected to pass on Linux).

## Testing without touching the game

- `VW_PAD_FILE=<file>`: whitespace-separated held buttons (`cross`, `circle`, `options`,
  `up`, ...); empty file releases. Re-read when it changes.
- `VW_FRAME_DUMP_TRIGGER=<file>` + `VW_DUMP_DIR=<dir>`: creating the file dumps the next
  frame (`display_*` guest buffer, `output_*` presented image; raw RGBA, size in the name).
- `VW_TOGGLE_FILE=<file>`: decimal mask of optimizations to disable live (bits in
  `gpu/shim/vw_toggles.h`, `src/runtime_memory.c`).
- `tools/odin_re/drive.ps1`: Windows scripted session (timed pad input + frame dumps); port it to
  a shell script for Linux.

## Reverse engineering

`tools/odin_re/`: `make_ghidra_elf.py` turns the prepared eboot into a Ghidra-friendly ELF (ET_EXEC,
PT_SCE_RELRO → PT_LOAD, all 239,947 relocations applied, imports → fake EXTERN block) plus
`imports.tsv`; `ghidra_scripts/LabelImports.java` names imports (202/215 by name; extra names in
`extra_names.tsv`); the other scripts are generic helpers (Decomp, Xrefs, DumpCallers, ...).
Usage in `tools/odin_re/README.md` (paths there are the Windows ones; Ghidra 12.1.2 + JDK 21).

Key finding: frame limiter `FUN_006b3eb0` — waits `sceVideoOutWaitVblank`, then spins until
1e6 / *(float *)(ctx + 0x54) µs have passed (target fps at ctx+0x54). See `RE_NOTES.md`.

## Per-game settings, saves, GPU workarounds (review 2026-10-12)

- `scripts/game_profiles.py`: one place for a game's launch settings, read by run.py
  (Odin Sphere: optimizations 36-37 off, linear readbacks, no readback shortcuts).
- Saves (`/savedataN`) opened for writing go through a temporary copy (`<name>.<n>.vwsave`),
  synced and renamed over the file on close: a crash mid-save keeps the old save (from bbport,
  its #64; Windows: `_commit` + `MoveFileEx`). Leftover copies are removed when saves mount.
  `tests/test_save_commit.c` (build.sh --test).
- `RADV_DEBUG=nohiz` is set for GCN 3-5 GPUs under RADV (Polaris, Vega and their APUs: HiZ showed
  black halos/missing geometry in bbport, its #54/#99), from `vwport --gpu-device`;
  `VW_AUTO_NOHIZ=0` or a user RADV_DEBUG keeps it off. This laptop runs the game on its
  discrete RX 560X (Polaris 11); the Vega 8 iGPU is the second device.
- Render scale: all hooks are checked for range before any is written (a partly scaled frame
  draws in two coordinate systems); factors below 1 are refused for this game (a scaled target
  size such as 1280x720 is also a texture asset size the texture size hooks would misreport).
- Capture detail (`VW_CAPTURE_DETAIL=1`) lives in vk_frame_capture.cpp, not the draw path.

## Launcher and packages

`launcher/vw_launcher.py`: Tkinter launcher (Windows and Linux), themed per game (`GAMES`);
`launcher/odin_launcher.py` starts it for Odin Sphere. The dump's own artwork (sce_sys/pic1.png,
icon0.png), the game folder check (CUSA05083 01.01), render resolution, display mode, VSync,
FPS counter, FSR 1 and sharpening (vw.ini), Play, log, saves folder.
`python3 packaging/package.py odin` builds the static executable (`build.sh --static`) and
writes `dist/Odin Sphere PC/` with four files: the launcher (PyInstaller, with run.py, scripts/
and patches/ inside), `vwport`, README.txt and LICENSE, plus a .tar.gz. Windows:
`bash packaging/windows/build.sh odin` in MSYS2 CLANG64 (not tried since the cleanup).

## Next steps (Phase 2–4, see PLAN.md)

1. Press past the title: New Game → first chapter; watch for missing files, CRI USM movie
   playback, save writing (`run/user`), errors in the log.
2. Fix crashes/rendering issues as they show up (frame dumps + `VW_TOGGLE_FILE` bisecting).
3. Enhancements (Phase 7): render scale done (above, also in the launcher); high fps needs
   interpolation of the triple-buffered render state or frame generation.
