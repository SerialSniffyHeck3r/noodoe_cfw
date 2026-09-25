package io.opennoodoe.app.installer;
import org.junit.Test;
import java.io.*;
import java.nio.file.Files;
import static org.junit.Assert.*;
public class VisualInstallConfirmationTest {
 private InstallJournal journal()throws Exception{InstallJournal j=new InstallJournal(new File(Files.createTempDirectory("visual-confirm").toFile(),"journal"));j.bind("AA","bundle");j.values.setProperty("uid","1,2,3");j.values.setProperty("attempt.id","one");j.save("WAIT_CFW_BOOT");return j;}
 @Test public void defaultObserverCannotSilentlyApprove()throws Exception {
  InstallJournal j=journal();try{VisualInstallConfirmation.require(j,s->{},"hash",Long.MAX_VALUE);fail();}catch(IOException expected){}assertFalse(j.values.containsKey("visual.confirmed"));assertEquals("WAIT_CFW_BOOT",j.state());
 }
 @Test public void approvalIsExactAndDurableAcrossReconnect()throws Exception {
  InstallJournal j=journal();byte[] sha=new byte[32];String key=VisualInstallConfirmation.binding(j,8,sha);int[] count={0};
  StockUpdateSession.Progress p=new StockUpdateSession.Progress(){public void update(String s){}public void awaitScreen(String b,long deadline){count[0]++;}};
  VisualInstallConfirmation.require(j,p,key,Long.MAX_VALUE);VisualInstallConfirmation.require(j,s->{},key,Long.MAX_VALUE);assertEquals(1,count[0]);
  assertNotEquals(key,VisualInstallConfirmation.binding(j,9,sha));sha[0]=1;assertNotEquals(key,VisualInstallConfirmation.binding(j,8,sha));sha[0]=0;
  j.values.setProperty("attempt.id","two");assertNotEquals(key,VisualInstallConfirmation.binding(j,8,sha));j.values.setProperty("attempt.id","one");j.values.setProperty("uid","9,2,3");assertNotEquals(key,VisualInstallConfirmation.binding(j,8,sha));
 }
 @Test public void interruptedWaitDoesNotPersistApproval()throws Exception {
  InstallJournal j=journal();try{VisualInstallConfirmation.require(j,new StockUpdateSession.Progress(){public void update(String s){}public void awaitScreen(String b,long d)throws Exception{throw new InterruptedException();}},"hash",1);fail();}catch(InterruptedException expected){}assertFalse(j.values.containsKey("visual.confirmed"));
 }
 @Test public void productionLogWrapperForwardsTheLiveDevicePoll()throws Exception {
  InstallJournal j=journal();int[] polls={0};
  try(io.opennoodoe.app.diagnostics.SessionLog log=new io.opennoodoe.app.diagnostics.SessionLog(Files.createTempDirectory("screen-log").toFile())){
   StockUpdateSession.Progress ui=new StockUpdateSession.Progress(){public void update(String s){}
    public void awaitScreen(String b,long d,StockUpdateSession.ScreenCheck check)throws Exception{check.poll();}};
   VisualInstallConfirmation.require(j,new LoggedInstallerProgress(ui,log),"bound",Long.MAX_VALUE,()->polls[0]++);
   assertEquals(1,polls[0]);assertEquals("bound",j.values.getProperty("visual.confirmed"));log.complete();
  }
 }
}
