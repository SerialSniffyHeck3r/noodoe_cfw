package io.opennoodoe.app.installer;
import org.junit.Test;import static org.junit.Assert.*;
import java.util.Arrays;import java.security.MessageDigest;
import io.opennoodoe.app.protocol.ByteCodec;
public class ResourceTransferTest {
 @Test public void sendsUsedPagesAndRejectsCorruptedPayload()throws Exception{
  byte[] container=new byte[1048576];Arrays.fill(container,(byte)255);
  // RSC uses CMT1 (0x434D5431), unlike the journal's 0x31544D43.
  ByteCodec.putU32le(container,8,100);ByteCodec.putU32le(container,12,1);ByteCodec.putU32le(container,4092,0x434d5431);
  MessageDigest digest=MessageDigest.getInstance("SHA-256");digest.update(container,48,16);digest.update(container,4096,100);byte[] sha=digest.digest();System.arraycopy(sha,0,container,16,32);
  assertEquals(8192,ResourceUpdateSession.slot(container,sha).length);
  ByteCodec.putU32le(container,4092,0x31544d43);
  try{ResourceUpdateSession.slot(container,sha);fail("journal marker accepted as resource commit");}catch(java.io.IOException expected){}
  ByteCodec.putU32le(container,4092,0x434d5431);
  container[4100]^=1;try{ResourceUpdateSession.slot(container,sha);fail();}catch(java.io.IOException expected){}
 }
}
