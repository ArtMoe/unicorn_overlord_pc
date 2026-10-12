#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Prepares the game image and starts gow3-probe (Windows and Linux; run.sh is the Linux
development script).

Same environment variables as run.sh (GOW3_GAME_DIR, GOW3_DATA_DIR, GOW3_PATCHES, GOW3_MODS_*,
GOW3_USER_DIR, ...). Extra arguments go to gow3-probe.
The in-game "Apply and restart" runs this script again through GOW3_RESTART_COMMAND;
`--after PID` waits for the previous game process to end first (its GPU device and memory).
"""
import ctypes
import os
from pathlib import Path
import shutil
import subprocess
import sys

PORT = Path(__file__).resolve().parent


def fail(message):
    print(message, file=sys.stderr)
    sys.exit(1)


WINDOWS = os.name == 'nt'


def wait_for(pid):
    if not WINDOWS:
        import time
        if pid == os.getpid():  # the restart exec'd this script in the game's own process
            return
        for _ in range(300):  # 30 s, as on Windows
            try:
                os.kill(pid, 0)
            except OSError:
                return
            time.sleep(0.1)
        return
    kernel32 = ctypes.windll.kernel32
    SYNCHRONIZE = 0x00100000
    handle = kernel32.OpenProcess(SYNCHRONIZE, False, pid)
    if handle:
        kernel32.WaitForSingleObject(handle, 30000)
        kernel32.CloseHandle(handle)


def no_console():
    """Started without a console (the launcher): console programs must not open their own."""
    if not WINDOWS:
        return 0
    return 0 if ctypes.windll.kernel32.GetConsoleWindow() else subprocess.CREATE_NO_WINDOW


def run_script(name, *args, capture=False):
    # Inside the packaged launcher executable (PyInstaller) the executable runs scripts itself.
    script = ['--script'] if getattr(sys, 'frozen', False) else []
    command = [sys.executable, *script, str(PORT / 'scripts' / name), *map(str, args)]
    if capture:
        result = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, text=True,
                                creationflags=no_console())
        if result.returncode:
            sys.exit(result.returncode)
        return result.stdout
    # stdin given: the output handles are passed explicitly (a windowed launcher child would
    # get none otherwise).
    result = subprocess.run(command, stdin=subprocess.DEVNULL, creationflags=no_console())
    if result.returncode:
        sys.exit(result.returncode)
    return None


def find_executable(name):
    """bin/ (packaged), else out/ (built with build.sh)."""
    for candidate in (PORT / 'bin' / name, PORT / 'out' / name):
        if candidate.is_file():
            return candidate
    return None


def main():
    args = sys.argv[1:]
    if len(args) >= 2 and args[0] == '--after':
        wait_for(int(args[1]))
        args = args[2:]
    # As run.sh: relative paths (GOW3_GAME_DIR, GOW3_DATA_DIR) are from the port.
    os.chdir(PORT)
    env = os.environ
    data = Path(env.get('GOW3_DATA_DIR', PORT))
    out = data / 'out'
    out.mkdir(parents=True, exist_ok=True)
    env.setdefault('GOW3_CONFIG', str(data / 'gow3.ini'))
    # Values this script derived on the previous start ("Apply and restart" inherits them) are
    # derived again from the new options.
    for key in env.pop('GOW3_DERIVED', '').split(','):
        env.pop(key, None)
    derived = []
    sys.path.insert(0, str(PORT / 'scripts'))
    import graphics_options
    graphics, reverted = graphics_options.prepare(env['GOW3_CONFIG'], out / 'graphics_state.json')
    if reverted:
        print('Graphics: the last restart never reached the game; previous options restored', flush=True)
    env.update(graphics_options.environment(graphics))

    probe = Path(env['GOW3_PROBE']) if env.get('GOW3_PROBE') else find_executable('gow3-probe.exe' if WINDOWS else 'gow3-probe')
    if not probe or not probe.is_file():
        fail('gow3-probe not found: build it with build.sh or use a packaged build.')
    # Development builds run from the MSYS2 tree: its CLANG64 DLLs (SDL3, FFmpeg, ...).
    import game_profiles
    capabilities = probe.parent / ('gow3-gpu-capabilities.exe' if WINDOWS else 'gow3-gpu-capabilities')
    if capabilities.is_file() and game_profiles.radv_nohiz(env, game_profiles.device_name(capabilities)):
        print('RADV: RADV_DEBUG=nohiz for this GPU (GOW3_AUTO_NOHIZ=0: off)', flush=True)
    if WINDOWS and not (probe.parent / 'SDL3.dll').is_file():
        clang64 = Path(env.get('MSYS2_ROOT', r'C:\msys64')) / 'clang64' / 'bin'
        if clang64.is_dir():
            env['PATH'] = f'{clang64}{os.pathsep}{env.get("PATH", "")}'

    if not env.get('GOW3_GAME_DIR'):
        fail('Set GOW3_GAME_DIR to the game folder (eboot.bin, sce_module, ...).')
    game = Path(env['GOW3_GAME_DIR'])
    if not (game / 'eboot.bin').is_file():
        fail(f'No eboot.bin in {game} (set GOW3_GAME_DIR).')
    original_game = game.resolve()
    game = Path(run_script('mods.py', game, '--out', out,
                           '--mods-dir', env.get('GOW3_MODS_DIR', data / 'mods'),
                           '--config', env.get('GOW3_MODS_CONFIG', data / 'mods.json'),
                           '--enabled', env.get('GOW3_MODS_ENABLED', '1'), capture=True).strip())
    mod_view = game if game.resolve() != original_game else None
    try:
        run_script('prepare.py', game, '--out', out)
        run_script('link_libc.py', game, '--out', out)
        run_script('link_modules.py', game, '--out', out)
        run_script('content_profile.py', game, '--out', out, '--sku', env.get('GOW3_CONTENT_SKU', 'full'))

        from patches import (eboot_matches, game_app_version, game_profile, game_title_id,
                             patch_requirements, selected_patches)
        profile = game_profile(game_title_id(game))
        patched = bool(profile) and ((game_app_version(game) == profile[2] and eboot_matches(game, profile))
                                     or bool(env.get('GOW3_FORCE_PATCHES')))
        # The window shows the game's icon (BMP: the format SDL loads without extra libraries).
        try:
            from PIL import Image
            with Image.open(game / 'sce_sys' / 'icon0.png') as icon:
                icon.convert('RGBA').resize((64, 64), Image.LANCZOS).save(out / 'window_icon.bmp')
            env.setdefault('GOW3_WINDOW_ICON', str(out / 'window_icon.bmp'))
        except (ImportError, OSError):
            pass
        # Per-game settings (scripts/game_profiles.py, shared with run.sh).
        game_profiles.apply(game_title_id(game), env)
        if patched:
            # The patch notes give the direct memory and VBlank rate the selected patches need.
            names = selected_patches(profile[1], profile[2], env.get('GOW3_PATCHES', ''), env.get('GOW3_PATCHES_ONLY') == '1')
            env['GOW3_PATCHES'] = ';'.join(names)
            env['GOW3_PATCHES_ONLY'] = '1'
            dmem, vblank = patch_requirements(profile[1], names, profile[2])
            for key, value in (('GOW3_DMEM_MB', dmem), ('GOW3_VBLANK_HZ', vblank)):
                if value and key not in env:
                    env[key] = str(value)
                    derived.append(key)
        run_script('patches.py', '--out', out, '--extra', env.get('GOW3_PATCHES', ''), '--game-dir', game,
                   '--patches-dir', env.get('GOW3_PATCHES_DIR', data / 'patches'),
                   '--patches-config', env.get('GOW3_PATCHES_CONFIG', data / 'patches.json'))
        if 'GOW3_VBLANK_HZ' not in env:
            env['GOW3_VBLANK_HZ'] = '60'
            derived.append('GOW3_VBLANK_HZ')
        env['GOW3_DERIVED'] = ','.join(derived)

        # The in-game restart starts this script again once this process is gone.
        # GPU caches (shaders, pipelines) beside the saves; read before main(), so set here.
        user_dir = Path(env.get('GOW3_USER_DIR', data / 'user')).resolve()
        user_dir.mkdir(parents=True, exist_ok=True)
        env.setdefault('GOW3_GPU_USER_DIR', str(user_dir))
        from save_backup import backup
        try:
            if made := backup(user_dir):
                print(f'Saves backed up to {made}', flush=True)
        except OSError as error:
            print(f'Save backup failed, starting anyway: {error}', flush=True)
        this = ['--run'] if getattr(sys, 'frozen', False) else [str(Path(__file__).resolve())]
        restart = [sys.executable, *this, '--after', str(os.getpid()), *args]
        if WINDOWS:
            env['GOW3_RESTART_COMMAND'] = subprocess.list2cmdline(restart)
        else:
            import shlex
            env['GOW3_RESTART_COMMAND'] = shlex.join(restart)
        command = [str(probe), str(out / 'boot-linked.bin'), '--content-profile', str(out / 'content.bin'),
                   '--patches', str(out / 'patches.bin'), '--app0', str(game),
                   '--user', str(user_dir),
                   '--timeout', env.get('GOW3_TIMEOUT', '0'), *args]
        # No stdin: an inherited pipe (shells such as Git Bash) cost the game its console output.
        return subprocess.run(command, stdin=subprocess.DEVNULL, creationflags=no_console()).returncode
    finally:
        if mod_view:
            shutil.rmtree(mod_view, ignore_errors=True)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
