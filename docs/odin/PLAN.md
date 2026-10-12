# Odin Sphere Leifthrasir native port — plan

Target: CUSA05083 (US) app 01.01. Base: gow3_pc (PS4 eboot runs natively on x86-64,
runtime replaces PS4 system libraries, shadPS4 video core translates GPU work to Vulkan).
vw-specific hooks are gated on CUSA01623 01.02 and stay inert for this game.
RE workspace: `../re` (Ghidra project, notes, import tables).

## Phase 0 — Groundwork (done)
Clone + submodules, MSYS2 CLANG64 toolchain, clang 23 build fixes, prepare/link scripts run
on the dump (`out_odin/`), Ghidra project with relocations applied and 202/215 imports named.

## Phase 1 — First boot (loader + runtime gaps)
Goal: eboot reaches its main loop without crashing.
- Run `vwport` against CUSA05083; log every unresolved import and the first crash.
- Implement/stub the 35 imports vw never needed: NpTus, SystemGesture, ErrorDialog,
  Coredump, GameLiveStreaming, CommonDialog → safe stubs; libkernel (7), libc (2),
  SaveData GetMountInfo → real implementations.
- Fix assumptions in memory/threads/TLS that only held for vw.
Done when: the game thread loop runs and the frame function (FUN_006b3eb0) is reached.

## Phase 2 — Files, data and saves
Goal: game loads its data.
- app0 (base CPKs) + patch overlay (Odin2_US_PATCH1_RE_US.CPK), sce_discmap(_patch).plt.
- libSceFios2 path behaviour, CRI CPK streaming reads (large files, async I/O).
- Save data: mount/dirs/params/icon, GetMountInfo, user/profile services.
Done when: no missing-file errors in the log and saves can be written and read back.

## Phase 3 — Graphics to title screen
Goal: correct image on screen.
- VideoOut vblank events (AddVblankEvent/WaitVblank), SubmitAndFlipForWorkload, the 2
  unnamed GnmDriver imports.
- shadPS4 shader/format issues specific to this renderer (2D sprites, blend modes,
  render-target copies, MSAA if used).
Done when: intro movies are skipped or play and the title screen draws correctly.

## Phase 4 — Audio, input, movies
- sceAudioOut (CRI ADX2/HCA mixes in software, so likely only output ports needed).
- Pad mapping, touchpad (SystemGesture stubs → touchpad as button if the game needs it).
- Movies: CRI Sofdec (USM), decoded by the game; check timing and that audio stays in sync.
Done when: menus, gameplay and cutscenes have sound and controls work.

## Phase 5 — Playability and stability
- Play through each character's opening chapter; fix crashes, softlocks, rendering bugs.
- Performance: shader cache warm-up, stutter, thread priorities.
- Frame pacing at 60 fps through the game's own limiter (FUN_006b3eb0).
Done when: a full chapter plays without errors at a steady 60 fps.

## Phase 6 — Launcher and packaging
- Rebrand the launcher/settings (title, icon, title ID checks, game dir), Odin-specific patch
  XML profile keyed on the 01.01 eboot sha256, Windows package.

## Phase 7 — Enhancements
- Resolution scaling + FSR1/EASU-RCAS (2D art: spatial upscaling, not FSR3 motion vectors).
- High fps: unlock target fps (ctx+0x54), first check whether logic steps per frame; add
  interpolation or decouple update/render if needed.
- Cheats (HP, phozons, items, money) through in-memory patches and the ImGui menu.
- Graphics options (filtering, UI scale, ultrawide if layout allows).
