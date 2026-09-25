package io.opennoodoe.app.companion;
import android.app.*;
import android.content.*;
import android.graphics.*;
import android.service.notification.StatusBarNotification;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.annotation.*;
import java.io.*;
import java.util.*;
import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28) @GraphicsMode(GraphicsMode.Mode.NATIVE)
public class LargePhonePanelTest {
 private Context c;
 @Before public void setup(){c=RuntimeEnvironment.getApplication();CjkFonts.initialize(c);}
 private NotificationHistory.Entry entry(String title,String body,long time){
  Notification n=new Notification.Builder(c,"test").setSmallIcon(android.R.drawable.ic_dialog_email).setContentTitle(title).setContentText(body).build();
  StatusBarNotification sb=new StatusBarNotification(c.getPackageName(),c.getPackageName(),7401,"test",android.os.Process.myUid(),0,0,n,android.os.Process.myUserHandle(),time);
  return new NotificationHistory.Entry(1,2,sb,"Discord",title,body,true);
 }
 private void save(String name,byte[] packed,int width,int height)throws Exception{save(name,packed,width,height,false);}
 private void save(String name,byte[] packed,int width,int height,boolean compact)throws Exception{
  assertEquals(width*height/2,packed.length);File dir=new File("build/phone-preview");assertTrue(dir.isDirectory()||dir.mkdirs());
  try(FileOutputStream f=new FileOutputStream(new File(dir,name+".a4"))){f.write(packed);}
  try(FileOutputStream f=new FileOutputStream(new File(dir,name+".rle"))){f.write(PanelCodec.encodeLarge(packed,width==256?4:compact?5:3));}
  int outHeight=width==256?height:178;int[] px=new int[width*outHeight];Arrays.fill(px,Color.rgb(16,20,24));
  for(int y=0;y<height;y++)for(int x=0;x<width;x++){
   int row=y;if(width==288){int[] heights=compact?LargePhonePanel.COMPACT_HEIGHT:LargePhonePanel.HEIGHT;
    int[] rows=compact?LargePhonePanel.COMPACT_Y:LargePhonePanel.Y;int top=0;
    for(int i=0;i<4;i++){if(y<top+heights[i]){row=rows[i]+y-top;break;}top+=heights[i];}}
   int nib=(packed[(y*width+x)/2]>>((x&1)==0?4:0))&15;px[row*width+x]=Color.rgb(16+nib*239/15,20+nib*235/15,24+nib*231/15);
  }
  Bitmap b=Bitmap.createBitmap(px,width,outHeight,Bitmap.Config.ARGB_8888);try(FileOutputStream f=new FileOutputStream(new File(dir,name+".png"))){assertTrue(b.compress(Bitmap.CompressFormat.PNG,100,f));}b.recycle();
 }
 @Test public void nativeLargeCjkAndTimestampHaveDistinctFixedBoxes()throws Exception{
  android.text.TextPaint samplePaint=new android.text.TextPaint();
  String sample="알림 테스트 · 通知テスト";CjkFonts.apply(samplePaint,sample,true);samplePaint.setTextSize(26);
  assertEquals("Sample title should fit without an ellipsis",sample,
   android.text.TextUtils.ellipsize(sample,samplePaint,288,android.text.TextUtils.TruncateAt.END).toString());
  TimeZone before=TimeZone.getDefault();TimeZone.setDefault(TimeZone.getTimeZone("Asia/Seoul"));
  try{long date=1758762480000L;
   byte[] a=LargePhonePanel.notification(c,"S24 Ultra",87,entry("山田 美咲","到着したら連絡してね。",date));
   save("notification-large",a,288,144);
   save("notification-sample",LargePhonePanel.notification(c,"S24 Ultra",87,
    entry(sample,"누도에 잘 도착했나요? 臺灣 · 日本 · 한국",date)),288,144);
   byte[] b=LargePhonePanel.notification(c,"S24 Ultra",87,entry("山田 美咲","到着したら連絡してね。",date+60000));
   boolean timeChanged=false;for(int y=30;y<70;y++)for(int x=102;x<144;x++)if(a[y*144+x]!=b[y*144+x])timeChanged=true;
   assertTrue("Arrival time must change only the time field",timeChanged);
   for(int y=70;y<144;y++)for(int x=0;x<144;x++)assertEquals(a[y*144+x],b[y*144+x]);
   byte[] empty=LargePhonePanel.notification(c,"S24 Ultra",87,null);for(int i=30*144;i<empty.length;i++)assertEquals(0,empty[i]);save("header-large",empty,288,144);
   ReplySettings.save(c,new String[]{"稍後回覆","了解しました","安全運転中です","今から帰ります","감사합니다"},ReplySettings.DEFAULT_SIGNATURE);save("replies-large",LargePhonePanel.replies(new ReplySettings(c)),256,162);
  }finally{TimeZone.setDefault(before);}
 }
 @Test public void compactNotificationKeepsSampleTitleAndTwoBodyLines()throws Exception{
  String title="알림 테스트 · 通知テスト",message="누도에 잘 도착했나요? 臺灣 · 日本 · 한국";
  android.text.TextPaint titlePaint=new android.text.TextPaint();CjkFonts.apply(titlePaint,title,true);titlePaint.setTextSize(24);
  assertEquals(title,android.text.TextUtils.ellipsize(title,titlePaint,288,android.text.TextUtils.TruncateAt.END).toString());
  android.text.TextPaint bodyPaint=new android.text.TextPaint();CjkFonts.apply(bodyPaint,message,false);bodyPaint.setTextSize(21);
  android.text.StaticLayout body=android.text.StaticLayout.Builder.obtain(message,0,message.length(),bodyPaint,288)
   .setIncludePad(false).setMaxLines(2).setEllipsize(android.text.TextUtils.TruncateAt.END).setEllipsizedWidth(288).build();
  assertEquals(2,body.getLineCount());assertEquals(0,body.getEllipsisCount(1));assertTrue(body.getHeight()<=60);
  byte[] pixels=LargePhonePanel.compactNotification(c,"S24 Ultra",87,entry(title,message,1758762480000L));
  save("notification-compact-sample",pixels,288,144,true);
  save("notification-compact-cjk",LargePhonePanel.compactNotification(c,"S24 Ultra",87,
   entry("山田 美咲","到着したら連絡してね。",1758762480000L)),288,144,true);
  byte[] empty=LargePhonePanel.compactNotification(c,"S24 Ultra",87,null);
  for(int i=20*144;i<empty.length;i++)assertEquals(0,empty[i]);
  save("header-compact",empty,288,144,true);
 }
}
