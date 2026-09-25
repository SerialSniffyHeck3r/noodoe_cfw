package io.opennoodoe.app.companion;
import android.graphics.*;
import android.text.*;
import java.io.*;
import java.text.Normalizer;

/** Phone owns shaping/fallback/CJK. The device receives fixed geometry, never
 * TTF, scrolling labels or font-dependent metrics. Inputs are not logged. */
public final class PhoneVisualRenderer {
 /** Android may return a new Bitmap object for the same metadata. Identity or
  * generationId would restart transfers forever; compare sampled content. */
 public static long artSignature(Bitmap art){
  if(art==null)return 0;Bitmap small=Bitmap.createScaledBitmap(art,16,16,true);
  int[] pixels=new int[256];small.getPixels(pixels,0,16,0,0,16,16);
  if(small!=art)small.recycle();long hash=0xcbf29ce484222325L;
  for(int pixel:pixels)hash=(hash^(pixel&0xffffffffL))*0x100000001b3L;
  return hash;
 }
 public static final int WIDTH=304,HEIGHT=72;
 private PhoneVisualRenderer(){}
 private static String clean(String value){if(value==null)return "";String n=Normalizer.normalize(value,Normalizer.Form.NFC).replaceAll("[\\p{Cc}\\p{Zl}\\p{Zp}]"," ");return n.length()>2048?n.substring(0,n.offsetByCodePoints(0,Math.min(1024,n.codePointCount(0,n.length())))):n;}
 private static void line(Canvas canvas,String text,int top,int height,int max,int min,boolean bold,int alpha){
  text=clean(text);TextPaint paint=new TextPaint(Paint.ANTI_ALIAS_FLAG|Paint.SUBPIXEL_TEXT_FLAG);
  CjkFonts.apply(paint,text,bold);paint.setColor(Color.WHITE);paint.setAlpha(alpha);
  int size=max;for(;size>min;size--){paint.setTextSize(size);if(Layout.getDesiredWidth(text,paint)<=WIDTH-8)break;}paint.setTextSize(size);
  StaticLayout layout=StaticLayout.Builder.obtain(text,0,text.length(),paint,WIDTH-8)
   .setAlignment(Layout.Alignment.ALIGN_CENTER).setIncludePad(false).setMaxLines(1)
   .setEllipsize(TextUtils.TruncateAt.END).setEllipsizedWidth(WIDTH-8).build();
  canvas.save();canvas.clipRect(0,top,WIDTH,top+height);canvas.translate(4,top+(height-layout.getHeight())/2f);layout.draw(canvas);canvas.restore();
 }
 public static byte[] music(String title,String artist){
  Bitmap bitmap=Bitmap.createBitmap(WIDTH,HEIGHT,Bitmap.Config.ARGB_8888);Canvas canvas=new Canvas(bitmap);
  line(canvas,title,0,34,28,28,true,255);line(canvas,artist,36,32,24,24,false,215);
  int[] pixels=new int[WIDTH*HEIGHT];bitmap.getPixels(pixels,0,WIDTH,0,0,WIDTH,HEIGHT);bitmap.recycle();
  return Alpha4.pack(pixels);
 }
 /** Center-crop square artwork, bounded baseline JPEG. Same encoder serves
  * persistent photo slots; no extracted stock pictures enter the APK. */
 public static byte[] jpeg(Bitmap source)throws IOException{return encodeJpeg(source,131072,88);}
 /** Live artwork trades excess JPEG bytes for latency, retaining the full
  * 480x480 canvas. Persistent photos keep their original quality budget. */
 public static byte[] artwork(Bitmap source)throws IOException{return encodeJpeg(source,49152,80);}
 private static byte[] encodeJpeg(Bitmap source,int limit,int firstQuality)throws IOException{
  if(source==null||source.getWidth()<1||source.getHeight()<1)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0671,"이미지를 읽을 수 없어요."));
  Bitmap square=square(source);
  try{for(int quality=firstQuality;quality>=8;quality-=8){ByteArrayOutputStream out=new ByteArrayOutputStream();if(!square.compress(Bitmap.CompressFormat.JPEG,quality,out))throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0672,"JPEG 변환에 실패했어요."));if(out.size()<=limit)return out.toByteArray();}}
  finally{square.recycle();}throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0673,"이미지 크기를 줄일 수 없어요."));
 }
 static Bitmap square(Bitmap source){
  Bitmap square=Bitmap.createBitmap(480,480,Bitmap.Config.ARGB_8888);Canvas c=new Canvas(square);
  int side=Math.min(source.getWidth(),source.getHeight()),x=(source.getWidth()-side)/2,y=(source.getHeight()-side)/2;
  c.drawBitmap(source,new Rect(x,y,x+side,y+side),new Rect(0,0,480,480),new Paint(Paint.FILTER_BITMAP_FLAG));
  return square;
 }
}
