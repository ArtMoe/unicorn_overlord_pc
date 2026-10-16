# Known issues

This is an early, work-in-progress port. What is listed here is known; please don't open a new
issue for it unless you have new information (a log, a save, a way to reproduce it).

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

- The first battle map runs at about **53 FPS** on the test laptop (Ryzen 5 2500U with a
  Radeon RX 560X), not a steady 60. Large battles draw ~6000 sprites per frame.
- The limit is the CPU, not the GPU: the PS4's GPU reads its command stream in hardware, the
  port decodes it in software on one thread ("stage A"). A faster CPU (higher single-core
  speed) helps most. Splitting that work across threads is the next planned step; see
  [dev/parallel_gpu.md](dev/parallel_gpu.md) and [dev/unicorn/HANDOFF.md](dev/unicorn/HANDOFF.md).
- The first time a scene is shown, its shaders are compiled, which can stutter. Later runs use
  the shader cache in `user/`.

## Graphics

- The GPU readback options (deferred and stale readbacks) are **on by default** because they are
  faster. If something looks wrong (black screen, missing or flickering graphics), turn them off
  in the in-game menu (**Insert**, Performance tab) and restart.
- A render resolution change in the in-game menu only applies after a restart.
- Render resolutions above 1080p use the game's own PS4 Pro mode. Only 1440p and 4K have been
  checked (title, menu and battle); 1800p has not.
- On Linux with AMD GPUs, `RADV_DEBUG=nohiz` is set automatically to avoid rendering errors.
- On laptops with two GPUs the port picks the discrete one.
- Needs a Vulkan 1.3 GPU and a recent driver. macOS is not supported.

## Builds

- The downloads on the `latest` pre-release are built automatically from `main` and have **not
  been played by hand**. Windows in particular: the static Windows build is made by CI but
  hasn't been tested by playing since the 2026-10-13 cleanup.
- The download contains only this project's `LICENSE`, not the license texts of the bundled
  libraries (FFmpeg, SDL3, Dear ImGui and the others listed in the README). This is planned
  (first item of the roadmap in pull request #10).

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
