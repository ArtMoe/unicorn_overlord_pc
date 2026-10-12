# Unicorn Overlord (CUSA36381, Asia, v01.04) — handoff

Branch `unicorn-overlord` of `hachi23/odin_sphere_pc` (it contains all of `odin-sphere`, merged
2026-10-13). Same method as Odin Sphere and God of War III: the game's x86-64 code runs natively,
`src/runtime_*.c` replaces the PS4 libraries, `gpu/shadps4` translates the GPU to Vulkan.
Read `docs/odin/HANDOFF.md` first for building on Linux, the run scripts and the tooling.

## State (2026-10-13)

- Boots, plays the opening, reaches the title (New Game / Load Game / Library / System, English
  through the EN patch), starts a new game, plays the prologue story scenes and the first battle
  (defence of the capital): tutorial, unit movement, unit panels, text log, minimap all work.
- Not reached yet: the end of the first battle, the world map, saving, full-motion video.
- Performance on the test laptop (Ryzen 5 2500U, the game on the discrete RX 560X): battle map
  ~19 FPS at first, **~27 FPS** after the draw-pipe batching (below). CPU-bound, the GPU is
  15-20% busy. Goal: 60.

## The dump

Asia version, base v1.01 PKG + the 1.04 update with an English patch:
- `~/Downloads/HP0177-CUSA36381_00-UNICORNOVERLORD0.v1.01-[DLPSGAME.COM].pkg` (8 GB)
- `~/Downloads/HP0177-CUSA36381_00-UNICORNOVERLORD0-A0104-V0100-EN-patch-by-joe97tab.pkg`
  (1 GB; a fake PKG, DRM type 15; param.sfo title "Unicorn Overlord EN Patch", APP_VER 01.04)

Extracted with the ShadPs4Plus PKG Extractor 1.0
(github.com/AzaharPlus/shadPS4Plus/releases/tag/PKG_EXTRACTOR_1_0, in
`~/.local/src/ps4tools/ShadPs4Plus-PkgExtractor-1.0-linux/pkg_extractor.AppImage`):
`pkg_extractor.AppImage <file.pkg> <out dir>` → `~/Downloads/uo_base/CUSA36381`,
`~/Downloads/uo_update/CUSA36381-patch`.

Merged into **`~/Downloads/game_dump/CUSA36381`** the way the console applies an update (update
files replace base files): base files hard-linked (`cp -al`, no second copy of the 6.3 GB
`Unicorn.CPK`), update files copied over. Names are matched case-insensitively, as on the PS4:
the update ships `Unicorn_CN.cpk`/`Unicorn_TW.cpk`, the executable opens `Unicorn_CN.CPK`. Keep
`uo_base` while the merged folder links to it.

Open question: the update's `UnicornPatch.CPK` is 1.0 GB, the base's 3.3 GB. Nothing missing has
shown so far (everything through the first battle renders and the text is English).

## Running

```
VW_GAME_DIR=$HOME/Downloads/game_dump/CUSA36381 VW_DATA_DIR=$HOME/odin_sphere_pc/run_uo bash run.sh
```
(with the shell environment cleaned as in docs/odin/HANDOFF.md). Saves, caches and vw.ini are
in `run_uo/`. The launcher and package (`launcher/odin_launcher.py`, `packaging/odin_package.py`)
are Odin Sphere's; they check the title ID and need a Unicorn Overlord variant.

## What the port needed (commit 1f99c4c)

Imports: 720 (Odin Sphere 215), 44 required modules. `out/vwport … --check-imports` lists
every import the game would stop at without running it; names come from shadPS4's
`aerolib.inl` (~/.local/src/ps4tools/aerolib.inl).

- **Bundled PSN libraries run natively.** `libSceNpToolkit2.prx`, `libSceNpCppWebApi.prx` and
  `libSceJobManager.prx` from the game's `sce_module` are linked like libc and Fios2
  (`TITLE_MODULES` in `scripts/link_modules.py`): their ~200 C++ imports disappear and only the
  PSN C APIs reach the runtime (290 → 206 missing at that step).
- **PSN C APIs offline** (`src/runtime_services.c`): context/request/handle creation and callback
  registration return ids, terminate/delete/abort succeed, everything that needs an account
  returns NP_SIGNED_OUT; sign-in dialog finishes at once; share utility, HTTP options,
  NpWebApi2, Matching2, TUS/TSS, Score, WordFilter, InGameMessage, InvitationDialog.
- **Newer SDK kernel/libc**: `_sceKernelSetThreadDtors` (called as each guest thread finishes),
  thread-atexit hooks, POSIX rwlocks, `scePthreadGetname`/`AttrGetstack` (real host stack)/
  `Getthreadid`, `sceKernelCancelEventFlag`, `AvailableDirectMemorySize`, `Getdents`, `Chmod`,
  `Utimes`, `IsNeoMode` (0), sanitizer queries, `sceLibcHeapGetTraceInfo` (as shadPS4),
  Win32 RTC file times, `sceSysmoduleLoadModuleInternalWithArg`/`GetModuleInfoForUnwind`.
- **`sce::Json` (libSceJson2, a firmware library)**: only the set-up (MemAllocator,
  Initializer) is stubbed. Parsing/building values is not implemented: the first use stops at
  that import with its name.
