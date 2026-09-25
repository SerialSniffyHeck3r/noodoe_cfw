package io.opennoodoe.app.installer;
import org.junit.Test;
import static org.junit.Assert.*;

public class BondSessionTest {
 static class Peer implements BondSession.Peer,BondSession.Clock {
  int state=BondSession.NONE,requests,waits;long time,completeAt=25000;boolean accepted=true,denied,interrupt;
  public int state(){return state;}public long now(){return time;}
  public boolean request(){requests++;if(accepted)state=BondSession.BONDING;return accepted;}
  public void waitChange(long ms)throws InterruptedException{
   waits++;if(interrupt)throw new InterruptedException();time+=ms;
   if(time>=completeAt)state=denied?BondSession.NONE:BondSession.BONDED;
  }
 }
 @Test public void newPeerRequestsBondAndWaitsBeyondSocketDeadline()throws Exception{
  Peer p=new Peer();BondSession.ensure(p,p,s->{});assertEquals(1,p.requests);assertEquals(BondSession.BONDED,p.state);assertEquals(25000,p.time);
 }
 @Test public void retainedBondIsNeverRemovedOrRecreated()throws Exception{
  Peer p=new Peer();p.state=BondSession.BONDED;BondSession.ensure(p,p,s->{});assertEquals(0,p.requests);assertEquals(0,p.waits);
 }
 @Test public void existingPairingContinuesWithoutSecondRequest()throws Exception{
  Peer p=new Peer();p.state=BondSession.BONDING;BondSession.ensure(p,p,s->{});assertEquals(0,p.requests);assertEquals(BondSession.BONDED,p.state);
 }
 @Test public void declineDoesNotPromptAgain()throws Exception{
  Peer p=new Peer();p.denied=true;try{BondSession.ensure(p,p,s->{});fail();}catch(BondSession.Failure expected){}assertEquals(1,p.requests);
 }
 @Test public void missingResponseHasBoundedWait()throws Exception{
  Peer p=new Peer();p.completeAt=70000;try{BondSession.ensure(p,p,s->{});fail();}catch(BondSession.Failure expected){}assertEquals(60000,p.time);assertEquals(1,p.requests);
 }
 @Test public void rejectedRequestDoesNotOpenConnection()throws Exception{
  Peer p=new Peer();p.accepted=false;try{BondSession.ensure(p,p,s->{});fail();}catch(java.io.IOException expected){assertFalse(expected instanceof BondSession.Failure);}assertEquals(0,p.waits);assertEquals(1,p.requests);
 }
 @Test public void resetInterruptsPairingWait()throws Exception{
  Peer p=new Peer();p.interrupt=true;try{BondSession.ensure(p,p,s->{});fail();}catch(BondSession.Failure expected){assertTrue(Thread.currentThread().isInterrupted());}finally{Thread.interrupted();}assertEquals(1,p.requests);
 }
 @Test public void interruptedWorkerCannotStartNewPairing()throws Exception{
  Peer p=new Peer();Thread.currentThread().interrupt();try{BondSession.ensure(p,p,s->{});fail();}catch(BondSession.Failure expected){}finally{Thread.interrupted();}assertEquals(0,p.requests);
 }
}
