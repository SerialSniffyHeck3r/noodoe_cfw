package io.opennoodoe.app.companion;
import android.app.*;
import android.content.*;
import android.graphics.*;
import android.os.*;
import android.service.notification.StatusBarNotification;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.annotation.*;
import java.io.*;
import java.util.*;
import static org.junit.Assert.*;
/** Execute the shipping Android Canvas/StaticLayout path, not a desktop imitation. */
@RunWith(RobolectricTestRunner.class) @Config(sdk=28) @GraphicsMode(GraphicsMode.Mode.NATIVE)
public class PhonePanelRendererTest {
 private Context c;
 @Before public void setup(){c=RuntimeEnvironment.getApplication();CjkFonts.initialize(c);c.getSharedPreferences("companion",0).edit().putStringSet("notification.apps",new HashSet<>(Arrays.asList(c.getPackageName()))).commit();}
 private void save(String name,byte[] a)throws Exception{
  assertTrue(a.length==18432||a.length==27648);int height=a.length/144;int[] pixels=new int[288*height];int nonzero=0;
  for(int i=0;i<pixels.length;i++){int n=(a[i/2]>>(i%2==0?4:0))&15;pixels[i]=Color.rgb(n*17,n*17,n*17);if(n!=0)nonzero++;}
  assertTrue(nonzero>100);Bitmap b=Bitmap.createBitmap(pixels,288,height,Bitmap.Config.ARGB_8888);
  File d=new File("build/phone-preview");assertTrue(d.isDirectory()||d.mkdirs());try(FileOutputStream o=new FileOutputStream(new File(d,name+".a4"))){o.write(a);}try(FileOutputStream o=new FileOutputStream(new File(d,name+".png"))){assertTrue(b.compress(Bitmap.CompressFormat.PNG,100,o));}b.recycle();
 }
 @Test public void emptyHeaderAndJapaneseTraditionalChineseFit()throws Exception{
  byte[] empty=PhonePanelRenderer.notification(c,"Pixel 9",87,null);for(int y=25;y<128;y++)for(int x=0;x<144;x++)assertEquals(0,empty[y*144+x]);save("empty",empty);
  NotificationHistory h=new NotificationHistory();h.activate(c);Notification n=new Notification.Builder(c,"test").setSmallIcon(android.R.drawable.ic_dialog_email).setContentTitle("台灣・日本のお知らせ").setContentText("騎乘平安！目的地に到着したら連絡してください。請小心慢行。").build();
  h.posted(c,new StatusBarNotification(c.getPackageName(),c.getPackageName(),7401,"noodoe.notification.test",android.os.Process.myUid(),0,0,n,android.os.Process.myUserHandle(),System.currentTimeMillis()));
  save("notification",PhonePanelRenderer.notification(c,"我的手機・携帯電話",87,h.snapshot(c).get(0)));
  byte[] roomy=PhonePanelRenderer.notification(c,"我的手機・携帯電話",87,h.snapshot(c).get(0),true);
  save("notification-roomy",roomy);
  byte[] gpu=PanelCodec.packRoomy(roomy),reconstructed=new byte[roomy.length];int at=0;
  int[] starts={0,34,84,132},heights={24,32,32,40};
  for(int i=0;i<4;i++){int count=heights[i]*144;System.arraycopy(gpu,at,reconstructed,starts[i]*144,count);at+=count;}
  assertArrayEquals("Gutter removal must not discard any rendered pixels",roomy,reconstructed);
  try(FileOutputStream o=new FileOutputStream(new File("build/phone-preview/notification-roomy-gpu.a4"))){o.write(gpu);}
  byte[] packed=PanelCodec.encode(roomy);assertTrue(packed.length<roomy.length/2);
  File dir=new File("build/phone-preview");try(FileOutputStream o=new FileOutputStream(new File(dir,"notification-roomy.rle"))){o.write(packed);}
  int iconInk=0;for(int y=34;y<70;y++)for(int x=0;x<36;x++)if(((roomy[y*144+x/2]>>(x%2==0?4:0))&15)!=0)iconInk++;
  assertTrue("App icon must contain pixels",iconInk>50);
  ReplySettings.save(c,new String[]{"稍後回覆","了解しました","騎車中，稍後聯絡","今から帰ります","감사합니다"},ReplySettings.DEFAULT_SIGNATURE);save("replies",PhonePanelRenderer.replies(new ReplySettings(c)));
 }
 @Test public void androidFallbackCoversNonHanRepertoireSymbols(){android.text.TextPaint p=new android.text.TextPaint();CjkFonts.apply(p,"臺灣",false);for(String symbol:new String[]{"\u02cd","\u203e","\u223c","\u2252"})assertTrue("Missing symbol "+symbol,p.hasGlyph(symbol));}
 @Test public void selectedCallAlwaysContainsNameNumberAndTime()throws Exception{
  List<CallFavorites.Entry> list=Arrays.asList(new CallFavorites.Entry("山田 美咲","010-1234-5678",1758720480000L,2),new CallFavorites.Entry("Other person","999999",1758720500000L,3));
  NoodoeCallService.View idle=new NoodoeCallService.View(0,0,0,"","");
  byte[] card=PhonePanelRenderer.calls(list,0,0,idle);save("recent-call",card);
  // Only the selected record is rendered; a neighbouring name/number must not leak.
  assertArrayEquals(card,PhonePanelRenderer.calls(list.subList(0,1),0,0,idle));
  for(int[] band:new int[][]{{30,73},{74,107},{108,128}}){int ink=0;for(int i=band[0]*144;i<band[1]*144;i++)if(card[i]!=0)ink++;assertTrue("Name, number and date each need visible pixels",ink>30);}
  byte[] changed=PhonePanelRenderer.calls(Arrays.asList(new CallFavorites.Entry("山田 美咲","010-0000-0000",1758720480000L,2)),0,0,idle);
  assertFalse(Arrays.equals(Arrays.copyOfRange(card,74*144,108*144),Arrays.copyOfRange(changed,74*144,108*144)));
 }
}
