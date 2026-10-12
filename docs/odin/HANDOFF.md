# Odin Sphere Leifthrasir native port — handoff

Read this first, then `PLAN.md` (phases) and `RE_NOTES.md` (reverse-engineering findings).

## What this is

A fork of [gow3_pc](https://github.com/FelipePrado0/gow3_pc) (itself from bloodborne_pc) adapted to
**Odin Sphere Leifthrasir, PS4, CUSA05083 (US, UP0177), app version 01.01**. The PS4 eboot runs
natively on x86-64; `src/runtime_*.c` replaces the PS4 system libraries; `gpu/shadps4` (shadPS4
video core) translates GPU work to Vulkan. Branch: `odin-sphere`. gow3-specific hooks/patches are
gated on CUSA01623 01.02 and stay inert.

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
- `src/runtime_memory.c` + `run.py`: `GOW3_DISABLED_OPTIMIZATIONS=<mask>` disables GPU
  optimizations for the whole run; run.py sets bits **35 EarlyDrawInputs, 36 ConstantRing,
  37 DrawPipeline** for CUSA05083. Found by bisecting `GOW3_TOGGLE_FILE`: 37 makes the window show
  black/partial frames while frame dumps are correct (presentation happens before pipelined draws
  are recorded); 35/36 flicker. Root cause not fixed yet — fixing it and re-enabling is a
  performance task for later.
- Windows only: `GOW3_HANG_DUMP=<seconds>` prints every guest thread's stack (unwinder for host
  frames, frame pointers for guest frames) plus runtime counters; `src/probe.c`,
  `src/runtime_thread.c`.
- `gpu/CMakeLists.txt` (MinGW only): clang 23 / libc++ needs force-included `cstdlib cmath new
  wchar.h` for vendored code, and `-Wno-unused-but-set-global` on the loader. Linux builds are
  unaffected by these.

**Phase 2 (in game, Linux) — started 2026-10-12.** On Linux (CachyOS, AMD Raven Ridge, KDE
Wayland) the game runs New Game → voice language → attic → "Valkyrie" prologue → cutscenes
(Options → SKIP ALL) → Gwendolyn's first stage (Storming Battlefields) with movement, jump,
attack, HUD, minimap and area wipes correct, all GPU optimizations except 35-37 on.

Fixed: character sprites (Alice, Socrates, the book; every multi-part sprite) drew as scrambled
blocks. Draw preparation (toggle bits 4096/8192) read the fetch shader's V#s from the table the
user data points to, ahead of the GPU thread; Odin Sphere rewrites that table in place between
sprite parts, and only the flattened user data was verified. `BindVertexBuffers` now re-reads the
V#s and drops the prepared vertex inputs when they differ. Found by bisecting `GOW3_TOGGLE_FILE`.

FSR 3.1 (the GPU default) rendered below 1080p and dropped translucent UI layers: run.sh now sets
`GOW3_UPSCALER=none GOW3_READBACK_LINEAR=1` for CUSA05083 (as run.py).

**Render scale (Phase 7, `src/odin_render_scale.h`, `install_odin_render_scale` in probe.c).**
`render_resolution=` in gow3.ini (the in-game menu's "Render resolution": native, 480p, 720p,
1440p, 1800p, 4K; `GOW3_RENDER_SCALE=<factor 0.4-3>` overrides) renders at 1920x1080 times the factor; the 2D layout stays in 1080p units. Verified at
1440p and 4K from the title through the attic, the book intro, cutscenes and the first stage
(HUD, minimap) at 60 fps. Patched: render target/offscreen buffer/viewport immediates, the
RENDERBUFFER heap (scale^2), view viewports (FUN_006b5f80), clip scissors (FUN_006b4140), the
on-demand layer buffer (FUN_006b5900), and the texture sizes texel coordinates are divided by
(sprite batches 0x6b87cf, layer composites 0x6b917e/0x6ba130: a scaled target counts as
1920x1080 there). The last one was the 4K book-intro bug: the text layer was composited from the
top-left quarter of the 4K layer, showing a stretched strip of the title logo left in it.
`GOW3_RENDER_SCALE_PARTS` (bits, default 63) switches the parts off for bisecting.

Downscaling (render resolution above the window) averages each output pixel's footprint in
post_process.frag instead of one bilinear tap; "Sharpening" then applies CAS there (RCAS only runs
at the same size or upscaling).

