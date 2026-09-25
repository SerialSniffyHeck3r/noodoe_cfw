package io.opennoodoe.app.companion;
import java.io.ByteArrayOutputStream;

/** Lossless byte runs over the A4 mask. The receiver negotiates this format;
 * old firmware still receives the original 288x128 mask (kind 6). */
public final class PanelCodec {
 private PanelCodec(){}
 public static byte[] encode(byte[] mask){
  int height=mask.length/144;
  if(mask.length!=144*height||(height!=128&&height!=192))throw new IllegalArgumentException();
  int layout=height==192?2:1;
  if(layout==2)mask=packRoomy(mask);
  ByteArrayOutputStream out=new ByteArrayOutputStream();out.write(128);out.write(layout);
  runs(out,mask,0);return out.toByteArray();
 }
 public static byte[] encodeLarge(byte[] mask,int layout){
  if(mask.length!=20736||(layout!=3&&layout!=4&&layout!=5))throw new IllegalArgumentException();
  ByteArrayOutputStream out=new ByteArrayOutputStream();out.write(144);out.write(layout);runs(out,mask,0);return out.toByteArray();
 }
 public static byte[] music(byte[] tile){
  if(tile.length!=11184)throw new IllegalArgumentException();
  ByteArrayOutputStream out=new ByteArrayOutputStream();out.write(tile,0,16);
  runs(out,tile,16);return out.toByteArray();
 }
 private static void runs(ByteArrayOutputStream out,byte[] mask,int offset){
  for(int i=offset;i<mask.length;){
   int n=run(mask,i);
   if(n>=3){out.write(128|(n-1));out.write(mask[i]);i+=n;}
   else{int start=i;i+=n;while(i<mask.length&&i-start<128&&run(mask,i)<3)i+=Math.min(run(mask,i),128-(i-start));out.write(i-start-1);out.write(mask,start,i-start);}
  }
 }
 /** Omit only the fixed transparent gutters. Four unchanged A4 strips still
  * fit the existing GPU page bank; the UI restores their screen positions. */
 static byte[] packRoomy(byte[] mask){
  byte[] packed=new byte[18432];int at=0;
  int[] starts={0,34,84,132},heights={24,32,32,40};
  for(int i=0;i<4;i++){int bytes=heights[i]*144;System.arraycopy(mask,starts[i]*144,packed,at,bytes);at+=bytes;}
  return packed;
 }
 private static int run(byte[] b,int i){int n=1;while(n<128&&i+n<b.length&&b[i+n]==b[i])n++;return n;}
 public static VisualTransfer transfer(CompanionWire wire,long key,byte[] mask){return new VisualTransfer(wire,wire.packedPanels?8:6,key,wire.packedPanels?encode(mask):mask);}
}
