package io.opennoodoe.app.installer;
import io.opennoodoe.app.diagnostics.SessionLog;
import io.opennoodoe.app.protocol.ndcp.*;
import io.opennoodoe.app.transport.SppTransport;
import org.junit.Test;
import static org.junit.Assert.*;
import java.io.*;
import java.nio.file.Files;
import java.util.*;

public final class DiagnosticsTest {
 @Test public void tornTailAndCrcAreNotValidEvents()throws Exception{
  File root=Files.createTempDirectory("logs").toFile();
  try(SessionLog log=new SessionLog(root)){
   log.append("command_intent",SessionLog.fields("opcode","67","sequence","9"));
   File f=new File(new File(root,log.id()),"events-0000.log");
   assertEquals(2,SessionLog.read(f).size());
   try(FileOutputStream out=new FileOutputStream(f,true)){out.write(4);}
   try{SessionLog.read(f);fail();}catch(EOFException expected){}
  }
 }
 @Test public void privateContentCannotEnterSchema()throws Exception{
  try(SessionLog log=new SessionLog(Files.createTempDirectory("privacy").toFile())){
   for(String name:new String[]{"title","body","latitude","longitude","pairing_key","artist"})
    try{log.append("content",SessionLog.fields(name,"secret"));fail();}catch(IllegalArgumentException expected){}
  }
 }
 @Test public void retainsLatestFailureAndFailsClosedAtBudget()throws Exception{
  File root=Files.createTempDirectory("budget").toFile();String failed;
  try(SessionLog log=new SessionLog(root,4096)){failed=log.id();log.failed(new IOException());}
  boolean full=false;
  try(SessionLog log=new SessionLog(root,4096)){
   try{for(int i=0;i<100;i++)log.append("step",SessionLog.fields("offset",Integer.toString(i)));}catch(IOException expected){full=true;}
   assertTrue(full);assertTrue(new File(root,failed).exists());
  }
 }
 @Test public void failedExportDoesNotReleaseFailure()throws Exception{
  File root=Files.createTempDirectory("export").toFile();String failed;
  try(SessionLog log=new SessionLog(root)){failed=log.id();log.failed(new IOException());}
  try{SessionLog.export(root,new OutputStream(){public void write(int n)throws IOException{throw new IOException();}});fail();}catch(IOException expected){}
  assertFalse(new File(new File(root,failed),"exported").exists());
  SessionLog.export(root,new ByteArrayOutputStream());assertTrue(new File(new File(root,failed),"exported").exists());
 }
 static final class FailingAudit implements SppTransport,CommandAudit {
  int writes;
  public void beforeCommand(int op,int seq,int bytes)throws IOException{throw new IOException("disk unavailable");}
  public void commandResult(int op,int seq,long result){}
  public void send(byte[] b){writes++;}
  public byte[] receive(long timeout){return new byte[0];}
  public void close(){}
 }
 @Test public void diskFailurePreventsRemoteMutation()throws Exception{
  FailingAudit transport=new FailingAudit();
  try{new NdcpClient(transport).request(0x43,new byte[40]);fail();}catch(IOException expected){}
  assertEquals(0,transport.writes);
 }
}
