package io.opennoodoe.app.companion;
import android.content.Context;
import android.graphics.*;
import android.graphics.drawable.Drawable;
import android.text.*;

/** Native-sized CJK ink, never magnified small glyphs. The notification title
 * stays close to the music title's 28px while fitting the sample notification.
 * Long text is ellipsized. Four packed strips use288x144
 * pixels; their transparent gutters are reconstructed by the receiver. */
final class LargePhonePanel {
 static final int[] HEIGHT={30,40,44,30},Y={0,40,90,148};
 static final int[] COMPACT_HEIGHT={20,28,36,60},COMPACT_Y={6,29,60,99};
 private static void text(Canvas c,String raw,int x,int y,int width,int height,int size,boolean bold){
  String value=raw==null?"":raw.replaceAll("[\\p{Cc}\\p{Zl}\\p{Zp}]"," ");
  TextPaint p=new TextPaint(Paint.ANTI_ALIAS_FLAG);p.setColor(Color.WHITE);p.setTextSize(size);CjkFonts.apply(p,value,bold);
  String line=TextUtils.ellipsize(value,p,width,TextUtils.TruncateAt.END).toString();
  // Position actual ink, not a font's large language-specific line spacing.
  Rect ink=new Rect();p.getTextBounds(line,0,line.length(),ink);
  c.save();c.clipRect(x,y,x+width,y+height);c.drawText(line,x,y+(height-ink.height())/2f-ink.top,p);c.restore();
 }
 private static void icon(Canvas c,Drawable image,int x,int y,int size){if(image!=null){image=image.mutate();image.setTint(Color.WHITE);image.setBounds(x,y,x+size,y+size);image.draw(c);}}
 private static void body(Canvas c,String raw,int y){
  String value=raw==null?"":raw.replaceAll("[\\p{Cc}\\p{Zl}\\p{Zp}]"," ");
  TextPaint p=new TextPaint(Paint.ANTI_ALIAS_FLAG);p.setColor(Color.WHITE);p.setTextSize(21);CjkFonts.apply(p,value,false);
  StaticLayout lines=StaticLayout.Builder.obtain(value,0,value.length(),p,288)
   .setIncludePad(false).setLineSpacing(-4f,1f).setMaxLines(2)
   .setEllipsize(TextUtils.TruncateAt.END).setEllipsizedWidth(288).build();
  c.save();c.clipRect(0,y,288,y+60);c.translate(0,y+(60-lines.getHeight())/2f);lines.draw(c);c.restore();
 }
 /** Layout 5 keeps the same 20.25KiB bank, but gives the body two readable
  * lines and removes the large gaps between the card's text rows. */
 static byte[] compactNotification(Context context,String device,int battery,NotificationHistory.Entry entry){
  Bitmap b=Bitmap.createBitmap(288,144,Bitmap.Config.ARGB_8888);Canvas c=new Canvas(b);
  icon(c,context.getDrawable(io.opennoodoe.app.R.drawable.ic_battery_round),0,0,20);
  text(c,battery<0?"--%":battery+"%",23,0,65,20,18,false);
  icon(c,context.getDrawable(io.opennoodoe.app.R.drawable.ic_phone_round),92,0,20);
  text(c,device,116,0,172,20,18,false);
  if(entry!=null){Drawable image=null;
   try{Context owner=context.createPackageContext(entry.source.getPackageName(),0);android.graphics.drawable.Icon source=entry.source.getNotification().getSmallIcon();if(source!=null)image=source.loadDrawable(owner);if(image==null)image=owner.getPackageManager().getApplicationIcon(entry.source.getPackageName());}catch(Exception unavailable){}
   if(image==null)image=context.getDrawable(android.R.drawable.ic_dialog_email);
   icon(c,image,0,20,28);text(c,entry.app,38,20,157,28,22,true);
   text(c,new java.text.SimpleDateFormat("HH:mm",java.util.Locale.getDefault()).format(new java.util.Date(entry.source.getPostTime())),204,20,84,28,18,false);
   text(c,entry.title,0,48,288,36,24,true);body(c,entry.body,84);
  }return finish(b);
 }
 static byte[] notification(Context context,String device,int battery,NotificationHistory.Entry entry){
  Bitmap b=Bitmap.createBitmap(288,144,Bitmap.Config.ARGB_8888);Canvas c=new Canvas(b);
  icon(c,context.getDrawable(io.opennoodoe.app.R.drawable.ic_battery_round),0,1,28);
  text(c,battery<0?"--%":battery+"%",30,0,69,30,20,false);
  icon(c,context.getDrawable(io.opennoodoe.app.R.drawable.ic_phone_round),104,1,28);
  text(c,device,139,0,149,30,20,false);
  if(entry!=null){Drawable image=null;
   try{Context owner=context.createPackageContext(entry.source.getPackageName(),0);android.graphics.drawable.Icon source=entry.source.getNotification().getSmallIcon();if(source!=null)image=source.loadDrawable(owner);if(image==null)image=owner.getPackageManager().getApplicationIcon(entry.source.getPackageName());}catch(Exception unavailable){}
   if(image==null)image=context.getDrawable(android.R.drawable.ic_dialog_email);
   icon(c,image,0,31,38);text(c,entry.app,48,30,149,40,24,true);
   text(c,new java.text.SimpleDateFormat("HH:mm",java.util.Locale.getDefault()).format(new java.util.Date(entry.source.getPostTime())),204,30,84,40,20,false);
   text(c,entry.title,0,70,288,44,26,true);text(c,entry.body,0,114,288,30,24,false);
  }return finish(b);
 }
 static byte[] replies(ReplySettings settings){
  // Six26px rows (five replies + Back) fit256x162 in the same20736-byte bank.
  Bitmap b=Bitmap.createBitmap(256,162,Bitmap.Config.ARGB_8888);Canvas c=new Canvas(b);int row=0;
  for(String message:settings.replies)text(c,message,8,row++*27,240,27,26,false);
  text(c,"Back",8,row*27,240,27,26,false);return finish(b);
 }
 private static byte[] finish(Bitmap b){int[] pixels=new int[b.getWidth()*b.getHeight()];b.getPixels(pixels,0,b.getWidth(),0,0,b.getWidth(),b.getHeight());b.recycle();return Alpha4.pack(pixels);}
}
