// Usage: -postScript DecompRange.java <start hex> <end hex>  -- decompile every function starting in [start, end)
// @category vw
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;

public class DecompRange extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        long start = Long.parseUnsignedLong(a[0].replace("0x", ""), 16), end = Long.parseUnsignedLong(a[1].replace("0x", ""), 16);
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(toAddr(start), true);
        while (it.hasNext()) {
            Function f = it.next();
            if (f.getEntryPoint().getOffset() >= end) break;
            DecompileResults res = di.decompileFunction(f, 60, monitor);
            println("##### " + f.getName() + " @ " + f.getEntryPoint());
            if (res.decompileCompleted()) println(res.getDecompiledFunction().getC());
        }
    }
}
