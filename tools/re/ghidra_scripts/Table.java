// Usage: -postScript Table.java <hexaddr> <count> -- dump 8-byte pointers and their function names
// @category doax3
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class Table extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        Address p = toAddr(Long.parseUnsignedLong(a[0].replace("0x", ""), 16));
        int n = Integer.parseInt(a[1]);
        for (int i = 0; i < n; i++) {
            Address e = p.add(i * 8L);
            long v = getLong(e);
            Function f = v == 0 ? null : getFunctionAt(toAddr(v));
            println(String.format("%s [%2d] %016X %s", e, i, v, f == null ? "" : f.getName()));
        }
    }
}
