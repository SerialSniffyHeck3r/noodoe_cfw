package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import java.io.IOException;
import java.util.Arrays;
/** Canonical GIM1/GID1/GBJ1 encoding, identical to firmware gate_abi.h.
 * Each container is UID-bound only after the live Bootstrap identity check. */
public final class GateContainers {
 private static byte[] blank(int n){byte[] b=new byte[n];Arrays.fill(b,(byte)255);return b;}
 private static void put(byte[] b,int at,long... v){for(long x:v){ByteCodec.putU32le(b,at,x);at+=4;}}
 private static byte[] seal(byte[] b){put(b,4088,NdcpSession.crc(b,4088),0x31544d43);return b;}
 public static byte[] product(byte[] combined)throws IOException{
  if(combined.length!=0x70000)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0450,"Gate와 CFW가 함께 든 설치 파일이 필요해요."));
  long sp=ByteCodec.u32le(combined,0),pc=ByteCodec.u32le(combined,4);
  if(sp!=0x2002ff00L||(pc&1)==0||pc<0x08010000L||pc>=0x08020000L)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0451,"리커버리 시작 주소가 맞지 않아요."));
  byte[] app=Arrays.copyOfRange(combined,0x10000,0x70000);sp=ByteCodec.u32le(app,0);pc=ByteCodec.u32le(app,4);
  if(sp!=0x2002ff00L||(pc&1)==0||pc<0x08020000L||pc>=0x08080000L||ByteCodec.u32le(app,0x200)!=0x51534352||ByteCodec.u32le(app,0x204)!=1||ByteCodec.u32le(app,0x208)!=1)
   throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0452,"CFW 주소나 자산 정보가 맞지 않아요."));
  if(ByteCodec.u32le(app,0x230)!=0x3250554eL||ByteCodec.u32le(app,0x234)!=2||ByteCodec.u32le(app,0x238)!=2||ByteCodec.u32le(app,0x23c)!=0x60000)
   throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0453,"자동 복구 확인을 지원하는 CFW가 필요해요."));
  return app;
 }
 public static byte[] image(byte[] app,long[] uid,int slot,long version){
  if(app.length!=0x60000||uid.length!=3||slot<0||slot>1)throw new IllegalArgumentException("Gate bounds");
  byte[] b=blank(0x80000),h=blank(4096);put(h,0,0x314d4947,1,4096,0x80000,uid[0],uid[1],uid[2],0x08020000,0x60000,1);
  System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(app)),0,h,40,32);System.arraycopy(app,0x200,h,72,44);put(h,116,1,version,0);
  System.arraycopy(seal(h),0,b,0,4096);System.arraycopy(app,0,b,4096,app.length);
  h=blank(4096);put(h,0,0x31444947,1,slot,0x80000,uid[0],uid[1],uid[2],1);System.arraycopy(seal(h),0,b,0x7f000,4096);return b;
 }
 public static byte[] journal(byte[] app,long[] uid){return journal(app,uid,false);}
 public static byte[] journal(byte[] app,long[] uid,boolean restore){
  byte[] b=blank(0x10000),h=blank(4096);put(h,0,0x314a4247,2,1,1,0,0xffffffffL,0,0);
  System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(app)),0,h,32,32);Arrays.fill(h,64,96,(byte)0);
  put(h,96,uid[0],uid[1],uid[2],1,0,1);put(h,120,1,0xffffffffL,0);Arrays.fill(h,132,164,(byte)0);put(h,164,0,0,0,0,0);
  // Existing Bootstrap/Gate pass these opaque v2 fields unchanged.
  if(restore)put(h,176,0x52535452L,0x52535452L);
  System.arraycopy(seal(h),0,b,0,4096);return b;
 }
}
