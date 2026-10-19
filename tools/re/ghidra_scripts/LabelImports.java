// Usage: -postScript LabelImports.java <imports.tsv>
// Creates the EXTERN block that make_ghidra_elf.py pointed the GOT at, labels each 8-byte
// external slot and GOT slot with the import's name (or nid_<NID> when unknown), types GOT
// slots as pointers, and names the jmp [GOT] thunks after the import.
// @category vw
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.PointerDataType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.SourceType;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

public class LabelImports extends GhidraScript {
    private static String clean(String s) { return s.replaceAll("[^A-Za-z0-9_.:@$]", "_"); }

    @Override
    public void run() throws Exception {
        List<String> lines = Files.readAllLines(Path.of(getScriptArgs()[0]));
        long base = -1, count = 0;
        for (String l : lines) {
            if (l.startsWith("#")) continue;
            String[] f = l.split("\t", -1);
            long ext = Long.decode(f[1]);
            if (base < 0) base = ext;
            count++;
        }
        Address extBase = toAddr(base);
        if (getMemoryBlock(extBase) == null) {
            MemoryBlock b = currentProgram.getMemory().createUninitializedBlock(
                "EXTERN_IMPORTS", extBase, count * 8, false);
            b.setRead(true); b.setExecute(true);
        }
        int named = 0, thunks = 0;
        for (String l : lines) {
            if (l.startsWith("#")) continue;
            String[] f = l.split("\t", -1);
            String nid = f[3].split("#")[0];
            String lib = f.length > 5 ? f[5] : "?";
            String name = f[4].equals("?") ? clean(lib) + "_nid_" + clean(nid) : clean(f[4]);
            Address ext = toAddr(Long.decode(f[1]));
            createLabel(ext, name, true, SourceType.IMPORTED);
            setEOLComment(ext, f[3]);
            if (!f[4].equals("?")) named++;
            if (f[2].isEmpty()) continue;
            for (String g : f[2].split(",")) {
                Address got = toAddr(Long.decode(g));
                createLabel(got, "PTR_" + name, true, SourceType.IMPORTED);
                try { clearListing(got, got.add(7)); createData(got, PointerDataType.dataType); }
                catch (Exception e) { }
                for (Reference r : getReferencesTo(got)) {
                    Function fn = getFunctionContaining(r.getFromAddress());
                    if (fn != null && fn.getBody().getNumAddresses() <= 16) {
                        fn.setName(name, SourceType.IMPORTED);
                        thunks++;
                    }
                }
            }
        }
        println("imports " + count + ", named " + named + ", thunks named " + thunks);
    }
}
