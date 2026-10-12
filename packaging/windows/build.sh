#!/usr/bin/env bash
# One-shot Windows build: run in the MSYS2 CLANG64 shell from the repository root.
#   bash packaging/windows/build.sh [game]     (game: odin, ...; default odin)
# Installs the toolchain and libraries, fetches the submodules, builds out/vwport.exe and packages
# dist/<Title> PC/ plus its .zip (packaging/package.py, with a python.org Python).
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
if [[ ${MSYSTEM:-} != CLANG64 ]]; then
    echo 'Run this in the "MSYS2 CLANG64" shell (Start menu), not MSYS/UCRT64/PowerShell.' >&2
    exit 1
fi
pacman -S --needed --noconfirm git patch \
    mingw-w64-clang-x86_64-{toolchain,cmake,ninja,pkgconf,python} \
    mingw-w64-clang-x86_64-{vulkan-headers,vulkan-loader,vulkan-memory-allocator,glslang} \
    mingw-w64-clang-x86_64-{sdl3,fmt,boost,robin-map,xxhash,ffmpeg,zydis} \
    mingw-w64-clang-x86_64-{spirv-tools,spirv-headers,spirv-cross}
git submodule update --init --recursive
bash build.sh
# PyInstaller needs a python.org Python (the py launcher), not MSYS2's.
if ! command -v py >/dev/null; then
    echo 'Built out/vwport.exe. To package, install Python from python.org' >&2
    echo '(winget install Python.Python.3.12) and run this script again.' >&2
    exit 1
fi
py -3 packaging/package.py "${1:-odin}"
