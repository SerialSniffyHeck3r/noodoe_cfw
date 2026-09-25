package io.opennoodoe.app.companion;
/** Coalesced wakeups, not a work queue. Only the service owner writes SPP. */
public final class CompanionSignal {
 private long revision;
 public synchronized long version(){return revision;}
 public synchronized void changed(){++revision;notifyAll();}
 public synchronized void await(long observed,long millis)throws InterruptedException {
  if(observed==revision&&millis>0)wait(millis);
 }
}
