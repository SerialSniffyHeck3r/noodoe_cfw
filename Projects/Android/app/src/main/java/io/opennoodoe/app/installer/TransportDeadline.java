package io.opennoodoe.app.installer;
import java.util.concurrent.*;
/** Closing the socket releases a blocked native connect/write. No command retry. */
public final class TransportDeadline implements AutoCloseable {
 private boolean finished,expired;private final ScheduledFuture<?> future;
 public TransportDeadline(ScheduledExecutorService timer,long ms,Runnable abort){
  future=timer.schedule(()->{synchronized(this){if(finished)return;expired=true;finished=true;}abort.run();},ms,TimeUnit.MILLISECONDS);
 }
 public synchronized boolean expired(){return expired;}
 public synchronized void close(){finished=true;future.cancel(false);}
}
