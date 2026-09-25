package io.opennoodoe.app.installer;
import io.opennoodoe.app.diagnostics.SessionLog;
import io.opennoodoe.app.protocol.ndcp.CommandAudit;
import java.io.IOException;
import java.util.UUID;

/** One connection generation, no raw wire or personal content in evidence. */
final class LoggedInstallerTransport implements InstallerTransport,CommandAudit {
    private final InstallerTransport stream;
    private final SessionLog log;
    private final StockUpdateSession.Progress progress;
    private final String epoch=UUID.randomUUID().toString();
    private final long started=System.nanoTime();private long txBytes,rxBytes;private boolean closed;
    LoggedInstallerTransport(InstallerTransport stream,SessionLog log)throws IOException {
        this(stream,log,text->{});
    }
    LoggedInstallerTransport(InstallerTransport stream,SessionLog log,StockUpdateSession.Progress progress)throws IOException {
        this.stream=stream;this.log=log;this.progress=progress;
        try{log.append("connection_open",SessionLog.fields("epoch",epoch));}
        catch(IOException e){try{stream.close();}catch(IOException close){e.addSuppressed(close);}throw e;}
    }
    // DATA is journaled by verified sector progress; no per-chunk NOR/phone log storm.
    private boolean significant(int opcode){return opcode==0x40||opcode==0x42||opcode==0x43||opcode==0x44||opcode==0x47||opcode==0x48||opcode==0x1f||opcode==0x52||opcode==0x54||opcode==0x55||opcode==0x5c||opcode==0x61||opcode==0x62||opcode==0x81||opcode==0x82||opcode==0x88||opcode==0x8e||opcode==0x90||opcode==0x91;}
    public void beforeCommand(int op,int seq,int bytes)throws IOException {
        if(significant(op))log.append("command_intent",SessionLog.fields("epoch",epoch,"opcode",Integer.toString(op),"sequence",Integer.toUnsignedString(seq),"length",Integer.toString(bytes)));
    }
    public void commandResult(int op,int seq,long result)throws IOException {
        progress.reply();
        if(significant(op)||result!=0)log.append("command_result",SessionLog.fields("epoch",epoch,"opcode",Integer.toString(op),"sequence",Integer.toUnsignedString(seq),"result",Long.toString(result)));
    }
    public void send(byte[] bytes)throws IOException {stream.send(bytes);txBytes+=bytes.length;}
    public byte[] receive(long timeout)throws IOException {byte[] data=stream.receive(timeout);if(data!=null)rxBytes+=data.length;return data;}
    public void close()throws IOException {
        if(closed)return;closed=true;
        try{stream.close();}finally{log.append("connection_closed",SessionLog.fields("epoch",epoch,"tx_bytes",Long.toString(txBytes),"rx_bytes",Long.toString(rxBytes),"elapsed_ms",Long.toString((System.nanoTime()-started)/1000000)));}
    }
}
