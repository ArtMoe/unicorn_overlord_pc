// Usage: -postScript Xrefs.java <hexaddr>...  -- value (4 bytes) + every reference with context
// @category doax3
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;

public class Xrefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (String arg : getScriptArgs()) {
            Address t = toAddr(Long.parseUnsignedLong(arg.replace("0x", ""), 16));
            String v = "?";
            try { v = String.format("0x%08X", getInt(t)); } catch (Exception e) { }
            println("##### " + t + " value(le u32)=" + v + " block=" + currentProgram.getMemory().getBlock(t).getName());
            for (Reference r : getReferencesTo(t)) {
                Address a = r.getFromAddress();
                Function f = getFunctionContaining(a);
                println("--- " + r.getReferenceType() + " @ " + a + " in " + (f == null ? "?" : f.getName() + "@" + f.getEntryPoint()));
                Instruction p = getInstructionAt(a);
                if (p == null) continue;
                for (int i = 0; i < 6 && p.getPrevious() != null; i++) p = p.getPrevious();
                for (int i = 0; i < 12 && p != null; i++, p = p.getNext()) {
                    StringBuilder hx = new StringBuilder();
                    for (byte x : p.getBytes()) hx.append(String.format("%02X", x));
                    println("    " + p.getAddress() + "  " + hx + "  " + p);
                }
            }
        }
    }
}
