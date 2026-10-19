// Usage: -postScript StrRefs.java <substring>...  -- defined strings containing a substring, with the functions referencing them
// @category vw
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;

public class StrRefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (String needle : getScriptArgs()) {
            DataIterator it = currentProgram.getListing().getDefinedData(true);
            while (it.hasNext()) {
                Data d = it.next();
                Object v = d.getValue();
                if (!(v instanceof String) || !((String) v).contains(needle)) continue;
                println("STR " + d.getAddress() + " \"" + v + "\"");
                for (Reference r : getReferencesTo(d.getAddress())) {
                    Function f = getFunctionContaining(r.getFromAddress());
                    println("   ref " + r.getFromAddress() + " in " + (f == null ? "?" : f.getName() + "@" + f.getEntryPoint()));
                }
            }
        }
    }
}
