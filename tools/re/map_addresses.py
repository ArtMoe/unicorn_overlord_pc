#!/usr/bin/env python3
"""map_addresses.py <old.elf> <new.elf> <addr>... : where code and data at old image offsets are
in another build of the same game (Ghidra ELFs from make_ghidra_elf.py, relocations applied).

Samples unique 16-byte runs to learn how far each region moved, predicts each address, then
checks the bytes around the prediction: it prints the new address, the share of the 64 bytes
around it that are equal (pointers and rip-relative displacements may differ) and whether the
32 bytes at the address itself are equal. Addresses below 0x10000 are taken as hex too."""
import bisect
import struct
import sys


def load(path):
    data = open(path, 'rb').read()
    phoff = struct.unpack_from('<Q', data, 0x20)[0]
    count = struct.unpack_from('<H', data, 0x38)[0]
    image = {}
    for i in range(count):
        kind, _, offset, vaddr, _, filesz, _, _ = struct.unpack_from('<IIQQQQQQ', data, phoff + i * 56)
        if kind == 1 and filesz and vaddr < 0x7f0000000:
            image[vaddr] = data[offset:offset + filesz]
    flat = bytearray(max(v + len(b) for v, b in image.items()))
    for vaddr, chunk in image.items():
        flat[vaddr:vaddr + len(chunk)] = chunk
    return bytes(flat)


def anchors(old, new, step=0x8000, run=32):
    """(old, new) pairs of runs found once in the new image near where the last pair predicts,
    sorted, keeping a monotone chain."""
    pairs, delta = [], 0
    for at in range(0, len(old) - run, step):
        for shift in range(0, 0x800, 97):
            piece = old[at + shift:at + shift + run]
            if len(set(piece)) < 8:
                continue
            lo = max(0, at + shift + delta - 0x40000)
            found = new.find(piece, lo, at + shift + delta + 0x40000)
            if found >= 0 and new.find(piece, found + 1, found + 0x40000) < 0:
                pairs.append((at + shift, found))
                delta = found - at - shift
                break
    chain = []
    for o, n in pairs:  # drop pairs that disagree with both neighbours' order
        if chain and n <= chain[-1][1]:
            continue
        chain.append((o, n))
    return chain


def score(old, new, a, b, half=32):
    if a < half or b < half or a + half > len(old) or b + half > len(new):
        return 0.0
    x, y = old[a - half:a + half], new[b - half:b + half]
    return sum(p == q for p, q in zip(x, y)) / len(x)


def locate(old, new, chain, address, radius=0x2000):
    keys = [o for o, _ in chain]
    i = max(0, bisect.bisect_right(keys, address) - 1)
    guesses = {address + chain[j][1] - chain[j][0] for j in (i, min(i + 1, len(chain) - 1))}
    best = (0.0, None)
    for guess in guesses:
        for b in range(max(0, guess - radius), min(len(new), guess + radius)):
            s = score(old, new, address, b)
            if s > best[0]:
                best = (s, b)
    return best


def main():
    old, new = load(sys.argv[1]), load(sys.argv[2])
    chain = anchors(old, new)
    print(f'# {len(chain)} anchors')
    for arg in sys.argv[3:]:
        address = int(arg, 16)
        s, found = locate(old, new, chain, address)
        exact = found is not None and old[address:address + 32] == new[found:found + 32]
        print(f'{address:#x} -> {found:#x}  match {s:.2f}  exact32 {"yes" if exact else "no"}'
              if found is not None else f'{address:#x} -> ?')


if __name__ == '__main__':
    main()
