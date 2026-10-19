# Native settings rows: what the game's Options screen is made of (2026-10-19)

Goal: the port's settings (render resolution, display mode, VSync, FPS limit, sharpening, GPU
readbacks, button icons) as rows in the game's own System → Options screen instead of the
Insert overlay. This is the research for it; nothing here is implemented yet.

All addresses are CUSA36381 v01.04 eboot offsets (the Ghidra image from
`tools/odin_re/make_ghidra_elf.py`).

## Findings

- The screen is native C++, class `UcUI_Option` (vtable users in `0x7309c0`–`0x73cd00`), not one
  of the game's AngelScript menus. `FUN_00735490` builds it.
- Rows come from a static table at `0xd7c9f0`: 21 rows of 0x14 bytes,
  `{float default, int label_id, int help_id, int type, int flag}`. Types: 0 section header,
  1 and 2 choices, 3 slider (the volumes default to 100.0).
- Two order tables pick and order the rows: `0xd7c940` (21 rows) and `0xd7c9a0` (20 rows, without
  row 1 "Difficulty"); the global at `0x234eee8` selects one. The builder's loop bounds are the
  immediates 0x15 and 0x14.
- Labels and help texts are ids in `MSGSHEET/UcOption.FMS` (a CPK file, CRILAYLA-compressed in
  `UnicornPatch.CPK`). Ids 67, 75, 80 and 86 are the section headers System, Sound, Field and
  Stage; 101–113 are value texts (difficulties, voice languages, slow/normal/fast, OFF,
  `%.0f`/`%.1f` formats).
- `GUI/Interface.mbs` provides the row graphics; each row object is a 0x38e0-byte allocation made
  in the builder.

## How rows could be added

1. **Text.** Every language's `UcOption.FMS` passes the CRI decompressor, where the port already
   hooks (`src/probe.c`, `unicorn_cri_decompress_hook`; the button icons use it). Append a "PC"
   header and row labels there. The file's decompressed size is fixed by the CPK, so new strings
   must fit in the space of strings the PS4 build does not show (the Switch/Xbox-only help
   texts), or the FMS string table has to be rebuilt within the same size.
2. **Rows.** Copy both order tables and the row table into port memory with the extra rows,
   repoint the `lea` instructions that load them, and raise the loop bounds. Every function
   that walks the rows needs the same change; listing them (xrefs to the three tables) is the
   first task.
3. **State.** Find where `UcUI_Option` keeps per-row values (sized for 21 rows) and where it
   applies and saves them. PC rows must neither go to the game's system save nor be
   applied by the game: a hook on the value-change path routes rows with ids ≥ 21 to
   `VwSettings` (vw.ini) and leaves the game's rows alone.
4. **Restart rows** (render resolution, readbacks) reuse the overlay's "Apply and restart"
   (`restart_unconfirmed`, run.py reverts a launch that never shows frames).

Estimate: a few days of reverse engineering, mostly steps 2 and 3. A cheaper first step is a
single "PC settings" row that opens the existing overlay menu; it still needs steps 1 and 2.

## Related

- Button icon layouts (Xbox, Nintendo) are done the same way: `src/unicorn_button_icons.h`.
- The Insert overlay stays as the fallback until native rows work.
