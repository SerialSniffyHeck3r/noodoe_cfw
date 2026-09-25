package io.opennoodoe.app.installer;
import org.junit.Test;
import static org.junit.Assert.*;
import java.io.IOException;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;
import io.opennoodoe.app.protocol.ndcp.MaintenanceStatus;

public final class InstallerPresentationTest {
 static final class Clock implements InstallerPresentation.Clock {long ms;public long now(){return ms;}}
 @Test public void stageRateIsLocalAndChangingStageDiscardsOldRate(){
  Clock c=new Clock();InstallerPresentation p=new InstallerPresentation(c);p.begin("install");p.stage("stage","image",0,4096,"B");
  c.ms=2000;p.reply();p.stage("stage","image",2048,4096,"B");
  assertEquals(50,p.snapshot().percent);assertTrue(p.snapshot().rate.contains("1.0 KiB/s"));assertTrue(p.snapshot().rate.contains("이 단계"));
  p.stage("preserve","verify",0,8192,"B");assertEquals("",p.snapshot().rate);assertEquals(0,p.snapshot().percent);
 }
 @Test public void RepliesDoNotPretendToAdvanceBytes(){
  Clock c=new Clock();InstallerPresentation p=new InstallerPresentation(c);p.begin("install");p.stage("file-1","write",12,100,"B");
  c.ms=31001;p.reply();p.stage("file-1","write",12,100,"B");assertTrue(p.snapshot().notice.contains("진행량"));
  c.ms=47002;assertTrue(p.snapshot().notice.contains("결과는 아직 미확인"));
  p.reply();p.stage("file-1","write",13,100,"B");assertEquals("",p.snapshot().notice);
 }
 @Test public void unknownReplyAndHumanWaitHaveDifferentDisplays(){
  Clock c=new Clock();InstallerPresentation p=new InstallerPresentation(c);p.begin("install");c.ms=16000;
  assertTrue(p.snapshot().notice.contains("자동 재전송하지"));assertEquals(-1,p.snapshot().percent);
  for(String stage:new String[]{"confirm","reboot","stock-reboot","health"}){
   p.stage(stage,"wait",0,0,"");c.ms+=60000;assertEquals("",p.snapshot().notice);assertEquals(-1,p.snapshot().percent);
  }
 }
 @Test public void failureNeverLooksCompleteAndStoppedTimeIsFrozen(){
  Clock c=new Clock();InstallerPresentation p=new InstallerPresentation(c);p.begin("install");p.stage("stage","sent",100,100,"B");
  c.ms=5000;p.finish("lost COMMIT reply",true);assertEquals(-1,p.snapshot().percent);assertTrue(p.snapshot().error);
  String before=p.snapshot().timing;c.ms=900000;assertEquals(before,p.snapshot().timing);
  p.begin("install");p.stage("done","confirmed",1,1,"완료");p.finish("confirmed",false);assertEquals(100,p.snapshot().percent);
 }
 @Test public void roleSpecificRecoveryDoesNotRequireIgnInBootstrap(){
  assertTrue(InstallerFailure.rescue("bootstrap").contains("추가 IGN 조작은 필요 없어요"));
  assertTrue(InstallerFailure.rescue("product").contains("키 OFF"));
  assertTrue(InstallerFailure.explain(new IOException("timeout"),"bootstrap").contains("아직 확정하지"));
 }
 @Test public void expectedRebootDisconnectIsStillUnconfirmed(){
  Clock c=new Clock();InstallerPresentation p=new InstallerPresentation(c);p.begin("restore-stock");
  p.stage("unconfirmed","reply lost",0,0,"");p.finish("check stock boot",false);
  assertEquals(-1,p.snapshot().percent);assertTrue(p.snapshot().notice.contains("확인하지 못했어요"));assertEquals("작업 결과 미확인",p.snapshot().title);
 }
 @Test public void legacyStatusRetainsTypedErrorAndCorrectHoldInstructions()throws Exception{
  MaintenanceStatus s=new MaintenanceStatus(NdcpSession.words(0,1,7,0,0,0,0,0,0));assertTrue(s.message().contains("추가 IGN 조작은 없어요"));
  s=new MaintenanceStatus(NdcpSession.words(0,1,8,7,1,10,0x50004,0,0));assertTrue(s.message().contains("00050004"));
 }
 @Test public void detailedStatusRejectsCorruptionAndSeparatesLiveness()throws Exception{
  byte[] b=NdcpSession.words(0,2,3,7,2,4,4096,524288,0,42,60000,31000,18,17,7,2);
  BootstrapProgress.View v=new BootstrapProgress.View(b);assertEquals(4096,v.position);assertEquals(31000,v.idle);assertTrue(v.text().contains("정체"));assertTrue(v.text().contains("순정 복구본"));
  b[4]=3;try{new BootstrapProgress.View(b);fail();}catch(IOException expected){}
  try{new BootstrapProgress.View(new byte[63]);fail();}catch(IOException expected){}
 }
 @Test public void deadlineCancelsWithoutAbortingSuccessfulWork()throws Exception{
  ScheduledExecutorService timer=Executors.newSingleThreadScheduledExecutor();AtomicInteger calls=new AtomicInteger();
  try{TransportDeadline guard=new TransportDeadline(timer,50,calls::incrementAndGet);guard.close();Thread.sleep(100);assertFalse(guard.expired());assertEquals(0,calls.get());}finally{timer.shutdownNow();}
 }
 @Test public void evidenceRecordsMachineFieldsWithoutPrivateUiText()throws Exception{
  java.io.File root=java.nio.file.Files.createTempDirectory("progress-log").toFile();
  try(io.opennoodoe.app.diagnostics.SessionLog log=new io.opennoodoe.app.diagnostics.SessionLog(root)){
   LoggedInstallerProgress p=new LoggedInstallerProgress(v->{},log);
   p.stage("stage","private wallpaper name",10,100,"B");p.stage("stage","private music title",11,100,"B");
   p.deviceStatus(new BootstrapProgress.View(NdcpSession.words(0,2,8,7,2,4,4096,524288,0x50004,42,60000,31000,18,17,7,2)));
   String events=io.opennoodoe.app.diagnostics.SessionLog.read(new java.io.File(new java.io.File(root,log.id()),"events-0000.log")).toString();
   assertTrue(events.contains("bootstrap_status"));assertTrue(events.contains("327684"));assertFalse(events.contains("private"));
  }
 }
 @Test public void deadlineReleasesWaitAndDoesNotHoldItsMonitorWhileClosing()throws Exception{
  ScheduledExecutorService timer=Executors.newSingleThreadScheduledExecutor();CountDownLatch entered=new CountDownLatch(1),release=new CountDownLatch(1);
  try{TransportDeadline guard=new TransportDeadline(timer,5,()->{entered.countDown();try{release.await(2,TimeUnit.SECONDS);}catch(InterruptedException e){Thread.currentThread().interrupt();}});
   assertTrue(entered.await(1,TimeUnit.SECONDS));long start=System.nanoTime();guard.close();assertTrue(guard.expired());assertTrue(System.nanoTime()-start<500_000_000L);
  }finally{release.countDown();timer.shutdownNow();}
 }
}
