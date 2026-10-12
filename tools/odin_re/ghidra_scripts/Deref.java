// Usage: -postScript Deref.java <hexaddr>... -- read the 8-byte pointer at each address and print xrefs of its target
// @category doax3
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;

public class Deref extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (String arg : getScriptArgs()) {
            Address p = toAddr(Long.parseUnsignedLong(arg.replace("0x", ""), 16));
            Address t = toAddr(getLong(p));
            String v = "?";
            try { v = String.format("0x%08X", getInt(t)); } catch (Exception e) { }
            println("##### " + p + " -> " + t + " init(u32)=" + v + " block=" + currentProgram.getMemory().getBlock(t).getName());
            for (Reference r : getReferencesTo(t)) {
                Function f = getFunctionContaining(r.getFromAddress());
                println("  " + r.getReferenceType() + " @ " + r.getFromAddress() + " in " + (f == null ? "?" : f.getName() + "@" + f.getEntryPoint()));
            }
            int n = 0;
            for (Reference r : getReferencesTo(p)) n++;
            println("  (GOT slot referenced " + n + " times)");
        }
    }
}
