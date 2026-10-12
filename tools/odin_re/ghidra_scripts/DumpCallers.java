// Usage: -postScript DumpCallers.java <hexaddr> [<hexaddr>...]
// For each target function: list call sites with the 12 preceding instructions (with bytes),
// and print the decompilation of each calling function.
// @category doax3
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.util.*;

public class DumpCallers extends GhidraScript {
    @Override
    public void run() throws Exception {
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        Set<Function> done = new HashSet<>();
        for (String arg : getScriptArgs()) {
            Address t = toAddr(Long.parseUnsignedLong(arg.replace("0x", ""), 16));
            println("##### target " + t);
            for (Reference r : getReferencesTo(t)) {
                Address a = r.getFromAddress();
                Function f = getFunctionContaining(a);
                println("--- call @ " + a + " (" + r.getReferenceType() + ") in " + (f == null ? "?" : f.getName() + "@" + f.getEntryPoint()));
                Instruction ins = getInstructionAt(a);
                for (int i = 0; i < 12 && ins != null && ins.getPrevious() != null; i++) ins = ins.getPrevious();
                for (int i = 0; i < 14 && ins != null; i++, ins = ins.getNext()) println("    " + fmt(ins));
                if (f != null && done.add(f)) {
                    DecompileResults res = di.decompileFunction(f, 120, monitor);
                    if (res.decompileCompleted()) println(res.getDecompiledFunction().getC());
                }
            }
            Function tf = getFunctionAt(t);
            if (tf != null && done.add(tf)) {
                println("--- target body " + tf.getName());
                DecompileResults res = di.decompileFunction(tf, 120, monitor);
                if (res.decompileCompleted()) println(res.getDecompiledFunction().getC());
            }
        }
    }

    String fmt(Instruction ins) {
        StringBuilder hx = new StringBuilder();
        try { for (byte x : ins.getBytes()) hx.append(String.format("%02X", x)); } catch (Exception e) { }
        return ins.getAddress() + "  " + hx + "  " + ins;
    }
}
