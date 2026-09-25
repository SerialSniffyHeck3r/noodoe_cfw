package io.opennoodoe.app.installer;
import org.junit.Test;
import static org.junit.Assert.*;
import java.io.*;
import java.nio.file.Files;
import io.opennoodoe.app.protocol.ByteCodec;

public class BootstrapInstallFlowTest {
 static final class Wait implements BootstrapInstallFlow.Wait {long ms;public long now(){return ms;}public void pause(){ms+=250;}}
 static final class Radio implements InstallerTransport {
  byte[] response;int statuses,probes,failures=2;boolean dropped;int state=2,phase=0;boolean menuThenReady;
  public void send(byte[] frame)throws IOException {
   int op=frame[5]&255;byte[] b;int flags=1;
   if(op==0x5a){statuses++;int s=menuThenReady&&statuses<3?9:state;b=NdcpSession.words(0,1,s,phase,0,0,0,1,120000);}
   else if(op==0x80){probes++;if(dropped)throw new IOException("lost reply");assertEquals(0,ByteCodec.u32le(frame,16));assertEquals(2,ByteCodec.u32le(frame,20));
    if(failures-->0){b=NdcpSession.words(failures==1?3:2);flags=3;}else b=java.util.Arrays.copyOf(NdcpSession.words(0,0,0,0,0,0,2,0),34);
   }else throw new AssertionError("Wait issued a mutation: "+op);
   response=NdcpSession.encode(op,(int)ByteCodec.u32le(frame,8),b,flags);
  }
  public byte[] receive(long timeout){return response;}public void close(){}
 }
 @Test public void importedRunningBootstrapCanContinueAfterIdentity()throws Exception{
  InstallJournal j=new InstallJournal(Files.createTempDirectory("flow").resolve("journal").toFile());j.save("IMPORTED");
  BootstrapInstallFlow.identified(j);assertEquals("BOOTSTRAP_IDENTIFIED",j.state());assertFalse(BootstrapInstallFlow.resume(j.state()));
 }
 @Test public void uncertainFilePublicationIsNeverResetToFresh()throws Exception{
  InstallJournal j=new InstallJournal(Files.createTempDirectory("flow").resolve("journal").toFile());
  for(String state:new String[]{"CREATE_COMMIT_RESULT_UNKNOWN","FILE_CREATED","NDCP_COMMIT_RESULT_UNKNOWN","WAIT_CFW_BOOT"}){
   j.save(state);BootstrapInstallFlow.identified(j);assertEquals(state,j.state());try{BootstrapInstallFlow.resume(state);fail();}catch(IOException expected){}
  }
  assertTrue(BootstrapInstallFlow.resume("PROVISIONED"));assertTrue(BootstrapInstallFlow.resume("NDCP_DATA_RESULT_UNKNOWN"));
 }
 @Test public void rejectedReadOnlyPreparationCanRestartWithoutLosingEvidence()throws Exception{
  InstallJournal j=new InstallJournal(Files.createTempDirectory("flow").resolve("journal").toFile());
  j.values.setProperty("backup.directory","previous-read-plan");j.save("BACKUP_IN_PROGRESS");
  BootstrapInstallFlow.identified(j);assertEquals("BOOTSTRAP_IDENTIFIED",j.state());
  assertTrue(j.values.stringPropertyNames().stream().anyMatch(k->k.startsWith("backup.abandoned.")));
  j.values.setProperty("create.kind","4");j.save("BACKUP_IN_PROGRESS");BootstrapInstallFlow.identified(j);
  assertEquals("BACKUP_IN_PROGRESS",j.state());try{BootstrapInstallFlow.resume(j.state());fail();}catch(IOException expected){}
 }
 @Test public void sameConnectionWaitsForLocalIntentAndStartupReadiness()throws Exception{
  Radio r=new Radio();r.menuThenReady=true;Wait w=new Wait();
  BootstrapInstallFlow.awaitLocalInstall(new NdcpSession(r),s->{},w,3000);assertEquals(3,r.probes);assertEquals(5,r.statuses);
 }
 @Test public void transportFailureDoesNotRepeatEvenAReadWithinUnknownSession()throws Exception{
  Radio r=new Radio();r.dropped=true;try{BootstrapInstallFlow.awaitLocalInstall(new NdcpSession(r),s->{},new Wait(),3000);fail();}catch(IOException expected){}
  assertEquals(1,r.probes);
 }
 @Test public void committedRecoveryAndErrorNeverStartPreparation()throws Exception{
  for(int state:new int[]{5,7,8}){Radio r=new Radio();r.state=state;
   try{BootstrapInstallFlow.awaitLocalInstall(new NdcpSession(r),s->{},new Wait(),3000);fail();}catch(IOException expected){}assertEquals(0,r.probes);
  }
 }
 @Test public void localPromptTimesOutWithoutWriting()throws Exception{
  Radio r=new Radio();r.state=1;try{BootstrapInstallFlow.awaitLocalInstall(new NdcpSession(r),s->{},new Wait(),1000);fail();}catch(IOException e){assertTrue(e.getMessage().contains("쓰기 작업은 시작하지"));}assertEquals(0,r.probes);
 }
}