Open: the selected item of menus (the cutscene skip menu's orange bar) pulses in a 4-frame cycle
(A, brighter, A, dimmer). The game's draws, vertices, constants and textures are identical across
those frames (captures with GOW3_CAPTURE_FRAMES=4); not the GPU optimizations (all 51 bits off),
linear readbacks or jitter. Next: which display buffer each flip shows (presenter logs the address
on frame dumps).

High fps: logic advances one fixed step per frame (an unlocked 120 Hz run plays at double
speed), so the "Frame Limiter - VBlank Only" patch (patches/, off) is not a usable unlock; it
needs render interpolation or frame generation.

Debug aids: `GOW3_CAPTURE_FRAMES=<n>` records n display frames per capture;
`GOW3_CAPTURE_VIEWPORTS=1` adds per-draw viewport/scissor state and texture sharps;
`GOW3_DUMP_SHADERS=1` dumps newly compiled shaders to ~/.local/share/shadPS4/shader/dumps.

Open: on the plain text prompts ("There is no save data", voice language) the guest display
buffer has a grey band and text glow that the presented image lacks.

## Building on Linux

```sh
git clone --recursive -b odin-sphere <your fork URL> odin_port && cd odin_port
nix-shell shell.nix   # or install: gcc/clang, cmake, ninja, pkg-config, vulkan headers+loader,
                      # glslang, SDL3, ffmpeg, fmt, boost, robin-map, xxhash, zydis, spirv-tools/
                      # headers/cross, VulkanMemoryAllocator (shell.nix lists them)
bash build.sh         # Linux path: libgow3gpu.so + gow3-probe (GCC, LTO/PGO as upstream)
```

Copy the dump (folder `CUSA05083` with `eboot.bin`, `sce_module/`, `sce_sys/`, `*.CPK`,
`sce_discmap*.plt`) to the Linux machine yourself (USB / scp / rsync). Then:

```sh
GOW3_GAME_DIR=/path/to/CUSA05083 GOW3_DATA_DIR=$PWD/run bash run.sh   # or python3 run.py
```

`run.py`/`run.sh` re-run the preparation scripts (`scripts/prepare.py`, `link_libc.py`,
`link_modules.py`) into `$GOW3_DATA_DIR/out` automatically. Verify the eboot first:
sha256 `2865b6cec33f2515def173c1ed0c03afc27a1cd5fad1b0d0f516b4527aba9a88`.

Arch/CachyOS: `pacman -S boost robin-map zydis spirv-headers spirv-cross` (+ the usual vulkan,
sdl3, ffmpeg, fmt, xxhash, glslang); VulkanMemoryAllocator is not packaged, so clone it and pass
`CMAKE_INCLUDE_PATH=<VMA>/include` to `build.sh`. xbyak comes from `gpu/third_party/xbyak` on all
platforms. A shell that exports another toolchain (CC/CXX/CMAKE_SYSROOT/LDFLAGS) breaks the
build; unset those first. Real-screen capture at 60 fps (frame dumps do not show presentation
problems): `gpu-screen-recorder -w eDP-1 -f 60 -o rec.mkv`.

Things to check on Linux that were only verified on Windows:
- `runtime_eventflag.c` waits use `CLOCK_MONOTONIC` there (Windows uses `CLOCK_REALTIME`).
- `run.py` and `run.sh` both set `GOW3_DISABLED_OPTIMIZATIONS` for CUSA05083.
- The Linux watchdog (`--timeout`, thread dumps on SIGALRM) replaces `GOW3_HANG_DUMP`.
- `tests/`: `python -m unittest discover -s tests` (4 tests failed on Windows only: symlink
  privilege, CRLF run.sh, nix paths — expected to pass on Linux).

## Testing without touching the game

- `GOW3_PAD_FILE=<file>`: whitespace-separated held buttons (`cross`, `circle`, `options`,
  `up`, ...); empty file releases. Re-read when it changes.
- `GOW3_FRAME_DUMP_TRIGGER=<file>` + `GOW3_DUMP_DIR=<dir>`: creating the file dumps the next
  frame (`display_*` guest buffer, `output_*` presented image; raw RGBA, size in the name).
- `GOW3_TOGGLE_FILE=<file>`: decimal mask of optimizations to disable live (bits in
  `gpu/shim/gow3_toggles.h`, `src/runtime_memory.c`).
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

## Launcher and packages

`launcher/odin_launcher.py`: Tkinter launcher (Windows and Linux) with the dump's own artwork
(sce_sys/pic1.png, icon0.png), the game folder check (CUSA05083 01.01), render resolution,
display mode, VSync, FPS counter and sharpening (gow3.ini), Play, log, saves folder.
`python3 packaging/odin_package.py` (after build.sh) freezes it with PyInstaller into one
executable that also runs run.py and the scripts, and writes `dist/Odin Sphere PC/`:
the launcher, README.txt and runtime/ (16 files; data/ is created on first start), plus a
.tar.gz (Linux) or .zip (Windows, from MSYS2 CLANG64 with a Windows Python; not tried yet).
run.py now runs on Linux too; the in-game restart uses GOW3_RESTART_COMMAND on Linux as well.

## Next steps (Phase 2–4, see PLAN.md)

1. Press past the title: New Game → first chapter; watch for missing files, CRI USM movie
   playback, save writing (`run/user`), errors in the log.
2. Fix crashes/rendering issues as they show up (frame dumps + `GOW3_TOGGLE_FILE` bisecting).
3. Rebrand the launcher later (Phase 6).
4. Enhancements (Phase 7): render scale done (above); high fps needs interpolation of the
   triple-buffered render state or frame generation; render_scale in the launcher UI; cheats.
