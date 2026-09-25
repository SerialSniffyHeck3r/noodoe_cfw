package io.opennoodoe.app.diagnostics;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.zip.CRC32;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

/** Payload-free, synchronous write-ahead evidence. A failed append prevents the
 * caller from issuing a command. Backups and transaction journals live outside
 * this bounded event store and are never removed by rotation. */
public final class SessionLog implements Closeable {
    public static final long DEFAULT_BUDGET = 64L * 1024 * 1024;
    private static final int SEGMENT_BYTES = 1024 * 1024;
    private static final Set<String> KEYS = new HashSet<>(Arrays.asList(
        "action", "device", "uid", "bundle", "package_hash", "bl", "gate", "firmware",
        "epoch", "opcode", "sequence", "transaction", "offset", "length", "result",
        "state", "error_class", "boot_id", "failed_version", "restored_version", "reason",
        "tx_bytes","rx_bytes","elapsed_ms","user_wait_ms","active_ms",
        // Aggregate counters/status only: never coordinates, titles or contact data.
        "callbacks","registrations","fix_age_ms","callback_age_ms","screen",
        "location_power_mode","background_permission","media_events","track_changes","art_requests"));
    private final File root, directory;
    private final long budget;
    private final String session;
    private int segment;
    private long event;
    private boolean closed;

