package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import java.io.*;
import java.nio.file.Files;
import java.util.*;
import org.junit.Test;
import static org.junit.Assert.*;

public class BootstrapCommitDiagnosticsTest {
 @Test public void rejectedCommitGetsReadOnlyDetailsWithoutRetry()throws Exception{
  String hash=RecoveryBundle.sha(new byte[512]);
  File file=new File(Files.createTempDirectory("commit-diagnostic").toFile(),"journal");
  InstallJournal j=new InstallJournal(file);j.values.setProperty("transaction","7");j.values.setProperty("image.sha256",hash);j.save("NDCP_VERIFIED");
  List<Integer> calls=new ArrayList<>();
  InstallerTransport fake=new InstallerTransport(){byte[] pending;
   public void send(byte[] b){int op=b[5]&255;calls.add(op);byte[] r;int flags=1;
    if(op==0x45){r=new byte[84];ByteCodec.putU32le(r,4,3);ByteCodec.putU32le(r,8,7);System.arraycopy(NdcpSession.hex(hash),0,r,52,32);}
    else if(op==0x43){r=NdcpSession.words(12,6,7,0x70000,0x70000);flags=3;}
    else if(op==0x85)r=NdcpSession.words(0,1,5,6,0,0,0,0,6,12,0,0);
    else throw new AssertionError("Unexpected mutation "+op);
    pending=NdcpSession.encode(op,(int)ByteCodec.u32le(b,8),r,flags);
   }
   public byte[] receive(long timeout){byte[] r=pending;pending=null;return r;}
   public void close(){}
  };
  try{new NdcpSession(fake).commit(j);fail();}catch(IOException e){assertTrue(e.getMessage().contains("워치독"));}
  assertEquals(Arrays.asList(0x45,0x43,0x85),calls);
  InstallJournal saved=new InstallJournal(file);assertEquals("5",saved.values.getProperty("commit.diagnostic.phase"));
  assertEquals("NDCP_COMMIT_RESULT_UNKNOWN",saved.state());
  try{new NdcpSession(fake).commit(saved);fail();}catch(IOException e){assertTrue(e.getMessage().contains("blocked"));}
  assertEquals(3,calls.size());
 }
 @Test public void destructiveFailureIsNeverDescribedAsUntouched()throws Exception{
  BootstrapCommitDiagnostics d=new BootstrapCommitDiagnostics(NdcpSession.words(0,1,7,9,0,1,9,0,6,11,1,2));
  assertTrue(d.summary().contains("확인해야"));assertFalse(d.summary().contains("시작되지"));
 }
 @Test public void malformedDiagnosticsCannotPromoteState()throws Exception{
  try{new BootstrapCommitDiagnostics(NdcpSession.words(0,1,99,0,0,0,0,0,0,0,0,0));fail();}catch(IOException expected){}
 }
}
