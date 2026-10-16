# Session handoff (2026-10-16): public release, start program, remapping, native settings

Read this first, then `docs/dev/unicorn/HANDOFF.md` (the port itself) and
`docs/KNOWN_ISSUES.md`. The owner is new to GitHub: explain git and GitHub steps in plain words,
and ask before anything outward-facing (push, merge, force-push, repo settings).

## Continuing on another computer

The work continues on a different machine. Everything needed is on GitHub; the paths in the
table below are the **previous** PC's and will differ.

1. **Clone fresh** (the history was rewritten on 2026-10-16, so an older clone can't be pulled):
   `git clone --recursive https://github.com/hachi23/unicorn_overlord_pc` then
   `git switch feat/setup-and-ingame-settings` (this work; PR #11's docs are already on `main`).
2. Set the repository-only git identity: `git config user.name hachi23` and
   `git config user.email 88461786+hachi23@users.noreply.github.com`.
3. Put your own CUSA36381 v01.04 dump (base + 1.04 EN patch, merged) somewhere and point
   `VW_GAME_DIR`/the setup dialog at it. Game files are never committed.
4. Windows: install MSYS2 and build (see "How to build" below; `packaging/windows/build.sh`
   installs the packages). Linux: `bash build.sh` (`shell.nix` lists the packages).
5. The Ghidra project is **not** in the repository. Recreate it: run `scripts/prepare.py`,
   `link_libc.py`, `link_modules.py` on the dump with `--out <re>/out`, then
   `python tools/odin_re/make_ghidra_elf.py . <re>/out <re>`, then the analyzeHeadless import
   below with `-import <re>/eboot_ghidra.elf -postScript LabelImports.java <re>/imports.tsv` and
   `GHIDRA_HEADLESS_MAXMEM=12G` (2 GB runs out of memory; analysis takes ~15 min).
6. Update `tools/uo_drive.ps1`'s two paths (port folder, dump folder).

## Where things are (previous PC)

| What | Where |
|---|---|
| Repository (public) | https://github.com/hachi23/unicorn_overlord_pc, default branch `main` |
| Local clone | `D:\Emulation\Recomps\unicorn_overlord_pc` (the only copy; the old `D:\Emulation\Recomps\Unicorn Overlord` was deleted) |
| Game dump (merged base 1.01 + 1.04 EN patch) | `D:\Emulation\Games\ps4\ShadPs4Plus-PkgExtractor-1.1-windows\CUSA36381` |
| Run/data folder for tests | `D:\Emulation\Recomps\unicorn_overlord_pc\run_uo` (vw.ini, logs/, user/, launcher.json, frame dumps in dump/) |
| Ghidra project (UO eboot, analyzed) | `D:\Emulation\Recomps\uo_re\ghidra-project`, program `eboot_ghidra.elf`; prepared image in `uo_re\out`, `uo_re\imports.tsv` |
| Ghidra + JDK | `C:\Users\tusha\doax3-mod\toolchain\ghidra_12.1.2_PUBLIC`, `...\jdk-21.0.12.1+1` |
| MSYS2 | `C:\msys64` (CLANG64 packages installed) |
| Ghidra helper scripts | `tools/odin_re/ghidra_scripts/` (Decomp, Xrefs, CallArgs, CallUp, Deref, FindImm, Table, ...) |

Test PC: Ryzen 7 7800X3D, Radeon RX 9070 XT, 31 GB, Windows 11. The game runs here (title
screen at about 25 s). The numbers in KNOWN_ISSUES.md come from the original laptop (Ryzen 5 2500U
+ RX 560X); keep them labelled as such.

Git identity is set for this repository only: `hachi23` /
`88461786+hachi23@users.noreply.github.com` (keeps the real email out of public history). End
commits with the `Co-Authored-By` line the session gives you.

## Branches and pull requests

| Branch | State |
|---|---|
| `main` | Includes **PR #11** (merged 2026-10-16): README "What it is", credits, `docs/KNOWN_ISSUES.md`, test-system table, readback how-to |
| `feat/setup-and-ingame-settings` | Pushed, no PR yet. The work in progress below. Open a PR into `main` once the open items are fixed |

Other open PRs: #10 (roadmap doc from an earlier session), #6-#9 (Dependabot action bumps, CI
green, safe to merge; they may need "Update branch" after the history rewrite). No branch
ruleset is applied yet (see `docs/RELEASING.md`).

**History rewritten (2026-10-16, at the owner's request):** the download file names that were in
old revisions of `docs/dev/unicorn/HANDOFF.md` were replaced in every commit of every branch and
the `latest` tag, then force-pushed. All commit ids changed: old clones (also yogesh1239's fork)
must re-clone or reset. GitHub still keeps the old commits behind closed pull requests
(refs/pull/*), which only GitHub Support can purge.

## What the owner asked for (2026-10-16)

1. Docs: test system noted (done in PR #11); remove "downloads never played by hand" (done; the
   owner will test the builds).
2. Readback options can be switched off (they already can: in-game menu → Performance; documented).
3. **No launcher window.** One-time setup: choose the UO dump (CUSA36381 v01.04 with the English
   patch) → checked → prepared → game starts; later starts go straight in. **All settings inside
   the game.**
4. **Settings as real rows in the game's own System → Settings screen** ("fully native rows",
   chosen explicitly over a hybrid/overlay). Not the Insert overlay. Includes resolution,
   sharpening/FSR, GPU readback options, display mode/VSync/FPS cap, ultrawide, remapping.
5. **Ultrawide support.**
6. Controller remapping.
7. Ghidra for any further graphics options (an earlier survey found only the PS4 Pro switch; see
   unicorn/HANDOFF.md "Graphics options in the game code"). Lower priority; the owner did not
   tick it in the last question.

## Done on `feat/setup-and-ingame-settings`

### Start program (launcher/vw_launcher.py rewritten)
- No window. `main(key)`: reads `data/launcher.json` `game_dir`; if missing/invalid or
  `--choose-game`: info box → folder dialog (Tk, hidden root) → `check_game` (title id +
  APP_VER from param.sfo) → retry/cancel box. Then sets `VW_GAME_DIR/VW_DATA_DIR/VW_CONFIG`,
  `VW_START_COMMAND` (its own command line) and runs run.py **in-process** (`run_role(['--run'])`).
  Non-zero exit → message box offering to open `last_run.log`.
- `--after PID` waits for the old game process (used by the in-game "Change game folder").
- `GAMES` keeps only what package.py needs (title, colors for the icon) plus texts.
- run.py: sets `VW_CHOOSE_GAME_COMMAND = VW_START_COMMAND + " --choose-game --after <pid>"`.
- src/probe.c: `runtime_restart_with(env_name)`; `runtime_restart()` calls it with
  `VW_RESTART_COMMAND`.
- **Tested:** start with a saved folder → game reaches the title (log `run_uo/setup.out`).
  **Not tested:** the first-run dialogs (need a human), packaged PyInstaller build
  (`packaging/package.py` still passes `--hidden-import PIL._tkinter_finder`, harmless; the
  `--windowed` exe now never shows a window except dialogs), the "Change game folder" button.

### Remapping (src/runtime_pad.c + in-game Controls tab)
- vw.ini `key_map=` / `pad_map=` in the existing VW_KEY_MAP format, extended: several sources
  with `|` (`cross=Q|Space`), `none` unbinds. Load order: defaults → vw.ini → env.
- C API for the menu: `runtime_pad_button_count/_button_name/_describe/_bind/_reset/_spec`
  (spec = only buttons that differ from the defaults).
- Bridge: `vwgpu_setting_text()` (gpu/vwgpu.h) reads text settings; `VwSettings::Text/SetText`
  keep `key_map`/`pad_map` and `Save()` writes them (Save rewrites the whole file, so every key
  must be known to vw_settings.cpp).
- Overlay: new "Controls" tab (4 tabs now; `menu_tab` clamp 0..3), click a box → next key / pad
  button binds (Esc or a pad button cancels a key binding; Insert and names containing `, | =`
  are refused), right click clears, reset buttons. "Change game folder" button in the Overlay
  tab's About section.
- Stubs added in tests/test_gpu_runtime_stubs.cpp, tests/test_pad.c, tests/test_pad_map.c.
- **Tested:** builds (`bash build.sh` in CLANG64, ~1 min); `key_map=cross=Q|Space,circle=none`
  in vw.ini → log "pad map: 0 gamepad and 2 keyboard buttons remapped". graphics-settings-test
  passes. Python tests: 79 run, 3 errors that also fail on unmodified main (Windows symlink
  privilege / file locking in test_mods), 29 skipped.
- **Open item 1: `out/gpu/pad-map-test` hangs** with the new assertions added at the end of
  tests/test_pad_map.c (the old assertions passed before they were added). Not diagnosed. The new
  code calls `runtime_pad_bind/describe/spec/reset`, which take `lock` and call
  `load_button_maps()`; check for a deadlock or an endless loop in `apply_button_map`'s `|` split
  (`for (char *part=names, *bar; part && valid; part=bar)`) or in `runtime_pad_spec`. Run it
  under gdb in CLANG64 or add prints with `setvbuf(stdout,NULL,_IONBF,0)`.
- The Controls tab is in the **overlay**; per point 4 it should eventually be native rows (or at
  least reachable from the game's menu). Keep the C API; only the UI moves.

## Next: native rows in the game's Settings screen (the main job)

Goal: extra rows (e.g. a "PC" page or rows appended to Settings) drawn and navigated by the
game itself, values stored in vw.ini through `VwSettings`.

What is known so far:
- The eboot contains `Option_FUNC`, `Option_FUNC_STEP`, `Option_STEP`, `OptionIgnore`,
  `Config`, `Config_ASR`, `configgroup`, `Setting_`, `Volume`, `Camera` (quick string scan of
  `uo_re/out/eboot.elf`). No menu layout file names in the eboot: the layout and texts are data
  inside the CPKs (`Unicorn.CPK` 6.3 GB, `UnicornPatch.CPK` 1.0 GB, `Unicorn_CN/TW.cpk`).
- The engine is Vanillaware's (same family as Odin Sphere Leifthrasir; `docs/dev/odin/RE_NOTES.md`
  may have overlapping structures).

Suggested plan:
1. Ghidra: xrefs to `Option_FUNC`/`Option_STEP`/`configgroup` → the parser of the options table
   and the per-row callback table. Find how a row's value is read/written (the game's save of its
   own settings: `sceSaveData` / system file in `user/`).
2. Find where the CPK option definition and its text are loaded (file open path through
   `runtime_file.c`; log opened file names with the runtime's file tracing to see which CPK
   entry the Settings screen reads when it opens).
3. Extract that entry (CRI CPK tools: e.g. YACpkTool/CriPakTools; the port's mod system can
   replace loose files: `docs/MODS.md`, mods replace top-level entries of the game folder) and
   decode its format.
4. Choose the injection: (a) a modded data file with extra rows + extra text entries, served
   through the mod system, and native code hooks (`patches/` XML or a loader patch like
   `src/unicorn_render_scale.h`) that route the new rows' get/set to `VwSettings`; or (b) hook the
   row-building function in code and append rows at run time. (a) is easier to keep working.
5. Map each port setting to a row: render resolution (restart), display mode, VSync, FPS limit,
   FSR 1, sharpening + strength, stale/deferred readback (restart), async shaders, aspect ratio
   (ultrawide), overlay on/off. Restart-needed rows reuse the existing "Apply and restart" path
   (`restart_unconfirmed`, run.py reverts if a launch never shows frames).
6. Keep the Insert overlay as a fallback until the native rows work.

## Next: ultrawide

- Rendering size is set by the size getters `FUN_0003e2b0` (width) / `FUN_0003e2d0` (height) and
  inlined copies in `FUN_0003cb60`; the port patches their `mov ecx, imm32` operands for the Pro
  path (`src/unicorn_render_scale.h`, immediates at 0x3d9fd/0x3db77/0x3e2bd and
  0x3da19/0x3db5b/0x3e2dd). Viewport from the getters in `FUN_00041c50`; display setup
  `FUN_0003fa20`. Base (non-Pro) path uses 1920x1080.
- To do: find the 2D camera / projection (look for 1920.0f `0x44f00000`, 1080.0f `0x44870000`,
  16/9 `0x3fe38e39`, 0.5625 `0x3f100000`) and how the UI anchors (likely a fixed 1920x1080
  virtual canvas). Try a wider render target with a wider camera, UI centred or anchored to the
  edges; then the presenter (gpu/shim) must show a non-16:9 image without stretching (check its
  letterbox/fit logic). Add an `aspect_ratio` setting (16:9, 21:9, 32:9, auto from the window).
- Test with frame dumps: `VW_FRAME_DUMP_TRIGGER` + `VW_DUMP_DIR`, convert the `.raw` (BGRA) to
  PNG (Pillow is installed for the python.org Python).

## How to build, run and test (Windows)

```powershell
# Build (about 1 minute):
$env:MSYSTEM='CLANG64'; $env:CHERE_INVOKING='1'; Set-Location D:\Emulation\Recomps\unicorn_overlord_pc
& C:\msys64\usr\bin\bash.exe -lc "bash build.sh 2>&1 | tail -5"
# A C test target:
& C:\msys64\usr\bin\bash.exe -lc "source ./msys2-env.sh; ninja -C out/gpu pad-map-test && out/gpu/pad-map-test"
# Python tests:
py -3 -m unittest discover -s tests
```

Scripted runs: `tools/uo_drive.ps1 -Name <n> -Steps "25:dump","30:cross" -Seconds 60 [-Launch setup]`
(paths inside are this PC's) and `python tools/raw2png.py <dumps>.raw`. By hand: set `VW_GAME_DIR` (or a `run_uo/launcher.json` with `game_dir` and start
`launcher/unicorn_launcher.py`), `VW_DATA_DIR=run_uo`, `VW_PAD_FILE=run_uo/pad.txt` (write button
names like `cross`, empty = release), `VW_FRAME_DUMP_TRIGGER=run_uo/dump_now` (create the file to
dump a frame), `VW_DUMP_DIR`. The game window opens on the owner's desktop; say so first.

Ghidra headless (needs a big heap; 2 GB ran out of memory):

```powershell
$tc='C:\Users\tusha\doax3-mod\toolchain'; $env:JAVA_HOME="$tc\jdk-21.0.12.1+1"; $env:GHIDRA_HEADLESS_MAXMEM='12G'
& "$tc\ghidra_12.1.2_PUBLIC\support\analyzeHeadless.bat" D:\Emulation\Recomps\uo_re\ghidra-project uo `
  -process eboot_ghidra.elf -noanalysis -readOnly `
  -scriptPath D:\Emulation\Recomps\unicorn_overlord_pc\tools\odin_re\ghidra_scripts -postScript Decomp.java <args>
```

## Docs to update when the features land

README (Playing: no launcher, first-run setup, settings in the game; Keyboard: remappable),
`docs/KNOWN_ISSUES.md` ("Not supported": remapping is now supported; add ultrawide state),
`docs/TROUBLESHOOTING.md` ("The launcher refuses the game folder" → the setup dialog; the
launcher's Log button no longer exists: logs are in `data/logs/`), `packaging/windows/README.md`
and `docs/RELEASING.md` wording ("launcher" → "start program"), `CONTRIBUTING.md` area names.
