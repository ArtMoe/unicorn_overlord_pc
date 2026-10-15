# Troubleshooting

## Where things are

The **data directory** holds settings, saves and logs:

| How you run the port | Data directory |
|---|---|
| Packaged launcher (`.exe` or `.AppImage`) | `data/` beside the launcher file |
| From the source tree | `run_uo/` in the repository (or `VW_DATA_DIR`) |

Inside it: `vw.ini` (settings), `user/` (saves and shader caches), `logs/` (one log per run,
newest 10 kept), `last_run.log` (copy of the newest log). Saves are backed up automatically
before each start.

## The launcher refuses the game folder

- The folder must contain `eboot.bin`, `sce_module/` and `sce_sys/` directly.
- It must be **CUSA36381 version 01.04**: the base game with the 1.04 update applied on top
  (update files replace base files). The base game alone is 01.01 and is refused.

## The game does not start or closes at once

1. Open the launcher's **Log** button, or `logs/` in the data directory.
2. Look for a line starting with `STOP:` (something the port does not support yet) or
   `Guest fault` / `Host fault` (a crash, followed by where it happened).
3. Check the GPU line near the top: the port needs a Vulkan 1.3 GPU and an up-to-date driver.

## Black screen, wrong colours or missing graphics

- Turn the GPU readback options off in the in-game menu (**Insert**, Performance tab) and
  restart.
- Set the render resolution back to native (1080p).
- If an option change stops the game from starting, the next start restores the previous
  options by itself.

## The game freezes

Start it with `VW_HANG_DUMP=5`. Every 5 seconds the log then shows where each game thread is.
Attach that log to the bug report.

## Reporting a bug

Open an issue with the bug report form and attach the log of the run that failed. See
[CONTRIBUTING.md](../CONTRIBUTING.md#reporting-bugs) for what helps most.
