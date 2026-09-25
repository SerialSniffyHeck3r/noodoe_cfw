package io.opennoodoe.app.installer;
import org.junit.Test;
import static org.junit.Assert.*;
import java.io.*;
import java.nio.file.Files;
import java.util.Arrays;
import io.opennoodoe.app.protocol.ByteCodec;

public final class BootstrapContinuationTest {
 byte[] image=new byte[RecoveryBundle.APP_BYTES];
 InstallJournal journal()throws Exception {
  InstallJournal j=new InstallJournal(Files.createTempDirectory("continuation").resolve("j").toFile());
  j.values.setProperty("transaction","7");j.values.setProperty("version","65542");
  j.values.setProperty("resident.version","15");j.values.setProperty("image.sha256",RecoveryBundle.sha(image));j.save("NDCP_VERIFIED");return j;
 }
 byte[] status(int state){byte[] r=Arrays.copyOf(NdcpSession.words(0,state,7,image.length,image.length,65542,NdcpSession.crc(image,image.length),0,15,65542,0x7f90,image.length,state==3?0:NdcpSession.crc(image,image.length)),84);
  System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(image)),0,r,52,32);return r;}
 @Test public void disconnectedVerifiedImageDoesNotNeedAnotherUpload()throws Exception{
  assertEquals(BootstrapContinuation.RECHECK,BootstrapContinuation.inspect(status(3),image,journal()));
 }
 @Test public void committedAndRebootWaitNeverTriggerSecondCommit()throws Exception{
  for(int s:new int[]{4,5})assertEquals(BootstrapContinuation.INSTALLING,BootstrapContinuation.inspect(status(s),image,journal()));
 }
 @Test public void differentImageTransactionOrBootMetadataCannotBeReused()throws Exception{
  for(int offset:new int[]{8,12,16,20,24,32,36,40,44,48,52}){
   byte[] r=status(4);r[offset]^=1;
   try{BootstrapContinuation.inspect(r,image,journal());fail("offset "+offset);}catch(IOException expected){}
  }
 }
 @Test public void missingProofDoesNotInventResume()throws Exception{
  for(int state:new int[]{0,1,2,6}){byte[] r=status(state);r[12]=1;assertEquals(0,BootstrapContinuation.inspect(r,image,journal()));}
  InstallJournal j=journal();j.values.remove("transaction");
  try{BootstrapContinuation.inspect(status(3),image,j);fail();}catch(IOException expected){}
 }
 @Test public void completeDataWithLostFinishCanBeRecheckedWithoutUpload()throws Exception{
  for(int state:new int[]{1,6}){
   byte[] r=status(state);Arrays.fill(r,16,20,(byte)0);Arrays.fill(r,48,84,(byte)0);
   assertEquals(BootstrapContinuation.RECHECK,BootstrapContinuation.inspect(r,image,journal()));
  }
 }
 @Test public void reverifyOnlyReadsAndNeverSendsCommitResetOrData()throws Exception{
  InstallJournal j=journal();final int[] count={0};
  InstallerTransport radio=new InstallerTransport(){byte[] r;
   public void send(byte[] f){assertEquals(0x42,f[5]&255);count[0]++;
    byte[] body=Arrays.copyOf(NdcpSession.words(0,3,7,image.length,image.length),52);
    System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(image)),0,body,20,32);
    r=NdcpSession.encode(0x42,(int)ByteCodec.u32le(f,8),body,1);}
   public byte[] receive(long t){return r;}public void close(){}
  };
  BootstrapContinuation.reverify(new NdcpSession(radio),j,s->{});
  assertEquals(1,count[0]);assertEquals("NDCP_VERIFIED",j.state());
 }
}
