# Windows build

The ports build and run natively on 64-bit Windows 10 (1903+) / 11 from the same sources as on
Linux, with the MSYS2 CLANG64 toolchain. Windows builds have not been retested since the Linux
cleanup of 2026-10-13: report the first error of `out/gpu-build.log` if one fails.

## Build and package

1. Install [MSYS2](https://www.msys2.org) (`winget install MSYS2.MSYS2`) and, for packaging,
   Python from python.org (`winget install Python.Python.3.12`; PyInstaller needs it).
2. Open the **MSYS2 CLANG64** shell, clone the repository and run the one-shot script:

   ```bash
   git clone --recursive https://github.com/hachi23/unicorn_overlord_pc
   cd unicorn_overlord_pc
   bash packaging/windows/build.sh unicorn
   ```

   It installs the packages, builds `out/vwport.exe` and writes `dist/<Title> PC/` and its .zip:
   the launcher (with the runtime scripts inside), `vwport.exe`, the CLANG64 DLLs it loads,
   `README.txt` and `LICENSE`.

For development, `bash build.sh` builds `out/vwport.exe` and
`VW_GAME_DIR=D:/path/CUSA05083 python run.py` starts a game. From the source tree `run.py` puts
`C:\msys64\clang64\bin` (or `$MSYS2_ROOT`) on `PATH` for the DLLs.

## What is different from Linux

| Linux | Windows |
|---|---|
| `mov rax, fs:[0]` rewritten to `gs:[0]`, GS base set with `arch_prctl` | GS is the TEB and cannot move. The loader rewrites the displacement to `gs:[0x1480 + 8*slot]` (the TEB's `TlsSlots[slot]`, same 9-byte instruction) and each guest thread keeps its TCB in that TLS slot. The slot is claimed first thing in `main` (Vulkan drivers take many). |
| One memfd for direct + flexible memory, `mmap(MAP_FIXED)` views | A pagefile-backed section; the PS4 range 64 GiB–1 TiB is reserved as one placeholder and views replace placeholders (`VirtualAlloc2`/`MapViewOfFile3`, as in shadPS4). Partial unmaps split views and restore page protections (GPU tracking). |
| `-no-pie`, heap below 1 TiB | `--disable-high-entropy-va`: heaps and thread stacks stay low and out of the PS4 range. |
| SIGSEGV handler, `sigsetjmp` recovery | Vectored exception handler; `vw_setjmp`/`vw_longjmp` (`src/compat_win.c`) restore all Win64 callee-saved registers without SEH unwinding (guest frames have no unwind data; clang's `__builtin_longjmp` restores a wrong frame pointer on Win64). |
| Guest thread stacks from `runtime_low_map` | Windows allocates them, fully committed (`pthread_attr_setstacksize`): guest code has no stack probes and would skip the guard page. The main thread stack is 16 MiB committed. |
| glibc `nanosleep`, monotonic condition variables | High-resolution waitable timers (`compat_sleep_ns`; winpthreads' sleep rounds to 15.6 ms); semaphore deadlines use `CLOCK_REALTIME` (winpthreads rejects monotonic condition variables). |
| `libvwgpu.so` (or `build.sh --static`) | `libvwgpu.a`, linked into `vwport.exe` by CMake (`gpu/CMakeLists.txt`), with a manifest for UTF-8 paths and long paths. |
| In-game restart: `exec` of `VW_RESTART_COMMAND` | `run.py` puts its command line in `VW_RESTART_COMMAND`; the new process waits for the old one (`--after PID`). |
| Mod overlay with symlinks | Symlinks when allowed (Developer Mode), else junctions for folders and hard links for files. |

Diagnostics: `VW_FRAME_DUMP_TRIGGER=<file>` writes the guest display buffer and the presented
image of the next frame to `VW_DUMP_DIR` (raw 32-bit pixels), which also
works for menus and movies.

Not ported: the watchdog/`--timeout` thread dumps (Linux signals), userfaultfd tracking
(`VW_UFFD`), MangoHud, PGO (GCC flags) and
the `tests/` programs (several use Linux-only APIs).
