// Usage: -postScript Decomp.java <hexaddr>...  -- decompile + disassemble (with bytes) each function
// @category doax3
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;

public class Decomp extends GhidraScript {
    @Override
    public void run() throws Exception {
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (String arg : getScriptArgs()) {
            Function f = getFunctionContaining(toAddr(Long.parseUnsignedLong(arg.replace("0x", ""), 16)));
            if (f == null) { println("no function at " + arg); continue; }
            println("##### " + f.getName() + " @ " + f.getEntryPoint());
            DecompileResults res = di.decompileFunction(f, 120, monitor);
            if (res.decompileCompleted()) println(res.getDecompiledFunction().getC());
            println("--- asm");
            for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true)) {
                StringBuilder hx = new StringBuilder();
                for (byte x : ins.getBytes()) hx.append(String.format("%02X", x));
                println("    " + ins.getAddress() + "  " + hx + "  " + ins);
            }
        }
    }
}
