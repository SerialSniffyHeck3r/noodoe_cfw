package io.opennoodoe.app.companion;
import android.graphics.*;
import android.text.*;
import java.text.Normalizer;

/** Shaping uses fixed28px bold/24px regular fonts. Only one384px title tile
 * is rasterized per request; even a long CJK title never allocates a strip. */
public final class MusicTextTiles {
 public static final int TILE=384,TITLE_H=36,ARTIST_H=28,VIEW=304;
 public final long key;public final int width;
 private final StaticLayout title;
 private final byte[] artist;
 private final float titleX;
 private static String clean(String value){
  String s=Normalizer.normalize(value==null?"":value,Normalizer.Form.NFC).replaceAll("[\\p{Cc}\\p{Zl}\\p{Zp}]"," ");
  return s.substring(0,s.offsetByCodePoints(0,Math.min(1024,s.codePointCount(0,s.length()))));
 }
 private static TextPaint paint(String text,int size,boolean bold,int alpha){
  TextPaint p=new TextPaint(Paint.ANTI_ALIAS_FLAG|Paint.SUBPIXEL_TEXT_FLAG);
  CjkFonts.apply(p,text,bold);p.setColor(Color.WHITE);p.setAlpha(alpha);p.setTextSize(size);return p;
 }
 private static StaticLayout layout(String text,TextPaint paint,int width){
  return StaticLayout.Builder.obtain(text,0,text.length(),paint,width).setIncludePad(false)
   .setMaxLines(1).setEllipsize(TextUtils.TruncateAt.END).setEllipsizedWidth(width)
   .setAlignment(Layout.Alignment.ALIGN_NORMAL).build();
 }
 private static byte[] pack(Bitmap b){int[] p=new int[b.getWidth()*b.getHeight()];b.getPixels(p,0,b.getWidth(),0,0,b.getWidth(),b.getHeight());b.recycle();return Alpha4.pack(p);}
 public MusicTextTiles(long key,String value,String performer){
  if(key==0)throw new IllegalArgumentException();this.key=key;String text=clean(value);
  TextPaint p=paint(text,28,true,255);int ink=(int)Math.ceil(Layout.getDesiredWidth(text,p));
  width=Math.max(VIEW,Math.min(32768,ink+8));title=layout(text,p,width-8);
  titleX=width==VIEW?4+(VIEW-8-ink)/2f:4;
  String name=clean(performer);TextPaint a=paint(name,24,false,215);StaticLayout row=layout(name,a,VIEW-8);
  Bitmap b=Bitmap.createBitmap(VIEW,ARTIST_H,Bitmap.Config.ARGB_8888);Canvas c=new Canvas(b);
  c.translate(4+Math.max(0,(VIEW-8-row.getLineWidth(0))/2f),(ARTIST_H-row.getHeight())/2f);row.draw(c);artist=pack(b);
 }
 public byte[] tile(long view,int index){
  if(view==0||index<0||index>=(width+TILE-1)/TILE)throw new IllegalArgumentException();
  Bitmap b=Bitmap.createBitmap(TILE,TITLE_H,Bitmap.Config.ARGB_8888);Canvas c=new Canvas(b);
  c.translate(titleX-index*TILE,(TITLE_H-title.getHeight())/2f);title.draw(c);byte[] pixels=pack(b);
  byte[] out=new byte[16+pixels.length+artist.length];System.arraycopy(CompanionWire.words(key,view,index,1),0,out,0,16);
  System.arraycopy(pixels,0,out,16,pixels.length);System.arraycopy(artist,0,out,16+pixels.length,artist.length);return out;
 }
}
