// Usage: -postScript CallUp.java <depth> <hexaddr>... -- caller tree (BFS) upward from each function
// @category doax3
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import java.util.*;

public class CallUp extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        int depth = Integer.parseInt(a[0]);
        for (int k = 1; k < a.length; k++) {
            Function f = getFunctionContaining(toAddr(Long.parseUnsignedLong(a[k].replace("0x", ""), 16)));
            println("##### " + f.getName());
            walk(f, 0, depth, new HashSet<>());
        }
    }
    void walk(Function f, int d, int max, Set<Function> seen) {
        if (d > max || !seen.add(f)) return;
        Set<Function> callers = f.getCallingFunctions(monitor);
        int dataRefs = 0;
        for (var r : getReferencesTo(f.getEntryPoint())) if (!r.getReferenceType().isCall()) dataRefs++;
        println("  ".repeat(d) + f.getName() + (dataRefs > 0 ? "  [data refs " + dataRefs + "]" : ""));
        for (Function c : callers) walk(c, d + 1, max, seen);
    }
}
