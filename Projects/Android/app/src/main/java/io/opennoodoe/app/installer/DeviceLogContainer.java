package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
/** The UID-bound identity is immutable; the other63 sectors start erased. */
public final class DeviceLogContainer {
 private DeviceLogContainer(){}
 public static byte[] create(long[] uid){
  if(uid==null||uid.length!=3)throw new IllegalArgumentException("UID");
  byte[] file=BootstrapFatPlan.filled(0x40000),header=BootstrapFatPlan.filled(4096);
  long[] words={0x31494c4e,1,0x40000,64,uid[0],uid[1],uid[2]};
  for(int i=0;i<words.length;i++)ByteCodec.putU32le(header,4*i,words[i]);
  ByteCodec.putU32le(header,4088,NdcpSession.crc(header,4088));ByteCodec.putU32le(header,4092,0x31544d43);
  System.arraycopy(header,0,file,0x3f000,4096);return file;
 }
}
