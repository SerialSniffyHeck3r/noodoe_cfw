package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import org.junit.Test;
import static org.junit.Assert.*;
import java.io.*;
import java.nio.file.Files;
import java.util.*;

public class BootstrapConnectSessionTest {
 static final class Clock implements BootstrapConnectSession.Clock {
  long time;public long now(){return time;}public void sleep(long ms){time+=ms;}
 }
 static final class Radio implements InstallerTransport {
  final RecoveryBundle bundle;byte[] reply;int role=1,uid=1;boolean wrongHash,closed;List<Integer> ops=new ArrayList<>();
  Radio(RecoveryBundle b){bundle=b;}
  public void send(byte[] f)throws IOException{
   int op=f[5]&255;ops.add(op);assertEquals(0x58,op);byte[] id=Arrays.copyOf(NdcpSession.words(0,1,role,uid,2,3),88);
   try{System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(bundle.paddedImage("bootstrap"))),0,id,24,32);}catch(Exception e){throw new IOException(e);}
   if(wrongHash)id[24]^=1;reply=NdcpSession.encode(op,(int)ByteCodec.u32le(f,8),id,1);
  }
  public byte[] receive(long ms){byte[] r=reply;reply=null;return r;}public void close(){closed=true;}
 }
 private InstallJournal journal()throws Exception{
  InstallJournal j=new InstallJournal(new File(Files.createTempDirectory("bootstrap-connect").toFile(),"j"));j.save("STOCK_ACCEPTED_WAIT_IGN_OFF");return j;
 }
 @Test public void temporaryRadioAbsenceThenBootstrapUsesPairingPathOnly()throws Exception{
  RecoveryBundle b=new InstallerTest().valid();Radio radio=new Radio(b);InstallJournal j=journal();int[] opens={0};
  InstallerController.Connections c=new InstallerController.Connections(){public InstallerTransport open(){fail("must use pairing/reboot transport");return null;}
   public InstallerTransport openBoot(StockUpdateSession.Progress p)throws IOException{if(++opens[0]<3)throw new IOException("BL restarting");return radio;}};
  new BootstrapConnectSession(new Clock()).run(c,b,j,s->{});assertEquals(3,opens[0]);assertTrue(radio.closed);assertEquals("BOOTSTRAP_IDENTIFIED",j.state());assertEquals("1,2,3",j.values.getProperty("uid"));assertEquals(Arrays.asList(0x58,0x58),radio.ops);
 }
 @Test public void readinessAfterFiveFailedConnectionsStillConnects()throws Exception{
  RecoveryBundle b=new InstallerTest().valid();Radio r=new Radio(b);int[] attempts={0};Clock clock=new Clock();
  new BootstrapConnectSession(clock).run(()->{if(++attempts[0]<12)throw new IOException("not booted");return r;},b,journal(),s->{});
  assertEquals(12,attempts[0]);assertTrue(clock.time<240000);assertTrue(r.closed);
 }
 @Test public void pairingDeclineStopsAllAutomaticRetries()throws Exception{
  int[] opens={0};InstallJournal j=journal();try{new BootstrapConnectSession(new Clock()).run(()->{opens[0]++;throw new BondSession.Failure("declined");},new InstallerTest().valid(),j,s->{});fail();}catch(BondSession.Failure expected){}
  assertEquals(1,opens[0]);assertEquals("STOCK_ACCEPTED_WAIT_IGN_OFF",j.state());
 }
 @Test public void timeoutPreservesTransferEvidenceAndDoesNotResend()throws Exception{
  int[] opens={0};InstallJournal j=journal();try{new BootstrapConnectSession(new Clock()).run(()->{opens[0]++;throw new IOException("absent");},new InstallerTest().valid(),j,s->{});fail();}catch(BootstrapConnectSession.Unavailable expected){}
  assertEquals(48,opens[0]);assertEquals("STOCK_ACCEPTED_WAIT_IGN_OFF",j.state());
 }
 @Test public void differentFirmwareDoesNotBecomeIdentified()throws Exception{reject(1,true,1);}
 @Test public void anotherUidCannotReplaceBoundDevice()throws Exception{reject(1,false,9);}
 @Test public void productIsNotMistakenForBootstrap()throws Exception{reject(2,false,1);}
 private void reject(int role,boolean hash,int uid)throws Exception{
  RecoveryBundle b=new InstallerTest().valid();Radio r=new Radio(b);r.role=role;r.wrongHash=hash;r.uid=uid;InstallJournal j=journal();j.values.setProperty("uid","1,2,3");j.save(j.state());int[] opens={0};
  try{new BootstrapConnectSession(new Clock()).run(()->{opens[0]++;return r;},b,j,s->{});fail();}catch(IOException expected){}
  assertEquals(1,opens[0]);assertEquals("STOCK_ACCEPTED_WAIT_IGN_OFF",j.state());assertTrue(r.closed);
 }
 @Test public void unknownCommitIsNotClearedByReconnect()throws Exception{
  InstallJournal j=journal();j.save("NDCP_COMMIT_RESULT_UNKNOWN");try{new BootstrapConnectSession(new Clock()).run(()->{fail();return null;},new InstallerTest().valid(),j,s->{});fail();}catch(InstallJournal.StateBlocked expected){}assertEquals("NDCP_COMMIT_RESULT_UNKNOWN",j.state());
 }
}
