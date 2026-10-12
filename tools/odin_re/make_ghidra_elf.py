#!/usr/bin/env python3
"""Turn the port's prepared eboot (out_odin/) into something Ghidra's stock ELF loader reads well.

- eboot_ghidra.elf: eboot.elf with e_type ET_EXEC (image base 0 = PS4 vaddrs) and every
  relative relocation applied, so vtables / function tables / string pointers are real.
  Import GOT slots get a fake address EXTERN_BASE + 8*index, so calls through them resolve
  to a labelled external stub area created by LabelImports.java.
- imports.tsv: index, GOT slot, NID, name (from the runtime's import_names.inc and
  prepare.py's hints; '?' when unknown).

usage: make_ghidra_elf.py <port dir> <prepared out dir> <re dir>
"""
import json
import re
import struct
import sys
from pathlib import Path

EXTERN_BASE = 0x7f0000000


def main():
    port, prepared, re_dir = map(Path, sys.argv[1:4])
    elf = bytearray((prepared / 'eboot.elf').read_bytes())
    boot = (prepared / 'boot.bin').read_bytes()
    analysis = json.loads((prepared / 'analysis.json').read_text(encoding='utf-8'))

    magic, _, _, nloads, nrelocs, nnames, _ = struct.unpack_from('<8sQQQQQQ', boot, 0)
    if magic != b'G3PROBE2':
        raise SystemExit('unexpected boot.bin format')
    pos = 56 + 24 * nloads
    names = [boot[pos + 128 * i:pos + 128 * (i + 1)].rstrip(b'\0').decode() for i in range(nnames)]
    pos += 128 * nnames
    relocs = [struct.unpack_from('<QQqq', boot, pos + 32 * i) for i in range(nrelocs)]

    # vaddr -> file offset through the ELF's PT_LOAD headers.
    phoff, = struct.unpack_from('<Q', elf, 32)
    phnum, = struct.unpack_from('<H', elf, 56)
    loads = []
    for i in range(phnum):
        ptype, _, off, vaddr, _, filesz, _, _ = struct.unpack_from('<IIQQQQQQ', elf, phoff + 56 * i)
        if ptype == 0x61000010:  # PT_SCE_RELRO (GOT, vtables): make it a plain PT_LOAD
            struct.pack_into('<I', elf, phoff + 56 * i, 1)
            ptype = 1
        if ptype == 1:
            loads.append((vaddr, filesz, off))

    def file_offset(vaddr):
        for start, size, off in loads:
            if start <= vaddr and vaddr + 8 <= start + size:
                return off + vaddr - start
        return None

    table = {}
    inc = (port / 'src' / 'import_names.inc').read_text(encoding='utf-8')
    for nid, name in re.findall(r'\{"([A-Za-z0-9+\-]{11})#[^"]*"\s*,\s*"([^"]+)"', inc):
        table[nid] = name
    for key, name in analysis.get('import_name_hints', {}).items():
        table[key.split('#')[0]] = name
    extra = re_dir / 'extra_names.tsv'
    if extra.exists():
        for line in extra.read_text(encoding='utf-8').splitlines():
            if line and not line.startswith('#'):
                nid, name = line.split('\t')[:2]
                table[nid] = name

    applied = skipped = 0
    slots = {}
    for target, kind, sym, addend in relocs:
        off = file_offset(target)
        if off is None:          # .bss: nothing in the file to write
            skipped += 1
            continue
        if kind == 0:            # relative: base (0) + addend
            value = addend
        else:                    # import (1 = code, 2 = data): fake external address
            value = EXTERN_BASE + 8 * sym + addend
            slots.setdefault(sym, []).append(target)
        struct.pack_into('<Q', elf, off, value & 0xffffffffffffffff)
        applied += 1
    struct.pack_into('<H', elf, 16, 2)  # ET_EXEC

    # Library of each import: the '#L#M' suffix encodes the import-library id in the
    # dynamic table's alphabet; DT_SCE_IMPORT_LIB entries carry id << 48 | name offset.
    dynamic = next(p for p in (struct.unpack_from('<IIQQQQQQ', elf, phoff + 56 * i)
                               for i in range(phnum)) if p[0] == 2)
    dynlib = next(p for p in (struct.unpack_from('<IIQQQQQQ', elf, phoff + 56 * i)
                              for i in range(phnum)) if p[0] == 0x61000000)
    tags = [struct.unpack_from('<QQ', elf, dynamic[2] + o) for o in range(0, dynamic[5], 16)]
    first = {}
    for tag, value in tags:
        first.setdefault(tag, value)
    blob = elf[dynlib[2]:dynlib[2] + dynlib[5]]
    strtab = blob[first[0x61000035]:first[0x61000035] + first[0x61000037]]
    alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-'

    def encode(n):
        out = ''
        while True:
            out = alphabet[n % 64] + out
            n //= 64
            if not n:
                return out
    libraries = {encode(v >> 48): strtab[v & 0xffffffff:strtab.index(0, v & 0xffffffff)].decode()
                 for t, v in tags if t in (0x61000015, 0x61000016)}

    re_dir.mkdir(parents=True, exist_ok=True)
    (re_dir / 'eboot_ghidra.elf').write_bytes(elf)
    unknown = 0
    with (re_dir / 'imports.tsv').open('w', encoding='utf-8', newline='\n') as f:
        f.write('# index\textern\tgot_slots\tnid\tname\tlibrary\n')
        for i, full in enumerate(names):
            name = table.get(full.split('#')[0], '?')
            unknown += name == '?'
            got = ','.join(hex(t) for t in slots.get(i, []))
            parts = full.split('#')
            library = libraries.get(parts[1], '?') if len(parts) > 1 else '?'
            f.write(f'{i}\t{EXTERN_BASE + 8 * i:#x}\t{got}\t{full}\t{name}\t{library}\n')
    print(f'relocations applied {applied}, outside file {skipped}; imports {len(names)}, unnamed {unknown}')


if __name__ == '__main__':
    main()
