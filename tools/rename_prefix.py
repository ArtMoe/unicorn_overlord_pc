#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""One-off codemod: the God of War III port's gow3 prefix becomes the shared vw prefix.

Renames tracked files (git mv) and rewrites their contents: GOW3 -> VW, Gow3 -> Vw,
gow3 -> vw, with the executable gow3-probe -> vwport. Third-party code, the upstream
project name gow3_pc and files listed in SKIP keep their text. Run from the repository root;
--check prints what would change.
"""
import re
import subprocess
import sys
from pathlib import Path

SKIP_DIRS = ('gpu/third_party/', 'third_party/')
SKIP = {'README.md', 'README.pt-BR.md', 'packaging/windows/README.md', 'tools/rename_prefix.py'}
KEEP = ('gow3_pc',)
RULES = [('gow3-probe', 'vwport'), ('GOW3', 'VW'), ('Gow3', 'Vw'), ('gow3', 'vw')]


def rename_text(text):
    for i, keep in enumerate(KEEP):
        text = text.replace(keep, f'\0KEEP{i}\0')
    for old, new in RULES:
        text = text.replace(old, new)
    for i, keep in enumerate(KEEP):
        text = text.replace(f'\0KEEP{i}\0', keep)
    return text


def tracked():
    out = subprocess.run(['git', 'ls-files', '-z'], capture_output=True, check=True).stdout
    return [p for p in out.decode().split('\0') if p and not p.startswith(SKIP_DIRS)]


def main():
    check = '--check' in sys.argv
    changed = 0
    for path in tracked():
        if path in SKIP or not Path(path).is_file():
            continue
        data = Path(path).read_bytes()
        if b'\0' in data:
            continue  # binary
        text = data.decode('utf-8')
        new = rename_text(text)
        if new != text:
            changed += 1
            if not check:
                Path(path).write_text(new, encoding='utf-8')
    moves = [(p, rename_text(p)) for p in tracked() if p not in SKIP and rename_text(p) != p]
    for old, new in moves:
        print(f'{old} -> {new}')
        if not check:
            subprocess.run(['git', 'mv', old, new], check=True)
    print(f'{changed} files rewritten, {len(moves)} renamed')


if __name__ == '__main__':
    main()
