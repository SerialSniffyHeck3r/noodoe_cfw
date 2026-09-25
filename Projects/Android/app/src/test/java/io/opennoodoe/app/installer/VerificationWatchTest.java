package io.opennoodoe.app.installer;
import org.junit.Test;
import static org.junit.Assert.*;
import java.io.*;
import java.nio.file.Files;
import java.util.Arrays;
import io.opennoodoe.app.protocol.ByteCodec;

public class VerificationWatchTest {
 static BootstrapProgress.View view(long pos,long heartbeat)throws Exception{return new BootstrapProgress.View(NdcpSession.words(0,2,3,13,0,7,pos,134217728,0,heartbeat,heartbeat*1000,0,heartbeat,heartbeat,14,1));}
 @Test public void sixMinuteScanAdvancesBeyondFormerThreeMinuteLimit()throws Exception{
  VerificationWatch w=new VerificationWatch(0);
  for(int second=0;second<370;second++)w.observe(second*1000,view(second*360000L,second));
 }
 @Test public void repliesAndHeartbeatCannotExtendStalledWork()throws Exception{
  VerificationWatch w=new VerificationWatch(0);w.observe(0,view(1024,1));
  for(int s=1;s<90;s++)w.observe(s*1000,view(1024,s));
  try{w.observe(90000,view(1024,90));fail();}catch(IOException e){assertTrue(e.getMessage().contains("90초"));}
 }
 @Test public void regressionOrAlternatingSubphasesDoNotCountAsProgress()throws Exception{
  VerificationWatch w=new VerificationWatch(0);w.observe(0,view(2048,1));
  for(int s=1;s<90;s++)w.observe(s*1000,view(s%2==0?2048:1024,s));
  try{w.observe(90000,view(2048,90));fail();}catch(IOException expected){}
 }
 @Test public void absoluteBudgetRemainsBounded()throws Exception{
  VerificationWatch w=new VerificationWatch(0);
  for(int s=0;s<1800;s+=30)w.observe(s*1000,view(s,1));
  try{w.observe(1800000,view(1801,1));fail();}catch(IOException e){assertTrue(e.getMessage().contains("30분"));}
 }
 @Test public void scopedPollCompletesSixMinuteHashUsingOnlyStatusReads()throws Exception{
  final long[] ms={0};final int[] queries={0};
  InstallerTransport radio=new InstallerTransport(){byte[] reply;
   public void send(byte[] f)throws IOException {
    int op=f[5]&255;byte[] r;queries[0]++;
    if(op==0x56){boolean done=ms[0]>=370000;r=Arrays.copyOf(NdcpSession.words(0,done?8:13,0,0,0,0,0,0),done?64:32);}
    else if(op==0x84)r=NdcpSession.words(0,2,3,13,0,7,ms[0]*350,134217728,0,ms[0],ms[0],0,queries[0],queries[0],14,1);
    else throw new AssertionError("Unexpected mutation "+op);
    reply=NdcpSession.encode(op,(int)ByteCodec.u32le(f,8),r,1);
   }
   public byte[] receive(long timeout){return reply;}public void close(){}
  };
  InstallerPresentation p=new InstallerPresentation(()->ms[0]);p.begin("guided-bootstrap");
  ScopedBackup backup=new ScopedBackup(new NdcpSession(radio),p,Files.createTempDirectory("long-scan").resolve("expected.bin").toFile());
  assertEquals(64,backup.poll("baseline",new ScopedBackup.Wait(){public long now(){return ms[0];}public void pause(){ms[0]+=1000;}}).length);
  assertEquals(370000,ms[0]);assertEquals("baseline",p.snapshot().key);assertEquals(20,p.snapshot().overallPercent);
  assertFalse(p.snapshot().text.contains("부팅 기록"));
 }
 @Test public void journeyNeverFinishesOnTransferOrError(){
  InstallerPresentation p=new InstallerPresentation(()->0);p.begin("guided-bootstrap");
  p.stage("baseline","scan",1,100,"B");assertEquals(20,p.snapshot().overallPercent);
  p.stage("file-upload-4","file",100,100,"B");assertEquals(40,p.snapshot().overallPercent);
  p.stage("file-0-7-4","file",1,100,"B");assertEquals(40,p.snapshot().overallPercent);
  p.stage("health","confirm",0,0,"");assertEquals(90,p.snapshot().overallPercent);
  p.finish("failed",true);assertEquals(90,p.snapshot().overallPercent);
  p.begin("guided-bootstrap");p.stage("done","confirmed",1,1,"");assertEquals(100,p.snapshot().overallPercent);
 }
}
