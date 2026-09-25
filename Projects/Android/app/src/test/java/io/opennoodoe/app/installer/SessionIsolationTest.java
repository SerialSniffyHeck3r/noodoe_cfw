package io.opennoodoe.app.installer;
import org.junit.Test;import static org.junit.Assert.*;import java.io.*;import java.nio.file.Files;
public class SessionIsolationTest {
 @Test public void lateReplyCannotCrossReset()throws Exception{
  SessionEpoch e=new SessionEpoch();long old=e.current();int[] sent={0};
  InstallerTransport t=e.bind(old,new InstallerTransport(){public void send(byte[] b){sent[0]++;}public byte[] receive(long ms){e.invalidate();return new byte[]{1};}public void close(){}});
  t.send(new byte[]{1});try{t.receive(1);fail();}catch(IOException expected){}
  try{t.send(new byte[]{2});fail();}catch(IOException expected){}assertEquals(1,sent[0]);assertFalse(e.accepts(old));
 }
 @Test public void anotherDeviceCannotBlockCurrentAttempt()throws Exception{
  File root=Files.createTempDirectory("attempts").toFile();InstallJournal j=new InstallJournal(new File(root,"old-AABBCC001122.journal"));
  j.values.setProperty("address","AA:BB:CC:00:11:22");j.values.setProperty("bundle","old");j.save("NDCP_COMMIT_RESULT_UNKNOWN");
  DeviceAttempts.check(root,"AA:BB:CC:00:11:23","new");
  try{DeviceAttempts.check(root,"AA:BB:CC:00:11:22","new");fail();}catch(IOException expected){}
  assertEquals("NDCP_COMMIT_RESULT_UNKNOWN",new InstallJournal(new File(root,"old-AABBCC001122.journal")).state());
 }
 @Test public void v2PartialPublicationRequiresExactEvidenceFormat()throws Exception{
  InstallJournal j=new InstallJournal(Files.createTempDirectory("v2").resolve("state").toFile());j.save("FILE_CREATED");
  try{BootstrapInstallFlow.resume(j);fail();}catch(IOException expected){}
  j.values.setProperty("backup.mode","scoped-v2");assertTrue(BootstrapInstallFlow.resume(j));
  j.save("NDCP_COMMIT_RESULT_UNKNOWN");try{BootstrapInstallFlow.resume(j);fail();}catch(IOException expected){}
 }
 @Test public void uidAliasDoesNotHideAmbiguousCommit()throws Exception{
  File root=Files.createTempDirectory("uid-attempt").toFile();InstallJournal old=new InstallJournal(new File(root,"a.journal"));
  old.bind("AA:BB:CC:00:11:22","old");old.values.setProperty("uid","1,2,3");old.save("NDCP_COMMIT_RESULT_UNKNOWN");
  InstallJournal current=new InstallJournal(new File(root,"b.journal"));current.bind("AA:BB:CC:00:11:23","new");
  DeviceAttempts.checkUid(current,"4,5,6");
  try{DeviceAttempts.checkUid(current,"1,2,3");fail();}catch(IOException expected){}
  old.save("STOCK_RETURN_CONFIRMED");DeviceAttempts.checkUid(current,"1,2,3");
 }
}
