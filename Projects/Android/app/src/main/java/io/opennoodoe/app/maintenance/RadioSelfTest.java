package io.opennoodoe.app.maintenance;
import io.opennoodoe.app.protocol.ndcp.NdcpClient;
import io.opennoodoe.app.companion.CompanionWire;
import io.opennoodoe.app.diagnostics.SessionLog;
import java.io.IOException;
import java.security.SecureRandom;
import io.opennoodoe.app.installer.StockUpdateSession.Progress;
import java.util.zip.CRC32;

/** Shared Bootstrap/Product 8 KiB echo. No flash, controller reset or bond change. */
public final class RadioSelfTest {
 private RadioSelfTest(){}
 public static void run(NdcpClient client,Progress progress,SessionLog log)throws IOException{
  long nonce=(new SecureRandom().nextInt()&0xffffffffL)|1L;
  byte[] start=client.request(0x92,CompanionWire.words(1,nonce));
  if(start.length!=28||CompanionWire.u(start,4)!=1||CompanionWire.u(start,8)!=0)throw new IOException("Radio test start response differs");
  CRC32 crc=new CRC32();
  for(int offset=0;offset<8192;offset+=512){
   byte[] request=new byte[524];System.arraycopy(CompanionWire.words(2,nonce,offset),0,request,0,12);
   for(int i=0;i<512;i++)request[12+i]=(byte)((offset+i)*73+nonce);
   byte[] reply=client.request(0x92,request);
   if(reply.length!=516)throw new IOException("Radio echo length differs at "+offset);
   for(int i=0;i<512;i++)if(reply[4+i]!=request[12+i])throw new IOException("Radio echo differs at "+(offset+i));
   crc.update(request,12,512);progress.update(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0274,"Bluetooth 왕복 검사 ")+(offset+512)+" / 8192 B");
  }
  byte[] result=client.request(0x92,CompanionWire.words(3,nonce));
  if(result.length!=28||CompanionWire.u(result,4)!=1||CompanionWire.u(result,8)!=8192||CompanionWire.u(result,12)!=crc.getValue()||CompanionWire.u(result,20)!=1||CompanionWire.u(result,24)!=0)throw new IOException("Radio final CRC/result differs");
  long elapsed=CompanionWire.u(result,16);
  log.append("radio_test_verified",SessionLog.fields("bytes","8192","crc",Long.toHexString(crc.getValue()),"device_ms",Long.toString(elapsed)));
  progress.update(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0275,"Bluetooth 8 KiB 송수신·CRC 일치 확인 / ")+elapsed+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0276," ms. 플래시는 변경하지 않았어요."));
 }
}
