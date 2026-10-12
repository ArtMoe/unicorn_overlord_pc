# Odin Sphere Leifthrasir RE workspace

Target: CUSA05083 (US, UP0177), app 01.01, eboot.bin sha256
2865b6cec33f2515def173c1ed0c03afc27a1cd5fad1b0d0f516b4527aba9a88.

Tools: Ghidra 12.1.2 + JDK 21 from `C:\Users\tusha\doax3-mod\toolchain` (shared, not copied).

## Rebuild from scratch

```sh
# 1. port preparation (plain ELF, relocations, linked libc/Fios2) -> port/out_odin/
python port/scripts/prepare.py      game_dump/CUSA05083 --out port/out_odin
python port/scripts/link_libc.py    game_dump/CUSA05083 --out port/out_odin
python port/scripts/link_modules.py game_dump/CUSA05083 --out port/out_odin
# 2. Ghidra-friendly ELF + import table -> re/
python re/tools/make_ghidra_elf.py port port/out_odin re
# 3. import + auto-analysis + import labels (Ghidra project re/ghidra-project, program eboot_ghidra.elf)
sh re/scripts/import.sh
```

`eboot_ghidra.elf` = eboot.elf with e_type ET_EXEC (image base 0, addresses = PS4 vaddrs),
PT_SCE_RELRO turned into PT_LOAD, and all 239,947 relocations applied. Import GOT slots point
to a fake `EXTERN_IMPORTS` block at 0x7f0000000 + 8*index, labelled by `LabelImports.java`.
Unknown NIDs are labelled `<library>_nid_<NID>`.

## Running scripts

`sh re/scripts/gh.sh <Script.java> <out.txt> [args...]` runs read-only on the analyzed
program (same helpers as doax3-mod: Decomp, Xrefs, CallArgs, CallUp, Deref, DumpCallers,
FindImm, Table).

## Unnamed imports (runtime has no implementation yet — vw never calls them)

libkernel 7, libSceNpTus 6, libSceSystemGesture 5, libSceErrorDialog 4, libSceGnmDriver 3,
libc 2, libSceCoredump 2, libSceGameLiveStreaming 2, libSceVideoOut 2, libSceSaveData 1,
libSceCommonDialog 1. See `imports.tsv` (name `?`).
