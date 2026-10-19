# Troubleshooting

## Where things are

The **data directory** holds settings, saves and logs:

| How you run the port | Data directory |
|---|---|
| Packaged launcher (`.exe` or `.AppImage`) | `data/` beside the launcher file |
| From the source tree | `run_uo/` in the repository (or `VW_DATA_DIR`) |

Inside it: `vw.ini` (settings), `user/` (saves and shader caches), `logs/` (one unique log per run,
newest 10 kept), `last_run.log` (live copy of the newest log segment). Each run has at most
two 8 MiB segments (`.log` and `.log.1`) and a small system summary (`.json`). Saves are backed up automatically
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

## The log says the graphics device was lost

`GPU crash: the graphics device was lost` (exit code 23) identifies a Vulkan device-loss
failure. This alone does not establish whether the cause is in the port, driver or hardware.
Try a current driver and native render resolution; report the exact scene, graphics settings
and driver version with the failed run's diagnostics.

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

Open **Log → Export diagnostics**, select the timestamped run that failed, review/edit the
redacted preview and save the ZIP. Attach it to an issue using the bug report template.
The export includes only that run's log segments and system/settings summary. It excludes
game data, saves, keys and shader caches. Known local paths are redacted; review for other
personal information before sharing. During a hang you can export the active run without
closing the game. Very large legacy logs are limited to 16 MiB per exported segment.

You can also attach the timestamped `.log` and `.log.1` manually, after checking for personal
paths. If you restarted the game, select the earlier failed run rather than `last_run.log`. See
[CONTRIBUTING.md](../CONTRIBUTING.md#reporting-bugs) for what helps most.