    public SessionLog(File root) throws IOException { this(root, DEFAULT_BUDGET); }
    public SessionLog(File root, long budget) throws IOException {
        if (budget < 4096) throw new IllegalArgumentException("Log budget");
        this.root = root; this.budget = budget;
        session = System.currentTimeMillis() + "-" + UUID.randomUUID();
        directory = new File(root, session);
        if (!directory.mkdirs()) throw new IOException("Cannot create durable log session");
        // An interrupted process is a failed/uncertain session until explicitly completed.
        marker(new File(directory, "unfinished"));
        DurableFiles.directory(root);
        if(root.getParentFile()!=null)DurableFiles.directory(root.getParentFile());
        append("session_open", Collections.emptyMap());
    }
    public String id() { return session; }
    public synchronized void reserveArtifact(long bytes)throws IOException {
        if(closed||bytes<0||bytes>DEFAULT_BUDGET)throw new IOException("Invalid diagnostic artifact size");reserve(bytes+4096);
    }
    public static Map<String,String> fields(String... pairs) {
        if ((pairs.length & 1) != 0) throw new IllegalArgumentException("Field pairs");
        Map<String,String> result = new TreeMap<>();
        for (int i=0;i<pairs.length;i+=2) result.put(pairs[i],pairs[i+1]);
        return result;
    }
    /** Schema allowlist deliberately excludes free-form text and exception messages. */
    public synchronized void append(String kind, Map<String,String> fields) throws IOException {
        if (closed) throw new IOException("Log closed");
        if (!kind.matches("[a-z_]{1,48}")) throw new IllegalArgumentException("Event kind");
        StringBuilder json = new StringBuilder("{\"schema\":1,\"session\":\"").append(session)
            .append("\",\"event\":").append(++event).append(",\"unix_ms\":").append(System.currentTimeMillis())
            .append(",\"monotonic_ns\":").append(System.nanoTime()).append(",\"kind\":\"").append(kind).append('"');
        for (Map.Entry<String,String> entry : new TreeMap<>(fields).entrySet()) {
            if (!KEYS.contains(entry.getKey())) throw new IllegalArgumentException("Private/unrecognized log field");
            String value = entry.getValue();
            if (value == null || value.length()>192 || !value.matches("[A-Za-z0-9_.:/, +\\-]*"))
                throw new IllegalArgumentException("Log field bounds");
            json.append(",\"").append(entry.getKey()).append("\":\"").append(value).append('"');
        }
        byte[] data = json.append('}').toString().getBytes(StandardCharsets.UTF_8);
        if (data.length>2048) throw new IOException("Log event too large");
        File target = new File(directory, String.format(Locale.ROOT,"events-%04d.log",segment));
        if (target.length()+data.length+8 > SEGMENT_BYTES) target=new File(directory,String.format(Locale.ROOT,"events-%04d.log",++segment));
        reserve(data.length+8);
        CRC32 crc=new CRC32();crc.update(data);
        // Length + UTF-8 JSON + CRC permits a reader to identify an incomplete tail.
        try (FileOutputStream stream=new FileOutputStream(target,true); DataOutputStream out=new DataOutputStream(stream)) {
            out.writeInt(data.length);out.write(data);out.writeInt((int)crc.getValue());out.flush();stream.getFD().sync();
        }
        DurableFiles.directory(directory);
    }
    public synchronized void complete() throws IOException {
        append("session_complete",Collections.emptyMap());
        if (!new File(directory,"unfinished").delete()) throw new IOException("Cannot finish log session");
        DurableFiles.directory(directory);
    }
    public synchronized void failed(Exception error) throws IOException {
        append("session_failed",fields("error_class",error.getClass().getSimpleName()));
    }
    private void reserve(long extra) throws IOException {
        File[] sessions=root.listFiles(File::isDirectory);
        if (sessions==null) throw new IOException("Cannot audit log budget");
        Arrays.sort(sessions,(a,b)->a.getName().compareTo(b.getName()));
        long used=0;File latestFailure=null;
        for(File s:sessions){used+=size(s);if(new File(s,"unfinished").exists()&&!s.equals(directory))latestFailure=s;}
        for(File s:sessions){
            if(used+extra<=budget)break;
            if(s.equals(directory)||(s.equals(latestFailure)&&!new File(s,"exported").exists()))continue;
            long before=size(s);remove(s);used-=before;
        }
        if(used+extra>budget)throw new IOException("Log full; export the retained failure before continuing");
    }
    private static long size(File f)throws IOException {
        if(!f.isDirectory())return f.length();File[] children=f.listFiles();
        if(children==null)throw new IOException("Cannot enumerate logs");long n=0;for(File c:children)n+=size(c);return n;
    }
    private static void remove(File f)throws IOException {
        if(f.isDirectory()){File[] children=f.listFiles();if(children==null)throw new IOException("Cannot rotate logs");for(File c:children)remove(c);}
        if(!f.delete())throw new IOException("Cannot rotate log entry");
    }
    private static void marker(File f)throws IOException {try(FileOutputStream out=new FileOutputStream(f)){out.write(1);out.getFD().sync();}DurableFiles.directory(f.getParentFile());}
    /** Export excludes backups and content; acknowledge retention only after ZIP close succeeds. */
    public static void export(File root, OutputStream destination)throws IOException {
        File[] sessions=root.listFiles(File::isDirectory);if(sessions==null)throw new IOException("No diagnostic sessions");
        Arrays.sort(sessions,(a,b)->a.getName().compareTo(b.getName()));
        try(ZipOutputStream zip=new ZipOutputStream(destination)){
            byte[] buffer=new byte[8192];
            for(File session:sessions){File[] files=session.listFiles();if(files==null)throw new IOException("Cannot export session");
                for(File f:files){if(!f.isFile())continue;zip.putNextEntry(new ZipEntry(session.getName()+"/"+f.getName()));
                    try(InputStream in=new FileInputStream(f)){int n;while((n=in.read(buffer))!=-1)zip.write(buffer,0,n);}zip.closeEntry();}}
        }
        for(File session:sessions)marker(new File(session,"exported"));
    }
    /** Returns complete validated events only; a torn/corrupt tail is reported, never guessed. */
    public static List<String> read(File file)throws IOException {
        List<String> lines=new ArrayList<>();
        try(DataInputStream in=new DataInputStream(new FileInputStream(file))){
            while(in.available()!=0){int n=in.readInt();if(n<2||n>2048)throw new IOException("Invalid event length");
                byte[] data=new byte[n];in.readFully(data);long expected=in.readInt()&0xffffffffL;CRC32 crc=new CRC32();crc.update(data);
                if(crc.getValue()!=expected)throw new IOException("Corrupt event");lines.add(new String(data,StandardCharsets.UTF_8));}
        }return lines;
    }
    @Override public void close(){closed=true;}
}
