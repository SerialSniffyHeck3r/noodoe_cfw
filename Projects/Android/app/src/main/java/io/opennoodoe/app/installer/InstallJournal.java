package io.opennoodoe.app.installer;

import java.io.*;
import java.util.Properties;

/** Durable intent before each mutating phase. An unknown result is never retried on reconnect. */
public final class InstallJournal {
    public final Properties values = new Properties();
    private final File file;
    private final io.opennoodoe.app.diagnostics.SessionLog log;
    private String loggedState="";
    public InstallJournal(File file) throws IOException {
        this(file,null);
    }
    public InstallJournal(File file,io.opennoodoe.app.diagnostics.SessionLog log) throws IOException {
        this.file = file;
        this.log = log;
        File selected=file.isFile()?file:new File(file+".previous");
        if(selected.isFile()){
            try(InputStream in=new FileInputStream(selected)){values.load(in);}
            String expected=values.getProperty("journal.sha256");
            if(expected==null||!expected.equals(checksum(values)))throw new IOException("Journal is legacy or damaged; preserve it and query the device before recovery");
        }
    }
    private static String checksum(Properties p){
        StringBuilder canonical=new StringBuilder();for(String key:new java.util.TreeSet<>(p.stringPropertyNames()))if(!key.equals("journal.sha256")){
            String value=p.getProperty(key);canonical.append(key.length()).append(':').append(key).append(value.length()).append(':').append(value);
        }
        return RecoveryBundle.sha(canonical.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8));
    }
    public void bind(String address, String bundle) throws IOException {
        String oldAddress = values.getProperty("address", address);
        String oldBundle = values.getProperty("bundle", bundle);
        if (!oldAddress.equalsIgnoreCase(address) || !oldBundle.equals(bundle))
            throw new IOException("Journal belongs to another device or bundle; preserve it before starting another transaction");
        values.setProperty("address", address); values.setProperty("bundle", bundle);
        save(values.getProperty("state", "IMPORTED"));
    }
    File evidenceRoot(){return file.getAbsoluteFile().getParentFile();}
    public String state() { return values.getProperty("state", "EMPTY"); }
    /** A completed stock return ends one attempt, not the device's lifetime.
     * Called only after a fresh stock/peer/version/stationary preflight. Keep
     * immutable evidence before replacing per-attempt transfer state. An
     * uncertain mutation is never eligible for this transition. */
    void restartAfterStockReturn() throws IOException {
        require("STOCK_RETURN_CONFIRMED");
        Properties completed = new Properties(); completed.putAll(values);
        File archived = preserveSnapshot("attempt-history", "Completed stock return; preserved before a new attempt");
        // UID and captured factory/BL evidence still bind this same device.
        // Offset, transaction, provisioned-file and return-mode data do not.
        values.clear();
        for (String key : completed.stringPropertyNames())
            if (key.equals("address") || key.equals("bundle") || key.equals("uid") || key.startsWith("resident."))
                values.setProperty(key, completed.getProperty(key));
        values.setProperty("previous.attempt", archived.getCanonicalPath());
        values.setProperty("previous.attempt.sha256", completed.getProperty("journal.sha256"));
        values.setProperty("attempt.id", java.util.UUID.randomUUID().toString());
        save("STOCK_IDENTIFIED");
    }
    /** Preserve the exact unresolved record before applying newly observed
     * device evidence. A failure cannot authorize the next remote operation. */
    File preserveSnapshot(String category,String description) throws IOException {
        File history = new File(evidenceRoot(), category);
        if (!history.isDirectory()) {
            if (!history.mkdirs()) throw new IOException("Cannot preserve completed installation journal");
            io.opennoodoe.app.diagnostics.DurableFiles.directory(evidenceRoot());
        }
        File archived = new File(history, java.util.UUID.randomUUID()+".journal");
        Properties completed = new Properties(); completed.putAll(values);
        completed.setProperty("journal.sha256", checksum(completed));
        try (FileOutputStream out = new FileOutputStream(archived)) {
            completed.store(out, description);
            out.getFD().sync();
        }
        if (!completed.equals(new InstallJournal(archived).values))
            throw new IOException("Completed installation journal readback mismatch");
        io.opennoodoe.app.diagnostics.DurableFiles.directory(history);
        return archived;
    }
    void recordCommitDiagnostic(long[] words)throws IOException {
        if(log==null)return;
        StringBuilder tuple=new StringBuilder();
        for(long word:words){if(tuple.length()>0)tuple.append(',');tuple.append(word);}
        log.append("commit_diagnostic",io.opennoodoe.app.diagnostics.SessionLog.fields(
            "action","bootstrap_commit_schema1","opcode","133","reason",tuple.toString(),
            "transaction",values.getProperty("transaction","0")));
    }
    public void require(String... states) throws IOException {
        for (String state : states) if (state.equals(state())) return;
        throw new StateBlocked(state());
    }
    public static final class StateBlocked extends IOException {
        public final String state;
        StateBlocked(String state) {
            super("Action blocked in journal state " + state); this.state=state;
        }
    }
    public void save(String state) throws IOException {
        values.setProperty("state", state); values.setProperty("updated_unix_ms", Long.toString(System.currentTimeMillis()));
        File parent = file.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) throw new IOException("Journal directory unavailable");
        values.setProperty("journal.sha256",checksum(values));
        File tmp = new File(file + ".tmp"), previous = new File(file + ".previous");
        try (FileOutputStream out = new FileOutputStream(tmp)) {
            values.store(out, "Noodoe installer; accepted transfer is not verified boot"); out.getFD().sync();
        }
        if (file.exists()) {
            if (previous.exists() && !previous.delete()) throw new IOException("Cannot rotate prior journal");
            if (!file.renameTo(previous)) throw new IOException("Cannot preserve prior journal");
        }
        if (!tmp.renameTo(file)) throw new IOException("Cannot activate journal; prior copy preserved");
        io.opennoodoe.app.diagnostics.DurableFiles.directory(file.getAbsoluteFile().getParentFile());
        // The command caller cannot proceed if either durable intent copy fails.
        if(log!=null&&(!state.equals(loggedState)||state.endsWith("SECTOR_VERIFIED"))){
            java.util.Map<String,String> fields=new java.util.TreeMap<>();fields.put("state",state);
            for(String key:new String[]{"uid","bundle","transaction","offset","version"})
                if(values.containsKey(key))fields.put(key.equals("version")?"firmware":key,values.getProperty(key));
            log.append("transaction_state",fields);loggedState=state;
        }
    }
}
