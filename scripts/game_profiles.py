#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Per-game launch settings, one place for run.py and run.sh.

`defaults` apply unless the variable is already set; `forced` always apply (they are what the
port was tested with). Render resolution is a code patch in the probe for each game
(src/odin_render_scale.h).

Shell use (run.sh): python3 scripts/game_profiles.py GAME_DIR prints `export` lines;
`--nohiz CAPABILITIES` the RADV workaround for the game's GPU (radv_nohiz).
"""
from pathlib import Path
import shlex
import sys

PROFILES = {
    'CUSA05083': {  # Odin Sphere Leifthrasir
        'defaults': {
            # The draw pipeline (bit 37) presents black/partial frames; early draw inputs and
            # the constant ring (bits 35, 36) flicker.
            'VW_DISABLED_OPTIMIZATIONS': str((1 << 35) | (1 << 36) | (1 << 37)),
            'VW_READBACK_LINEAR': '1',
        },
        'forced': {
            # Logic advances once per frame: the menu disables the engine frame rate option.
            'VW_FIXED_TIMESTEP': '1',
            # Tested without the readback shortcuts.
            'VW_DEFERRED_READBACK': '0',
            'VW_STALE_READBACK': '0',
        },
    },
}


def profile(title_id):
    return PROFILES.get(title_id or '', {})


def apply(title_id, env):
    """Sets the profile's variables in `env` (a dict, e.g. os.environ)."""
    p = profile(title_id)
    for key, value in p.get('defaults', {}).items():
        env.setdefault(key, value)
    env.update(p.get('forced', {}))


# GCN 3-5 under RADV (Polaris, Vega and their APUs): HiZ depth compression shows black halos
# and missing geometry; RADV_DEBUG=nohiz avoids it at some cost (found by bbport, its #54, #99).
NOHIZ_CHIPS = ('TONGA', 'ICELAND', 'CARRIZO', 'FIJI', 'STONEY', 'POLARIS', 'VEGA', 'RAVEN', 'RENOIR')


def radv_nohiz(env, device_name):
    """RADV_DEBUG=nohiz for the game's GPU when it needs it, unless RADV_DEBUG is the user's or
    VW_AUTO_NOHIZ=0. Returns whether it was set."""
    if env.get('VW_AUTO_NOHIZ', '1') == '0' or env.get('RADV_DEBUG'):
        return False
    if any(f'(RADV {chip}' in (device_name or '') for chip in NOHIZ_CHIPS):
        env['RADV_DEBUG'] = 'nohiz'
        return True
    return False


def device_name(capabilities):
    """The game's GPU name from vw-gpu-capabilities --device, or ''."""
    import subprocess
    try:
        out = subprocess.run([str(capabilities), '--device'], capture_output=True, text=True,
                             timeout=20, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0)).stdout
    except (OSError, subprocess.SubprocessError):
        return ''
    return out.strip().partition('\t')[2]


def main():
    if sys.argv[1] == '--nohiz':  # run.sh: python3 game_profiles.py --nohiz CAPABILITIES
        env = {k: v for k, v in __import__('os').environ.items() if k in ('RADV_DEBUG', 'VW_AUTO_NOHIZ')}
        name = device_name(sys.argv[2])
        if radv_nohiz(env, name):
            print(f'export RADV_DEBUG=nohiz; echo {shlex.quote(f"RADV: {name}: RADV_DEBUG=nohiz (VW_AUTO_NOHIZ=0: off)")}')
        return
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from patches import game_title_id
    p = profile(game_title_id(Path(sys.argv[1])))
    for key, value in p.get('defaults', {}).items():
        print(f'export {key}="${{{key}:-{value}}}"')
    for key, value in p.get('forced', {}).items():
        print(f'export {key}={shlex.quote(value)}')


if __name__ == '__main__':
    main()
