# Known issues

This is an early, work-in-progress port. What is listed here is known; please don't open a new
issue for it unless you have new information (a log, a save, a way to reproduce it).

## Test system

All numbers and test results here come from one laptop:

| Part | Model |
|---|---|
| CPU | AMD Ryzen 5 2500U (4 cores / 8 threads, 2.0-3.6 GHz) |
| GPU | AMD Radeon RX 560X (Polaris 11, 4 GB, laptop), on AC power |
| Integrated GPU | Radeon Vega 8 (not used by the game) |

Results from other hardware are welcome in the issues.

## How far the game has been tested

- Unicorn Overlord has been played through the prologue, into chapter 1 and its first real
  battle (Palevia Town). Saving works (the game autosaves before that battle).
- **Not tested yet:** the end of the first real battle, the world map, loading a save from the
  title screen, and the game's pre-rendered movies. Anything past chapter 1 is unknown ground.
- Odin Sphere Leifthrasir uses the same engine. Since the 2026-10-13 cleanup it has only been
  checked up to the title screen.

## Supported dumps only

- Unicorn Overlord: only **CUSA36381 (Asia) version 01.04** (base game with the 1.04 update
  merged on top). The launcher refuses other regions (US, EU, Japan) and other versions; they
  have different executables and the per-game code patches would not match.
- Odin Sphere Leifthrasir: only **CUSA05083 (US) version 01.01**.

## Performance

- The first battle map runs at about **53 FPS** on the test system above, not a steady 60. Large battles draw ~6000 sprites per frame.
- The limit is the CPU, not the GPU: the PS4's GPU reads its command stream in hardware, the
  port decodes it in software on one thread ("stage A"). A faster CPU (higher single-core
  speed) helps most. Splitting that work across threads is the next planned step; see
  [dev/parallel_gpu.md](dev/parallel_gpu.md) and [dev/unicorn/HANDOFF.md](dev/unicorn/HANDOFF.md).
- The first time a scene is shown, its shaders are compiled, which can stutter. Later runs use
  the shader cache in `user/`.

## Graphics

- The two GPU readback options are **on by default** because they are faster. If something
  looks wrong (black screen, missing or flickering graphics, wrong lighting or shadows), turn
  them off: in-game menu (**Insert**) → Performance → "Game reads GPU data without waiting" and
  "Deferred GPU readbacks", then **Apply and restart**. If the game then fails to start, the
  next start goes back to the previous options by itself.
- A render resolution change in the in-game menu only applies after a restart.
- Render resolutions above 1080p use the game's own PS4 Pro mode. Only 1440p and 4K have been
  checked (title, menu and battle); 1800p has not.
- On Linux with AMD GPUs, `RADV_DEBUG=nohiz` is set automatically to avoid rendering errors.
- On laptops with two GPUs the port picks the discrete one.
- Needs a Vulkan 1.3 GPU and a recent driver. macOS is not supported.

## Builds

- New packages include `LICENSE`, third-party notices in `licenses/` and a help file. Windows
  downloads are ZIP archives; keep their notices alongside the executable. Older test builds
  may lack these files. Matching source and debug symbol archives accompany new builds.
- CI tests native code without a GPU or game dump. Successful CI does not establish a full
  playthrough or driver compatibility; packaged builds still need Windows/Linux gameplay tests.

## Not supported (by design, for now)

- Everything online: PSN sign-in, network features and trophies. They all report "signed out"
  or succeed without doing anything, so the game runs offline.
- Remapping controls in the launcher. Buttons can be remapped with the `VW_KEY_MAP` (keyboard)
  and `VW_PAD_MAP` (controller) environment variables, e.g. `VW_KEY_MAP="cross=Q"`; sticks,
  triggers and the menu shortcut can't be remapped.
- DLC and PS4 package installation: the port runs an already extracted game folder.

## Open questions

- The 1.04 update's `UnicornPatch.CPK` is 1.0 GB while the base game's is 3.3 GB. Nothing has
  been found missing so far.
