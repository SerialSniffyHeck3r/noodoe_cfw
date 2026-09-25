package io.opennoodoe.app.companion;
import android.content.Context;
import android.graphics.*;
import android.graphics.drawable.Drawable;
import android.text.*;
import java.util.*;
/** Bounded 288x128/192 A4 pages, never an unbounded text texture. The top line is
 * device status; app/title/body follow with a fixed readable hierarchy. */
public final class PhonePanelRenderer {
 private static void text(Canvas c,String value,int x,int y,int w,int h,int size,boolean bold,int lines){
  value=value==null?"":value.replaceAll("[\\p{Cc}\\p{Zl}\\p{Zp}]"," ");if(value.length()>2048)value=value.substring(0,2048);
  TextPaint p=new TextPaint(Paint.ANTI_ALIAS_FLAG);p.setColor(Color.WHITE);p.setTextSize(size);CjkFonts.apply(p,value,bold);
  StaticLayout l;
  // CJK ascenders/descenders differ from Latin metrics. Fit the actual layout
  // height too, so a second line or glyph bottom is never cut at the mask edge.
  do{p.setTextSize(size);l=StaticLayout.Builder.obtain(value,0,value.length(),p,w).setIncludePad(false).setMaxLines(lines).setEllipsize(TextUtils.TruncateAt.END).setEllipsizedWidth(w).build();if(l.getHeight()<=h||size<=12)break;size--;}while(true);
  c.save();c.clipRect(x,y,x+w,y+h);c.translate(x,y);l.draw(c);c.restore();
 }
 private static void icon(Context context,Canvas c,int resource,int x,int y,int size){Drawable d=context.getDrawable(resource);if(d!=null){d.setTint(Color.WHITE);d.setBounds(x,y,x+size,y+size);d.draw(c);}}
 private static byte[] finish(Bitmap b){int[] p=new int[288*b.getHeight()];b.getPixels(p,0,288,0,0,288,b.getHeight());b.recycle();return Alpha4.pack(p);}
 public static byte[] notification(Context context,String device,int battery,NotificationHistory.Entry entry){
  return notification(context,device,battery,entry,false);
 }
 public static byte[] notification(Context context,String device,int battery,NotificationHistory.Entry entry,boolean roomy){
  if(!roomy||entry==null)return legacyNotification(context,device,battery,entry);
  Bitmap b=Bitmap.createBitmap(288,192,Bitmap.Config.ARGB_8888);Canvas c=new Canvas(b);
  status(context,c,device,battery);
  notificationIcon(context,c,entry,0,34,32);
  text(c,entry.app,44,34,244,32,24,true,1);
  text(c,entry.title,0,84,288,32,28,true,1);
  text(c,entry.body,0,132,288,40,20,false,2);
  return finish(b);
 }
 private static void status(Context context,Canvas c,String device,int battery){
  icon(context,c,io.opennoodoe.app.R.drawable.ic_battery_round,0,0,22);text(c,battery<0?"--%":battery+"%",25,0,60,24,18,false,1);
  icon(context,c,io.opennoodoe.app.R.drawable.ic_phone_round,91,0,22);text(c,device,117,0,171,24,18,false,1);
 }
 private static void notificationIcon(Context context,Canvas c,NotificationHistory.Entry entry,int x,int y,int size){
  Drawable d=null;
  try{android.graphics.drawable.Icon source=entry.source.getNotification().getSmallIcon();
   Context owner=context.createPackageContext(entry.source.getPackageName(),0);
   if(source!=null)d=source.loadDrawable(owner);
   if(d==null)d=owner.getPackageManager().getApplicationIcon(entry.source.getPackageName());
  }catch(Exception unavailable){/* Drawable fallback below; no personal data log. */}
  if(d==null)d=context.getDrawable(android.R.drawable.ic_dialog_email);
  if(d!=null){d=d.mutate();d.setTint(Color.WHITE);d.setBounds(x,y,x+size,y+size);d.draw(c);}
 }
 private static byte[] legacyNotification(Context context,String device,int battery,NotificationHistory.Entry entry){
  Bitmap b=Bitmap.createBitmap(288,128,Bitmap.Config.ARGB_8888);Canvas c=new Canvas(b);
  icon(context,c,io.opennoodoe.app.R.drawable.ic_battery_round,0,0,22);text(c,battery<0?"--%":battery+"%",25,0,60,24,18,false,1);
  icon(context,c,io.opennoodoe.app.R.drawable.ic_phone_round,91,0,22);text(c,device,117,0,171,24,18,false,1);
  if(entry!=null){notificationIcon(context,c,entry,0,25,22);
   text(c,entry.app,29,25,259,22,18,false,1);text(c,entry.title,0,48,288,31,26,true,1);text(c,entry.body,0,80,288,48,18,false,2);
  }return finish(b);
 }
 public static byte[] replies(ReplySettings settings){Bitmap b=Bitmap.createBitmap(288,128,Bitmap.Config.ARGB_8888);Canvas c=new Canvas(b);int i=0;
  for(String s:settings.replies)text(c,s,8,21*i++,272,21,17,false,1);text(c,"Back",8,21*i,272,21,17,false,1);return finish(b);}
 /** The device draws the category above this panel. Neighbor rows stay
  * subordinate; selected names keep a generous CJK-safe two-line area. */
 /** One selected call per page. Separate fixed boxes retain the number even
  * when a contact resolves; no shrinking for adjacent rows. A4 size is unchanged. */
 public static byte[] calls(List<CallFavorites.Entry> entries,int favorites,int selected,NoodoeCallService.View call){
  Bitmap b=Bitmap.createBitmap(288,128,Bitmap.Config.ARGB_8888);Canvas c=new Canvas(b);
  String name="",number="",time="";
  if(call.state!=0){name=call.name;number=call.number;}
  else if(selected>=0&&selected<entries.size()){
   CallFavorites.Entry e=entries.get(selected);name=e.name;number=e.number;
   if(e.date>0)time=new java.text.SimpleDateFormat("MM/dd  HH:mm",Locale.getDefault()).format(new Date(e.date));
  }
  centered(c,name.isEmpty()?"Unknown caller":name,30,43,36,true);
  centered(c,number.isEmpty()?"Private number":number,74,33,28,false);
  centered(c,time,108,20,18,false);
  return finish(b);
 }
 private static void centered(Canvas c,String value,int y,int height,int size,boolean bold){
  value=value.replaceAll("[\\p{Cc}\\p{Zl}\\p{Zp}]"," ");if(value.length()>2048)value=value.substring(0,2048);
  TextPaint p=new TextPaint(Paint.ANTI_ALIAS_FLAG);p.setColor(Color.WHITE);p.setTextSize(size);CjkFonts.apply(p,value,bold);
  StaticLayout l=StaticLayout.Builder.obtain(value,0,value.length(),p,288).setIncludePad(false).setAlignment(Layout.Alignment.ALIGN_CENTER).setMaxLines(1).setEllipsize(TextUtils.TruncateAt.END).setEllipsizedWidth(288).build();
  c.save();c.clipRect(0,y,288,y+height);c.translate(0,y);l.draw(c);c.restore();
 }
 private static String label(CallFavorites.Entry e){return e.name.isEmpty()?e.number:e.name;}
}
