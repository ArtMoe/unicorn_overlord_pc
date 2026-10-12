# vwport: native PC ports of PS4 games

Native Linux and Windows ports of two Vanillaware games for the PlayStation 4:

| Game | Dump | Branch | State |
|---|---|---|---|
| Odin Sphere Leifthrasir | CUSA05083 (US), app 01.01 | `odin-sphere` | Playable |
| Unicorn Overlord | CUSA36381 (Asia, English patch), app 01.04 | `unicorn-overlord` | Prologue and first battles playable; movies not implemented |

The game's own executable runs directly on the PC's processor (the PS4 uses the same x86-64
architecture). A runtime written for these games replaces the PS4 system libraries, and the
shadPS4 video core translates the graphics to Vulkan. It is not a general emulator.

> **No game files are included.** You need a dump of your own copy. This project is not
> affiliated with Sony Interactive Entertainment, Atlus or Vanillaware.

## Requirements

- 64-bit Linux, or Windows 10 (1903+) / 11.
- A Vulkan 1.3 GPU.
- Your own dump of a supported game, as a folder containing `eboot.bin`, `sce_module` and
  `sce_sys`.

## Playing

Start the launcher, pick the game folder, choose the graphics options and press **Play**.
Packaged builds contain the launcher and the game executable. From the
source tree:

```bash
python launcher/odin_launcher.py      # Unicorn Overlord: launcher/unicorn_launcher.py
```

In game, **Insert** (R3+L2 on a controller) opens the port's menu. It has three tabs:

- **Image:** display mode, VSync, FSR 1 and RCAS sharpening.
- **Performance:** frame rate limit, render resolution and the GPU readback options.
- **Overlay:** the performance overlay.

Settings are kept in `vw.ini` in the data directory and are shared with the launcher.

### Render resolution

Both games render at 1920×1080 by default. A higher render resolution draws the game larger
and averages it down to the window, which gives smoother edges at the cost of more GPU work.
Set it in the launcher, in the menu (applied on restart), or with `VW_RENDER_SCALE`:

| Game | Values | How |
|---|---|---|
| Odin Sphere | factor 1 to 3, or `1440p`, `1800p`, `4K` | Patches the game's render targets |
| Unicorn Overlord | `1440p`, `1800p`, `4K` | Uses the game's PS4 Pro path (`VW_NEO_MODE=1`) at that size |

### Keyboard

| Key | Button |
|---|---|
| WASD | left stick |
| Arrows | right stick |
| Space | Cross |
| Left Shift | Circle |
| E | Square |
| Q | Triangle |
| 1 and 3 | L1 and R1 |
| R and F | L2 and R2 |
| Z and C | L3 and R3 |
| I, K, J, L | d-pad |
| Enter | Options |
| Tab | touchpad |
| Insert | the port's menu |

## Building

Linux needs GCC or Clang, CMake, Ninja, Python 3, and the Vulkan, SDL3 and FFmpeg development
packages. `shell.nix` lists them.

```bash
git clone --recursive https://github.com/hachi23/odin_sphere_pc
cd odin_sphere_pc
bash build.sh                                  # out/vwport and out/gpu/libvwgpu.so
VW_GAME_DIR=/path/to/CUSA05083 bash run.sh     # builds, prepares the game and starts it
```

`run.sh` builds first and then runs `run.py`, which does the same on every platform. `run.py`
prepares the game image in `out/`, applies the mods and patches, and starts `vwport`. Saves
and shader caches go to `user/` in the data directory (`VW_DATA_DIR`, by default the port
folder).

On Windows, build in an MSYS2 CLANG64 shell. See
[packaging/windows/README.md](packaging/windows/README.md).

To package a release, run `python packaging/package.py odin`. On Linux it builds a static
`vwport` (`build.sh --static`, the GPU library linked in) and writes four files: the launcher
(with the runtime scripts inside), `vwport`, `README.txt` and `LICENSE`.

### Tests

```bash
python -m unittest discover -s tests
bash build.sh --test
```

The C++ tests are `EXCLUDE_FROM_ALL` targets in [gpu/CMakeLists.txt](gpu/CMakeLists.txt). For
example:

```bash
ninja -C out/gpu graphics-settings-test && out/gpu/graphics-settings-test
```

To check that every system function a game imports is implemented, without starting it:

```bash
out/vwport out/boot-linked.bin --app0 <game folder> --check-imports
```

## How it works

| Part | Where | What it does |
|---|---|---|
| Preparation | `scripts/` | Reads `eboot.bin`, applies its relocations and links the game's own system modules into one image |
| Loader | `src/probe.c` | Loads the image, applies the per-game code patches (render resolution) and starts the game's code |
| Runtime | `src/runtime_*.c` | The PS4 system functions: memory, threads, files, audio, video, controller, saves and services |
| GPU | `gpu/shadps4/` | The shadPS4 video core with this port's threaded draw pipeline ([notes](docs/parallel_gpu.md)) |
| In-game menu | `gpu/shim/vw_overlay.cpp` | The Dear ImGui menu and FPS overlay; settings in `gpu/shim/vw_settings.cpp` |
| Per-game settings | `scripts/game_profiles.py` | Launch defaults and GPU workarounds for each title |
| Mods and patches | `scripts/mods.py`, `scripts/patches.py` | Loose-file mods and shadPS4 XML patches ([docs](docs/MODS.md)) |
| Launcher | `launcher/` | Game folder, graphics options and the Play button |

Per-game notes: [Odin Sphere](docs/odin/HANDOFF.md), [Unicorn Overlord](docs/unicorn/HANDOFF.md)
(on its branch).

## Credits

- **deadinside28**: [bloodborne_pc](https://github.com/deadinside28/bloodborne_pc), the original
  native port that this approach comes from.
- **Supermedo**: [bloodborne_pc for Windows](https://github.com/Supermedo/bloodborne_pc).
- **Felipe Prado**: [gow3_pc](https://github.com/FelipePrado0/gow3_pc), the God of War III port
  this repository was forked from.
- **cuesta4**: [shadPS4 fork](https://github.com/cuesta4/shadPS4), for its GNM blend rewrite,
  depth/stencil state and image aliasing fixes.
- **shadPS4 team**: the [shadPS4](https://github.com/shadps4-emu/shadPS4) video core.

Also used: [LibAtrac9](https://github.com/Thealexbarney/LibAtrac9), [FFmpeg](https://ffmpeg.org),
[SDL3](https://github.com/libsdl-org/SDL), [Dear ImGui](https://github.com/ocornut/imgui),
[sirit](https://github.com/shadps4-emu/sirit), [magic_enum](https://github.com/Neargye/magic_enum),
[miniz](https://github.com/richgel999/miniz), [xbyak](https://github.com/herumi/xbyak),
[Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator),
AMD FidelityFX FSR 1 and CAS, and [Pillow](https://python-pillow.org).

## License

GNU GPL v2 or later ([LICENSE](LICENSE)). Third-party components keep their own licenses.
