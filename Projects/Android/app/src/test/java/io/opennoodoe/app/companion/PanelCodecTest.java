package io.opennoodoe.app.companion;
import org.junit.Test;
import java.util.*;
import static org.junit.Assert.*;
public class PanelCodecTest {
 static byte[] decode(byte[] in){
  assertTrue(in[1]==1||in[1]==2);byte[] out=new byte[(in[0]&255)*144];int at=2,pos=0;
  while(at<in.length){int b=in[at++]&255,n=(b&127)+1;
   if((b&128)!=0)Arrays.fill(out,pos,pos+n,in[at++]);else {System.arraycopy(in,at,out,pos,n);at+=n;}pos+=n;
  }assertEquals(out.length,pos);return out;
 }
 @Test public void losslessForAllByteValuesRunsAndNoise(){
  for(int height:new int[]{128,192})for(int seed=0;seed<10;seed++){
   byte[] original=new byte[height*144];if(seed>0)new Random(seed).nextBytes(original);
   for(int i=0;i<600;i++)original[i]=(byte)(i/128);assertArrayEquals(height==192?PanelCodec.packRoomy(original):original,decode(PanelCodec.encode(original)));
  }
 }
 @Test public void transparentMaskUsesLessThanOneChunk(){assertTrue(PanelCodec.encode(new byte[27648]).length<960);}
}
