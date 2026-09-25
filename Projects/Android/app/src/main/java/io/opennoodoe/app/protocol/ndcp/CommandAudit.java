package io.opennoodoe.app.protocol.ndcp;
import java.io.IOException;
/** Metadata only: implementations must never record application payloads. */
public interface CommandAudit {
    void beforeCommand(int opcode,int sequence,int bytes) throws IOException;
    void commandResult(int opcode,int sequence,long result) throws IOException;
}
