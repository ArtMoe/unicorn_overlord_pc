// Usage: -postScript FindImm.java <hex imm> [mnemonicPrefix]
// Lists every instruction with the given scalar operand (e.g. a struct field offset),
// optionally filtered by mnemonic prefix (e.g. "str"), with 8 preceding instructions.
// @category doax3
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;

public class FindImm extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        long imm = Long.parseUnsignedLong(a[0].replace("0x", ""), 16);
        String pre = a.length > 1 ? a[1] : "";
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext() && !monitor.isCancelled()) {
            Instruction ins = it.next();
            if (!ins.getMnemonicString().startsWith(pre)) continue;
            boolean hit = false;
            for (int i = 0; i < ins.getNumOperands() && !hit; i++)
                for (Object o : ins.getOpObjects(i))
                    if (o instanceof Scalar && ((Scalar) o).getUnsignedValue() == imm) hit = true;
            if (!hit) continue;
            Function f = getFunctionContaining(ins.getAddress());
            println("--- " + ins.getAddress() + " in " + (f == null ? "?" : f.getName() + "@" + f.getEntryPoint()));
            Instruction p = ins;
            for (int i = 0; i < 8 && p.getPrevious() != null; i++) p = p.getPrevious();
            for (int i = 0; i < 10 && p != null; i++, p = p.getNext()) println("    " + fmt(p));
        }
    }

    String fmt(Instruction ins) {
        StringBuilder hx = new StringBuilder();
        try { for (byte x : ins.getBytes()) hx.append(String.format("%02X", x)); } catch (Exception e) { }
        return ins.getAddress() + "  " + hx + "  " + ins;
    }
}