- **NID names**: `src/import_names.inc` maps NIDs to names for the runtime's lookup; ~340 were
  added. POSIX imports are looked up as `#libkernel` (runtime.c canonical_name): name them so.

Still missing (stop when first called): `sceVideodec2*` (movies: an FFmpeg backend like
`runtime_videodec.c` is needed), the rest of `sce::Json`, `sceSaveDataTransferringMount`,
`sceImeDialogGetPanelSizeExtended`, `sceAjmStrError`, one DiscMap and two `ulobjmgr` NIDs.

## Performance work

Measured in the first battle (scripts below drive there):
- ~6000 draws per frame (`perf probe` on `Vulkan::Rasterizer::DrawRecord`: 584,069 in 5 s at
  ~20 FPS): every sprite part, unit, building and effect is its own draw.
- Two threads saturate: `shadPS4:GpuComm` (PM4 decode, stage A) and `vw:DrawRec` (draw
  recording, stage B), ~6 µs per draw each. 60 FPS needs < ~2.7 µs.

Done — **draw pipe batching** (commit b426b8b, `vk_draw_pipe.h`, also merged into odin-sphere):
each draw was handed to stage B with a seq_cst store to the line B polls, plus a read of B's
position per Begin, plus a locked add on a counter sharing a line with A's. Now draws are
published every 16 (`VW_PIPE_BATCH`) and before A drains, waits for ring space, yields
(`YIELD` in liverpool.cpp) or ends a submission; the space check uses the last position seen.
18.6 → 27.3 FPS. bbport (the project this port was forked from) still publishes per draw.

Profile after that (perf, `--call-graph dwarf`):
- Stage A (`GpuComm`): ProcessGraphics + register packets ~25%, Rasterizer::Draw/PostDraw ~20%,
  waiting for B (DrainDrawPipe) ~12%, CollectRingBindings ~4%, TryPreparedPipeline ~2%.
  The prepared-draw fast path (TryPreparedPipeline) misses for most sprites: it requires the
  whole register checksum to match, and every sprite changes its user data (texture, vertex
  pointers); the regular path then rebuilds StageSpecialization (zeroed inline arrays of 32+16+
  16+16 entries and a fetch shader parse) per draw.
- Stage B (`DrawRec`): ~27% spin-waiting, ~69% work: ResolveVertexBuffers 21% (every 128-byte
  sprite quad goes through BufferCache::ObtainBuffer and a small guest copy), BindResources 14%,
  BindTextures 8%.

Next steps, by expected gain:
1. Stage B: a fast path for small per-frame vertex buffers (copy into the stream buffer instead
   of the buffer cache lookup); skip rebinding resources/textures identical to the previous draw.
2. Stage A: a pipeline fast path keyed on the pipeline-relevant state and the specialization
   inputs only (not user data addresses); cheaper register packet application.
3. Then test the God of War III options: `VW_DEFERRED_READBACK=1 VW_STALE_READBACK=1`
   (run.py enables them by default, run.sh does not) and toggle bits 35-37 (EarlyDrawInputs,
   ConstantRing, DrawPipeline; Odin Sphere needs them off — black/partial frames and flicker),
   one at a time, checking the battle and menus for glitches.
4. FPS measurement: frame dumps for every frame (`fps.sh`) cost time themselves; the overlay's
   FPS (`frame_ms_avg` in gpu/shim/vw_overlay.cpp, Render()) is the accurate one — log it to
   stdout behind an env variable for scripted runs.
5. Move `TITLE_MODULES` (link_modules.py) into `scripts/game_profiles.py` with a CUSA36381
   profile, like Odin Sphere's.

## Test machine notes

- Two GPUs: Radeon RX 560X (Polaris 11, `card1`, 01:00.0) and Vega 8 (Raven, 04:00.0). The
  renderer prefers the discrete one; `out/vw-gpu-capabilities --device` prints the one it uses.
  Both get `RADV_DEBUG=nohiz` automatically (game_profiles.radv_nohiz).
- `perf` works for user-space profiling of the game; uprobes (`perf probe -x
  out/gpu/libvwgpu.so`, addresses from `readelf -Ws | c++filt`) need sudo.

## Driving the game (scratchpad scripts, not in the repo)

The pad is a file (`VW_PAD_FILE`): `press.sh <seconds> <buttons>` writes button names
(cross, circle, options, up, …) or `lx=`/`ly=` for the left stick. Frame dumps
(`VW_FRAME_DUMP_TRIGGER`) show the game's own output without screen capture.
- `uo_run.sh [seconds]`: launch through run.sh, log to `uo.log`, print the first STOP.
- `uo_to_battle.sh`: launch and press through title → New Game → story (≈120 Cross presses with
  1.7 s gaps) to the first battle; timing varies, a few extra presses may be needed.
- `fps.sh [seconds]`: frames presented while a dump is requested every frame (a lower bound).

## Lessons

- A missing import first: check whether it is only missing a name (`import_names.inc`), and
  whether the game ships the library in `sce_module` (link it natively) before writing a stub.
- Cross-thread handoffs per draw cost more than the work for sprite-heavy games: look for
  seq_cst stores and shared cache lines in `perf annotate` (the sample lands on the instruction
  after the stall).
