package io.opennoodoe.app.companion;
import android.graphics.*;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.annotation.*;
import java.io.*;
import java.util.*;
import static org.junit.Assert.*;
/** Actual shaped CJK bytes cross-checked by the MCU decoder harness. */
@RunWith(RobolectricTestRunner.class) @Config(sdk=28) @GraphicsMode(GraphicsMode.Mode.NATIVE)
public class MusicPackedTest {
 static byte[] decode(byte[] in){byte[] out=new byte[11184];System.arraycopy(in,0,out,0,16);int pos=16,at=16;
  while(at<in.length){int b=in[at++]&255,n=(b&127)+1;if((b&128)!=0)Arrays.fill(out,pos,pos+n,in[at++]);else{System.arraycopy(in,at,out,pos,n);at+=n;}pos+=n;}
  assertEquals(out.length,pos);return out;
 }
 @Test public void runsAreLosslessIncludingIncompressibleData(){
  for(int seed=0;seed<20;seed++){byte[] a=new byte[11184];if(seed>0)new Random(seed).nextBytes(a);assertArrayEquals(a,decode(PanelCodec.music(a)));}
 }
 @Test public void shapedTitleAndLargerArtistNeedFewerRadioChunks()throws Exception{
  CjkFonts.initialize(RuntimeEnvironment.getApplication());File dir=new File("build/music-preview");assertTrue(dir.isDirectory()||dir.mkdirs());
  String[][] text={{"Night Ride","Display Test Artist"},{"夜に駆ける","YOASOBI"},{"騎車去臺灣 · 夜行","音樂測試藝術家"}};
  for(int i=0;i<text.length;i++){
   byte[] raw=new MusicTextTiles(700,text[i][0],text[i][1]).tile(1,0),packed=PanelCodec.music(raw);
   assertArrayEquals(raw,decode(packed));assertTrue(packed.length<raw.length/2);
   System.out.println("music sample "+i+": "+raw.length+" -> "+packed.length+" bytes");
   if(i==2){try(FileOutputStream f=new FileOutputStream(new File(dir,"tile.raw"))){f.write(raw);}try(FileOutputStream f=new FileOutputStream(new File(dir,"tile.rle"))){f.write(packed);}}
   // Both rows have nonempty ink; fixed-size artist remains separated from the title.
   int artist=16+384*36/2,ink=0;for(int n=artist;n<raw.length;n++)if(raw[n]!=0)ink++;assertTrue(ink>30);
  }
 }
 @Test public void encoderPublishesLatestSnapshotAndNeverRecyclesPlayerBitmap()throws Exception{
  Bitmap old=Bitmap.createBitmap(1600,900,Bitmap.Config.ARGB_8888),latest=Bitmap.createBitmap(480,480,Bitmap.Config.ARGB_8888);old.eraseColor(Color.RED);latest.eraseColor(Color.BLUE);
  try(MediaArtEncoder worker=new MediaArtEncoder()){
   worker.request("old",old);worker.request("latest",latest);byte[] data=null;long end=System.nanoTime()+5_000_000_000L;
   while(data==null&&System.nanoTime()<end){data=worker.take("latest");if(data==null)Thread.sleep(5);}
   assertNotNull(data);assertNull(worker.take("old"));assertFalse(old.isRecycled());assertFalse(latest.isRecycled());
   Bitmap decoded=BitmapFactory.decodeByteArray(data,0,data.length);assertEquals(480,decoded.getWidth());int c=decoded.getPixel(240,240);assertTrue(Color.BLUE==c||Color.blue(c)>240&&Color.red(c)<10);decoded.recycle();
  }finally{old.recycle();latest.recycle();}
 }
}
