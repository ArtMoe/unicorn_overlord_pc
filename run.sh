#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
if [[ ${1:-} == --software ]]; then
    shift
    if [[ -z ${VK_DRIVER_FILES:-} ]]; then
        for candidate in /run/opengl-driver/share/vulkan/icd.d/lvp_icd*.json /usr/share/vulkan/icd.d/lvp_icd*.json; do
            if [[ -f $candidate ]]; then export VK_DRIVER_FILES=$candidate; break; fi
        done
    fi
    if [[ -z ${VK_DRIVER_FILES:-} ]]; then echo 'Lavapipe not found; set VK_DRIVER_FILES.' >&2; exit 1; fi
    export VK_LOADER_LAYERS_DISABLE='~implicit~'
fi
# VW_PREBUILT=1 (packaged builds, the AppImage): out/vwport and its GPU library are installed
# next to this script; nothing is built and no nix-shell is needed.
# VW_DATA_DIR: writable directory for the generated files (out/), saves (user/) and vw.ini;
# by default this directory.
data=${VW_DATA_DIR:-.}
out=$data/out
mkdir -p "$out"
export VW_CONFIG=${VW_CONFIG:-$data/vw.ini}
if [[ -z ${VW_PREBUILT:-} && -z ${VW_IN_NIX_SHELL:-} ]] && ! { command -v pkg-config >/dev/null && pkg-config --exists vulkan sdl3; } && command -v nix-shell >/dev/null; then
    args=''; if (( $# )); then args=$(printf '%q ' "$@"); fi
    exec env VW_IN_NIX_SHELL=1 nix-shell shell.nix --run "bash run.sh $args"
fi
if [[ -z ${PYTHON:-} ]]; then
    PYTHON=$(command -v python3 || true)
    if [[ -z $PYTHON ]]; then
        for candidate in /nix/store/*-python3-*/bin/python3; do
            if [[ -x $candidate ]]; then PYTHON=$candidate; break; fi
        done
    fi
fi
if [[ -z ${PYTHON:-} ]]; then echo 'Install Python 3 or set PYTHON.' >&2; exit 1; fi
# VW_GAME_DIR: the game's folder (eboot.bin, sce_module, ...).
game=${VW_GAME_DIR:?Set VW_GAME_DIR to the game folder (eboot.bin, sce_module, ...).}
if [[ ! -f $game/eboot.bin ]]; then echo "No eboot.bin in $game (set VW_GAME_DIR)." >&2; exit 1; fi
# Per-game settings (scripts/game_profiles.py, shared with run.py).
eval "$("$PYTHON" scripts/game_profiles.py "$game")"
original_game=$game
game=$("$PYTHON" scripts/mods.py "$game" --out "$out" \
    --mods-dir "${VW_MODS_DIR:-$data/mods}" --config "${VW_MODS_CONFIG:-$data/mods.json}" \
    --enabled "${VW_MODS_ENABLED:-1}")
# A private merged view lasts for this launch, including restarts. Cleanup only our own view.
if [[ $game != "$(realpath "$original_game")" ]]; then
    mod_game=$game
    trap '"$PYTHON" -c '\''import shutil,sys; shutil.rmtree(sys.argv[1])'\'' "$mod_game"' EXIT
fi
"$PYTHON" scripts/prepare.py "$game" --out "$out"
"$PYTHON" scripts/link_libc.py "$game" --out "$out"
"$PYTHON" scripts/link_modules.py "$game" --out "$out"
"$PYTHON" scripts/content_profile.py "$game" --out "$out" --sku "${VW_CONTENT_SKU:-full}"
# VW_PATCHES adds patch names ("a;b") to the game's built-in patch file.
"$PYTHON" scripts/patches.py --out "$out" --extra "${VW_PATCHES:-}" --game-dir "$game"     --patches-dir "${VW_PATCHES_DIR:-$data/patches}" --patches-config "${VW_PATCHES_CONFIG:-$data/patches.json}"
export VW_VBLANK_HZ=${VW_VBLANK_HZ:-60}
if [[ -n ${VW_PREBUILT:-} ]]; then
    probe=${VW_PROBE:-bin/vwport}
else
    bash build.sh
    probe=out/vwport
fi
# GCN 3-5 under RADV: no HiZ (scripts/game_profiles.py radv_nohiz).
if [[ -x ${probe%/*}/vw-gpu-capabilities ]]; then
    eval "$("$PYTHON" scripts/game_profiles.py --nohiz "${probe%/*}/vw-gpu-capabilities")"
fi
probe_args=("$out/boot-linked.bin" --content-profile "$out/content.bin" --patches "$out/patches.bin" --app0 "$game" --user "${VW_USER_DIR:-$data/user}" --timeout "${VW_TIMEOUT:-0}" "$@")
if [[ -n ${mod_game:-} ]]; then
    "$probe" "${probe_args[@]}" &
    mod_pid=$!
    trap 'kill -TERM "$mod_pid" 2>/dev/null || true' TERM INT
    mod_status=0
    wait "$mod_pid" || mod_status=$?
    # An interrupted wait must finish the child before removing its mounted view.
    if kill -0 "$mod_pid" 2>/dev/null; then wait "$mod_pid" || mod_status=$?; fi
    exit "$mod_status"
fi
exec "$probe" "${probe_args[@]}"
