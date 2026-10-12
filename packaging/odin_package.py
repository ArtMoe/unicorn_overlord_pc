#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Packages the Odin Sphere Leifthrasir port (Linux, or Windows from MSYS2 CLANG64).

Build first (bash build.sh), then: python3 packaging/odin_package.py
Result: dist/Odin Sphere PC/ and an archive of it (.tar.gz on Linux, .zip on Windows):

    Odin Sphere(.exe)   the launcher (PyInstaller, one file; it also runs the game's preparation
                        scripts, so no Python installation is needed)
    README.txt
    runtime/            run.py, scripts/, patches/, bin/ (gow3-probe, GPU library, DLLs)
    data/               created on first start: gow3.ini, saves, generated files

No game file is included: the launcher asks for the folder of the user's own dump.
"""
import ast
import os
from pathlib import Path
import shutil
import subprocess
import sys
import sysconfig
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parent.parent
WINDOWS = os.name == 'nt' or sys.platform == 'msys'
NAME = 'Odin Sphere PC'
OUT = ROOT / 'out'
DIST = ROOT / 'dist' / NAME
VENV = OUT / 'odin-pyenv'


def run(*command, **kwargs):
    print('+', ' '.join(map(str, command)), flush=True)
    subprocess.run([str(c) for c in command], check=True, **kwargs)


def venv_python():
    python = VENV / ('Scripts/python.exe' if WINDOWS else 'bin/python')
    if not python.is_file():
        run(sys.executable, '-m', 'venv', VENV)
        run(python, '-m', 'pip', 'install', '-q', '--disable-pip-version-check', 'pyinstaller', 'pillow')
    return python


def stdlib_imports():
    """Standard modules imported by run.py and the scripts (run inside the frozen launcher)."""
    stdlib = set(sys.stdlib_module_names)
    found = set()
    for source in [ROOT / 'run.py', *sorted((ROOT / 'scripts').glob('*.py'))]:
        for node in ast.walk(ast.parse(source.read_text(encoding='utf-8'))):
            names = []
            if isinstance(node, ast.Import):
                names = [a.name for a in node.names]
            elif isinstance(node, ast.ImportFrom) and node.module and not node.level:
                names = [node.module]
            for name in names:
                if name.split('.')[0] in stdlib and not (name.startswith('ctypes.wintypes') and not WINDOWS):
                    found.add(name)
    return sorted(found)


ICON_CODE = """
import sys
from PIL import Image, ImageDraw
size = 256
img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
draw = ImageDraw.Draw(img)
draw.ellipse((8, 8, size - 8, size - 8), fill=(13, 18, 38, 255), outline=(217, 180, 95, 255), width=10)
c, r, w = size / 2, 92, 22
draw.polygon([(c, c - r), (c + w, c - w), (c + r, c), (c + w, c + w), (c, c + r), (c - w, c + w),
              (c - r, c), (c - w, c - w)], fill=(246, 217, 139, 255))
