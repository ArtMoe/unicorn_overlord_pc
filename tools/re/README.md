# Reverse-engineering workspace (Ghidra)

Tools for reading Unicorn Overlord's executable (CUSA36381 v01.04) in Ghidra. Ghidra (12.x) and a
JDK 21 are not included. Nothing here is needed to build or play.

## Set up

```sh
# 1. Prepare the game image as the port does (any start of the game also writes <data>/out/).
python scripts/prepare.py      <dump> --out <re>/out
python scripts/link_libc.py    <dump> --out <re>/out
python scripts/link_modules.py <dump> --out <re>/out
# 2. A Ghidra-friendly ELF and the import table.
python tools/re/make_ghidra_elf.py . <re>/out <re>
# 3. Import and analyse (about 15 minutes; give it 8-12 GB of heap).
GHIDRA_HEADLESS_MAXMEM=10G <ghidra>/support/analyzeHeadless <re>/ghidra-project uo \
    -import <re>/eboot_ghidra.elf -scriptPath tools/re/ghidra_scripts \
    -postScript LabelImports.java <re>/imports.tsv
```

`eboot_ghidra.elf` is the prepared eboot with e_type ET_EXEC (image base 0: addresses are the
PS4 virtual addresses, which are also the offsets the probe patches) and every relative relocation
applied. Import slots point to a labelled `EXTERN_IMPORTS` block at 0x7f0000000 + 8 × index;
unknown NIDs are labelled `<library>_nid_<NID>`. `extra_names.tsv` adds names that are not in
`src/import_names.inc`.

## Running scripts

```sh
<ghidra>/support/analyzeHeadless <re>/ghidra-project uo -process eboot_ghidra.elf -noanalysis \
    -readOnly -scriptPath tools/re/ghidra_scripts -postScript Decomp.java 0x1fdfc0
```

Scripts in `ghidra_scripts/`: Decomp (pseudo-C and disassembly of a function), DecompRange (every
function in an address range), Xrefs, CallArgs, CallUp, Deref, DumpCallers, FindImm, Table,
StrRefs (defined strings containing a text, with the functions that use them).
