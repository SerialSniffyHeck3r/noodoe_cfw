package io.opennoodoe.app.installer;

import java.io.IOException;

/** Local cancellation is a transport fence, never a remote ABORT or RESET.
 * Every request checks before sending and again after receiving: a late reply
 * from the old socket cannot authorize the next operation on a new device. */
public final class SessionEpoch {
    private long generation = 1;
    public synchronized long current() { return generation; }
    public synchronized long invalidate() { return ++generation; }
    public synchronized boolean accepts(long token) { return token == generation; }
    public void check(long token) throws IOException {
        if (!accepts(token)) throw new IOException("App session reset; remote result must be queried");
    }
    public InstallerTransport bind(long token, InstallerTransport stream) throws IOException {
        if (!accepts(token)) { stream.close(); check(token); }
        return new InstallerTransport() {
            public void send(byte[] bytes) throws IOException { check(token); stream.send(bytes); check(token); }
            public byte[] receive(long timeout) throws IOException { check(token); byte[] b=stream.receive(timeout); check(token); return b; }
            public void close() throws IOException { stream.close(); }
        };
    }
}