draw.ellipse((c - 14, c - 14, c + 14, c + 14), fill=(13, 18, 38, 255))
img.save(sys.argv[1], sizes=[(256, 256), (64, 64), (32, 32), (16, 16)])
"""


def make_icon(python):
    """A gold four-pointed star on night blue (the game's own art is not shipped)."""
    path = OUT / ('odin-icon.ico' if WINDOWS else 'odin-icon.png')
    run(python, '-c', ICON_CODE, path)
    return path


def freeze(python):
    # PIL._tkinter_finder: loaded by ImageTk at run time, not seen by PyInstaller's analysis.
    hidden = [arg for module in [*stdlib_imports(), 'PIL._tkinter_finder'] for arg in ('--hidden-import', module)]
    run(python, '-m', 'PyInstaller', '--noconfirm', '--clean', '--log-level', 'WARN', '--onefile',
        '--windowed', '--name', 'Odin Sphere', '--icon', make_icon(python), '--distpath', OUT / 'pyi-dist',
        '--workpath', OUT / 'pyi-work', '--specpath', OUT / 'pyi-work', '--paths', ROOT / 'scripts',
        *hidden, ROOT / 'launcher' / 'odin_launcher.py', cwd=ROOT)
    return OUT / 'pyi-dist' / ('Odin Sphere.exe' if WINDOWS else 'Odin Sphere')


def copy_binaries(bin_dir):
    if WINDOWS:
        probe = OUT / 'gow3-probe.exe'
        shutil.copy2(probe, bin_dir)
        shutil.copy2(OUT / 'gow3-gpu-capabilities.exe', bin_dir)
        # The CLANG64 DLLs the probe loads (ldd lists them with their MSYS2 paths).
        listing = subprocess.run(['ldd', str(probe)], capture_output=True, text=True, check=True).stdout
        for line in listing.splitlines():
            parts = line.split('=>')
            if len(parts) == 2 and '/clang64/bin/' in parts[1]:
                dll = parts[1].split('(')[0].strip()
                shutil.copy2(subprocess.run(['cygpath', '-w', dll], capture_output=True, text=True,
                                            check=True).stdout.strip(), bin_dir)
    else:
        shutil.copy2(OUT / 'gow3-probe', bin_dir)
        shutil.copy2(OUT / 'gow3-gpu-capabilities', bin_dir)  # the game's GPU, for launch choices
        (bin_dir / 'gpu').mkdir()
        shutil.copy2(OUT / 'gpu' / 'libgow3gpu.so', bin_dir / 'gpu')  # RUNPATH $ORIGIN/gpu
        for binary in (bin_dir / 'gow3-probe', bin_dir / 'gow3-gpu-capabilities', bin_dir / 'gpu' / 'libgow3gpu.so'):
            subprocess.run(['strip', '--strip-unneeded', str(binary)], check=False)


README = """Odin Sphere Leifthrasir - native PC port
==========================================

1. Start "{exe}".
2. Click "Choose..." and select the folder of your own dump of the game:
   CUSA05083 version 01.01 (the folder that holds eboot.bin and sce_sys).
3. Pick the resolution and display options, then "Begin the Tale".

In game: the settings menu opens with Insert (keyboard) or R3+L2 (controller).
Saves, settings and generated files live in the "data" folder next to the launcher.
Nothing of the game is included in this package.
{requirements}"""

LINUX_REQUIREMENTS = """
Linux needs these system packages (most desktops have them already):
  Arch:          sudo pacman -S sdl3 ffmpeg vulkan-icd-loader fmt zydis xxhash
  Debian/Ubuntu: sudo apt install libsdl3-0 ffmpeg libvulkan1 libfmt-dev libzydis-dev libxxhash0
  plus your GPU's Vulkan driver (Mesa radv/anv, or NVIDIA).
"""


def main():
    probe = OUT / ('gow3-probe.exe' if WINDOWS else 'gow3-probe')
    if not probe.is_file():
        sys.exit('Build first: bash build.sh')
    python = venv_python()
    launcher = freeze(python)
    if DIST.exists():
        shutil.rmtree(DIST)
    runtime = DIST / 'runtime'
    (runtime / 'bin').mkdir(parents=True)
    shutil.copy2(launcher, DIST)
    shutil.copy2(ROOT / 'run.py', runtime)
    shutil.copytree(ROOT / 'scripts', runtime / 'scripts', ignore=shutil.ignore_patterns('__pycache__'))
    shutil.copytree(ROOT / 'patches', runtime / 'patches')
    shutil.copy2(ROOT / 'LICENSE', runtime)
    copy_binaries(runtime / 'bin')
    (DIST / 'README.txt').write_text(README.format(
        exe=launcher.name, requirements='' if WINDOWS else LINUX_REQUIREMENTS), encoding='utf-8')
    archive = DIST.parent / (NAME.replace(' ', '-') + ('-windows.zip' if WINDOWS else '-linux.tar.gz'))
    if WINDOWS:
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
            for path in DIST.rglob('*'):
                z.write(path, Path(NAME) / path.relative_to(DIST))
    else:
        with tarfile.open(archive, 'w:gz') as t:
            t.add(DIST, arcname=NAME)
    files = sum(1 for p in DIST.rglob('*') if p.is_file())
    print(f'{DIST}: {files} files; archive {archive} ({archive.stat().st_size >> 20} MiB)')


if __name__ == '__main__':
    main()
