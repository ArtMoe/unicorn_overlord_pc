// Usage: -postScript CallArgs.java <target> [lo hi] -- every call to target, with the 3 preceding instructions (bytes)
// @category doax3
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;

public class CallArgs extends GhidraScript {
    long hx(String s) { return Long.parseUnsignedLong(s.replace("0x", ""), 16); }
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        Address t = toAddr(hx(a[0]));
        long lo = a.length > 2 ? hx(a[1]) : 0, hi = a.length > 2 ? hx(a[2]) : -1L;
        for (Reference r : getReferencesTo(t)) {
            if (!r.getReferenceType().isCall()) continue;
            long off = r.getFromAddress().getOffset();
            if (Long.compareUnsigned(off, lo) < 0 || Long.compareUnsigned(off, hi) > 0) continue;
            Function f = getFunctionContaining(r.getFromAddress());
            StringBuilder sb = new StringBuilder(r.getFromAddress() + " " + (f == null ? "?" : f.getName()) + " :");
            Instruction p = getInstructionAt(r.getFromAddress());
            for (int i = 0; i < 3 && p.getPrevious() != null; i++) p = p.getPrevious();
            for (int i = 0; i < 4; i++, p = p.getNext()) {
                StringBuilder b = new StringBuilder();
                for (byte x : p.getBytes()) b.append(String.format("%02X", x));
                sb.append(" | " + p.getAddress().toString().substring(6) + " " + b + " " + p);
            }
            println(sb.toString());
        }
    }
}
