package io.opennoodoe.app.transport;
import java.io.Closeable;
import java.io.IOException;
/** Exclusive ordered bytes. No stock handshake, role assumptions or autosync. */
public interface SppTransport extends Closeable {
 void send(byte[] bytes) throws IOException;
 byte[] receive(long timeoutMs) throws IOException;
}
