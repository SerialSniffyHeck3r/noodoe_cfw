package io.opennoodoe.app.companion;
import android.content.*;
import android.graphics.*;
import android.media.*;
import android.media.session.*;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.annotation.*;
import java.io.*;
import java.lang.reflect.*;
import java.util.*;
import io.opennoodoe.app.protocol.ndcp.NdcpClient;
import static org.junit.Assert.*;

/** Real Android shaping/encoding and runtime publication, including interleaved
 * notification allocation. These masks also feed the bench capture fixture. */
@RunWith(RobolectricTestRunner.class) @Config(sdk=28) @GraphicsMode(GraphicsMode.Mode.NATIVE)
public class MusicVisualTest {
 @Test public void artworkIs480SquareAndBoundedBeforeAnyWireTransfer()throws Exception{
  Bitmap source=Bitmap.createBitmap(1600,900,Bitmap.Config.ARGB_8888);int[] row=new int[1600];Random random=new Random(42);
  for(int y=0;y<900;y++){for(int x=0;x<1600;x++)row[x]=0xff000000|random.nextInt(0xffffff);source.setPixels(row,0,1600,0,y,1600,1);}
  byte[] jpeg=PhoneVisualRenderer.artwork(source);assertTrue(jpeg.length<=49152);
  Bitmap decoded=BitmapFactory.decodeByteArray(jpeg,0,jpeg.length);assertNotNull(decoded);assertEquals(480,decoded.getWidth());assertEquals(480,decoded.getHeight());
  assertFalse(source.isRecycled());decoded.recycle();source.recycle();
 }
 @Test public void musicMasksKeepBottomGapAndCjkFits()throws Exception{
  CjkFonts.initialize(RuntimeEnvironment.getApplication());
  String[][] samples={{"Night Ride","Display Test Artist"},{"夜に駆ける","YOASOBI"},{"騎車去臺灣 · 夜行","音樂測試藝術家"}};
  File dir=new File("build/music-preview");assertTrue(dir.isDirectory()||dir.mkdirs());
  for(int sample=0;sample<samples.length;sample++){
   byte[] a=PhoneVisualRenderer.music(samples[sample][0],samples[sample][1]);assertEquals(10944,a.length);
   for(int y=68;y<72;y++)for(int x=0;x<152;x++)assertEquals(0,a[y*152+x]);
   int[] pixels=new int[304*72];for(int i=0;i<pixels.length;i++){int n=(a[i/2]>>(i%2==0?4:0))&15;pixels[i]=Color.rgb(n*17,n*17,n*17);}
   Bitmap b=Bitmap.createBitmap(pixels,304,72,Bitmap.Config.ARGB_8888);
   try(FileOutputStream out=new FileOutputStream(new File(dir,"music-"+sample+".png"))){b.compress(Bitmap.CompressFormat.PNG,100,out);}
   try(FileOutputStream out=new FileOutputStream(new File(dir,"music-"+sample+".a4"))){out.write(a);}b.recycle();
  }
 }
 static class Radio extends VisualTransferTest.Radio {
  List<Long> published=new ArrayList<>();
  @Override public void send(byte[] frame){if((frame[5]&255)==0x76){int start=20;int t=frame[start+12]&255,a=frame[start+13]&255;published.add(CompanionWire.u(frame,start+14+t+a));}super.send(frame);}
 }
 private void call(CompanionRuntime r,String method)throws Exception{Method m=CompanionRuntime.class.getDeclaredMethod(method);m.setAccessible(true);m.invoke(r);}
 @Test public void notificationPanelsDoNotChangePublishedMusicKey()throws Exception{
  Context context=RuntimeEnvironment.getApplication();Radio radio=new Radio();CompanionWire wire=new CompanionWire(new NdcpClient(radio));wire.epoch=42;wire.visuals=wire.phonePanels=true;
  MediaSession session=new MediaSession(context,"test");MediaController controller=new MediaController(context,session.getSessionToken());
  Shadows.shadowOf(controller).setPackageName("test.music");
  MediaMetadata first=new MediaMetadata.Builder().putString(MediaMetadata.METADATA_KEY_TITLE,"Night ride").putString(MediaMetadata.METADATA_KEY_ARTIST,"Artist").putLong(MediaMetadata.METADATA_KEY_DURATION,225000).build();
  Shadows.shadowOf(controller).setMetadata(first);
  Shadows.shadowOf((MediaSessionManager)context.getSystemService(Context.MEDIA_SESSION_SERVICE)).addController(controller);
  try(CompanionRuntime runtime=new CompanionRuntime(context,wire)){
   Shadows.shadowOf(android.os.Looper.getMainLooper()).idle();
   call(runtime,"sendMusic");long key=radio.published.get(0);assertNotEquals(0,key);
   call(runtime,"sendNotifications");call(runtime,"sendMusic");assertEquals(key,(long)radio.published.get(1));
   Bitmap cover=Bitmap.createBitmap(480,480,Bitmap.Config.ARGB_8888);cover.eraseColor(Color.RED);
   Shadows.shadowOf(controller).setMetadata(new MediaMetadata.Builder(first).putLong(MediaMetadata.METADATA_KEY_DURATION,226000).putBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART,cover).build());
   call(runtime,"sendMusic");assertEquals("Late cover/duration must keep the text key",key,(long)radio.published.get(2));
   Shadows.shadowOf(controller).setMetadata(new MediaMetadata.Builder(first).putString(MediaMetadata.METADATA_KEY_TITLE,"Next track").build());
   call(runtime,"sendMusic");assertNotEquals(key,(long)radio.published.get(3));
   call(runtime,"sendMusic");assertEquals(radio.published.get(3),radio.published.get(4));
   cover.recycle();
  }finally{session.release();}
 }
}
