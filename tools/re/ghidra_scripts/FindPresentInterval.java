// Trace NVN present-interval procs: string -> GetProcAddress site -> stored global -> call sites.
// Prints disassembly around each call so the interval argument (w1) can be read.
// @category doax3
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.mem.*;

public class FindPresentInterval extends GhidraScript {
    static final String[] NAMES = {
        "nvnWindowSetPresentInterval", "nvnWindowBuilderSetPresentInterval",
        "nvnWindowGetPresentInterval", "nvnWindowBuilderGetPresentInterval" };

    @Override
    public void run() throws Exception {
        Listing L = currentProgram.getListing();
        ReferenceManager R = currentProgram.getReferenceManager();
        Memory mem = currentProgram.getMemory();
        for (String name : NAMES) {
            byte[] pat = (name + "\0").getBytes("ASCII");
            Address s = mem.findBytes(currentProgram.getMinAddress(), pat, null, true, monitor);
            // make sure we match the whole string, not a suffix of another
            while (s != null && mem.getByte(s.subtract(1)) != 0) {
                s = mem.findBytes(s.add(1), pat, null, true, monitor);
            }
            println("=== " + name + " string @ " + s);
            if (s == null) continue;
            for (Reference ref : R.getReferencesTo(s)) {
                Address from = ref.getFromAddress();
                Function f = getFunctionContaining(from);
                println("  string ref " + from + " in " + (f == null ? "?" : f.getName() + "@" + f.getEntryPoint()));
                // walk forward to the store of the returned pointer
                Instruction ins = L.getInstructionAt(from);
                for (int i = 0; i < 24 && ins != null; i++, ins = ins.getNext()) {
                    String m = ins.getMnemonicString();
                    if (m.startsWith("str") || m.startsWith("stp")) {
                        for (Reference r2 : ins.getReferencesFrom()) {
                            Address g = r2.getToAddress();
                            println("    stored -> " + g + "  (" + ins + ")");
                            dumpUsers(g);
                        }
                        if (ins.getReferencesFrom().length > 0) break;
                    }
                }
            }
        }
    }

    void dumpUsers(Address g) {
        Listing L = currentProgram.getListing();
        for (Reference r : currentProgram.getReferenceManager().getReferencesTo(g)) {
            if (!r.getReferenceType().isRead()) continue;
            Address a = r.getFromAddress();
            Function f = getFunctionContaining(a);
            println("      read @ " + a + " in " + (f == null ? "?" : f.getName() + "@" + f.getEntryPoint()));
            Instruction ins = L.getInstructionAt(a);
            for (int i = 0; i < 6 && ins != null && ins.getPrevious() != null; i++) ins = ins.getPrevious();
            for (int i = 0; i < 16 && ins != null; i++, ins = ins.getNext()) {
                try {
                    byte[] b = ins.getBytes();
                    StringBuilder hx = new StringBuilder();
                    for (byte x : b) hx.append(String.format("%02X", x));
                    println(String.format("        %s  %s  %s", ins.getAddress(), hx, ins));
                } catch (Exception e) { }
            }
        }
    }
}
