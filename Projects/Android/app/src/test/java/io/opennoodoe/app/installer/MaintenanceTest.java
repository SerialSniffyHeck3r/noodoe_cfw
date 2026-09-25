package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import io.opennoodoe.app.protocol.ndcp.*;
import io.opennoodoe.app.transport.SppTransport;
import org.junit.Test;
import static org.junit.Assert.*;
import java.io.*;
import java.util.*;

public final class MaintenanceTest {
 @Test public void bluetoothTestNeverAuthorizesInstall()throws Exception{
  byte[] b=NdcpSession.words(0,1,9,0,0,0,0,5,120000);
  MaintenanceStatus status=new MaintenanceStatus(b);
  assertFalse(status.installAllowed());assertTrue(status.message().contains("블루투스 테스트"));
  ByteCodec.putU32le(b,8,11);assertFalse(new MaintenanceStatus(b).installAllowed());
  assertTrue(new MaintenanceStatus(b).message().contains("조도"));
  ByteCodec.putU32le(b,8,12);try{new MaintenanceStatus(b);fail();}catch(IOException expected){}
 }
 private byte[] app(){byte[] b=new byte[0x60000];Arrays.fill(b,(byte)255);
  ByteCodec.putU32le(b,0,0x2002ff00);ByteCodec.putU32le(b,4,0x08020101);
  ByteCodec.putU32le(b,0x200,0x51534352);ByteCodec.putU32le(b,0x204,1);ByteCodec.putU32le(b,0x208,1);ByteCodec.putU32le(b,0x230,0x3250554e);ByteCodec.putU32le(b,0x234,2);ByteCodec.putU32le(b,0x238,2);ByteCodec.putU32le(b,0x23c,0x60000);return b;}
 @Test public void exactGatePythonGoldenBytes(){
  // Independently generated using firmware tools/gate_bundle.py, UID1,2,3.
  assertEquals("ceb8d255b6a9855b03b4a5f96cf019ac64e6afa3fb39e093addca73518229445",RecoveryBundle.sha(GateContainers.image(app(),new long[]{1,2,3},0,0x10006)));
  assertEquals("ab3487e57ecb55d6937c37d9924e52f7fff5297f03d2d20cfa2a07312accdca9",RecoveryBundle.sha(GateContainers.image(app(),new long[]{1,2,3},1,0x10006)));
  assertEquals("2e57be8462988a373c075a369ad2ead529325be1e189db1ba2ef68c45a1b35b1",RecoveryBundle.sha(GateContainers.journal(app(),new long[]{1,2,3})));
  assertEquals("adaa20d5e90bc2b61ac53cc38bf80089b87314c95d8f4df8a27fec77ed2f5904",RecoveryBundle.sha(DeviceLogContainer.create(new long[]{1,2,3})));
 }
 @Test public void layoutOneAndBadProductRejected()throws Exception{
  byte[] b=new byte[0x70000];ByteCodec.putU32le(b,0,0x2002ff00);ByteCodec.putU32le(b,4,0x08010101);
  try{GateContainers.product(b);fail();}catch(IOException expected){}
  System.arraycopy(app(),0,b,0x10000,0x60000);assertArrayEquals(app(),GateContainers.product(b));
  ByteCodec.putU32le(b,0x10004,0x08010101);
  try{GateContainers.product(b);fail();}catch(IOException expected){}
 }
 @Test public void statusIsNotAnInstallReceipt()throws Exception{
  byte[] b=NdcpSession.words(0,1,4,100,458752,458752,0,1,30000);
  assertFalse(new MaintenanceStatus(b).installAllowed());
  ByteCodec.putU32le(b,28,5);assertTrue(new MaintenanceStatus(b).installAllowed());
  ByteCodec.putU32le(b,8,5);assertFalse(new MaintenanceStatus(b).installAllowed());
  ByteCodec.putU32le(b,4,2);try{new MaintenanceStatus(b);fail();}catch(IOException expected){}
 }
 private static final class Radio implements SppTransport {
  final Deque<byte[]> queue=new ArrayDeque<>();int mode;int writes;
  public void send(byte[] request){writes++;int seq=(int)ByteCodec.u32le(request,8);
   byte[] good=NdcpClient.encode(0x5a,seq,NdcpSession.words(0,1,1,0,0,0,0,1,1000),1);
   if(mode==1)good[good.length-1]^=1;
   if(mode==2){queue.add(NdcpClient.encode(0x5a,seq,NdcpSession.words(2),3));return;}
   queue.add(NdcpClient.encode(0x5a,seq-1,NdcpSession.words(0),1));
   for(int i=0;i<good.length;i+=3)queue.add(Arrays.copyOfRange(good,i,Math.min(i+3,good.length)));
  }
  public byte[] receive(long timeout)throws IOException{if(queue.isEmpty())throw new IOException("link lost");return queue.removeFirst();}
  public void close(){}
 }
 @Test public void fragmentsAndOldSequenceAreHandled()throws Exception{
  Radio r=new Radio();MaintenanceStatus s=new MaintenanceStatus(new NdcpClient(r).request(0x5a,new byte[0]));assertEquals(1,s.state);assertEquals(1,r.writes);
 }
 @Test public void corruptionNeverRetriesAMutation()throws Exception{
  Radio r=new Radio();r.mode=1;try{new NdcpClient(r).request(0x5a,new byte[0]);fail();}catch(IOException expected){assertTrue(expected.getMessage().contains("CRC"));}assertEquals(1,r.writes);
 }
 @Test public void rejectionKeepsMachineCode()throws Exception{
  Radio r=new Radio();r.mode=2;try{new NdcpClient(r).request(0x5a,new byte[0]);fail();}catch(NdcpClient.DeviceRejected e){assertEquals(2,e.result);assertEquals(0x5a,e.opcode);}
 }
 @Test public void ridingCommandsNeverOpenRadio()throws Exception{
  InstallerController c=new InstallerController(new File("unused"));
  for(String action:new String[]{"update-cfw","cfw-verify","stage-cfw","commit-cfw","reset-cfw"})
   try{c.run(action,"","",()->{fail("radio must stay closed");return null;},s->{},b->{});fail();}catch(IOException expected){}
 }
 @Test public void resumeNeverIgnoresChangesInProtectedPrefix()throws Exception{
  File a=File.createTempFile("prefix-a",".bin"),b=File.createTempFile("prefix-b",".bin");
  try{
   java.nio.file.Files.write(a.toPath(),new byte[]{1,2,3,4});
   java.nio.file.Files.write(b.toPath(),new byte[]{1,2,3,9});
   assertTrue(BootstrapProvisioner.equalPrefix(a,b,3));
   assertFalse(BootstrapProvisioner.equalPrefix(a,b,4));
   assertFalse(BootstrapProvisioner.equalPrefix(a,b,5));
   assertFalse(BootstrapProvisioner.equalPrefix(a,b,-1));
   java.nio.file.Files.write(b.toPath(),new byte[]{9,2,3,4});
   assertFalse(BootstrapProvisioner.equalPrefix(a,b,3));
  }finally{a.delete();b.delete();}
 }
}
