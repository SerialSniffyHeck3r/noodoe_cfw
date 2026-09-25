package io.opennoodoe.app.companion;
import android.content.Context;
import android.graphics.*;
import androidx.exifinterface.media.ExifInterface;
import android.net.Uri;
import android.os.SystemClock;
import io.opennoodoe.app.diagnostics.SessionLog;
import java.io.*;
import io.opennoodoe.app.installer.StockUpdateSession.Progress;
/** Bounded phone decode, then the same verified visual transport as music.
 * Image bytes and the selected URI never enter the diagnostic log. */
public final class PhotoUpload {
 private PhotoUpload(){}
 public static byte[] prepare(Context c,Uri uri)throws IOException {
  BitmapFactory.Options o=new BitmapFactory.Options();o.inJustDecodeBounds=true;
  try(InputStream in=c.getContentResolver().openInputStream(uri)){BitmapFactory.decodeStream(in,null,o);}
  if(o.outWidth<=0||o.outHeight<=0||o.outWidth>50000||o.outHeight>50000)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0674,"사진 크기를 읽지 못했어요."));
  o.inSampleSize=1;while(o.outWidth/o.inSampleSize>960||o.outHeight/o.inSampleSize>960)o.inSampleSize*=2;
  o.inJustDecodeBounds=false;o.inPreferredConfig=Bitmap.Config.ARGB_8888;Bitmap image;
  try(InputStream in=c.getContentResolver().openInputStream(uri)){image=BitmapFactory.decodeStream(in,null,o);}
  if(image==null)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0675,"이 사진은 열 수 없어요."));
  try{
   int orientation=1;try(InputStream in=c.getContentResolver().openInputStream(uri)){orientation=new ExifInterface(in).getAttributeInt(ExifInterface.TAG_ORIENTATION,1);}catch(IOException ignored){}
   Matrix m=new Matrix();switch(orientation){case 2:m.setScale(-1,1);break;case 3:m.setRotate(180);break;case 4:m.setScale(1,-1);break;case 5:m.setRotate(90);m.postScale(-1,1);break;case 6:m.setRotate(90);break;case 7:m.setRotate(-90);m.postScale(-1,1);break;case 8:m.setRotate(-90);break;}
   if(!m.isIdentity()){Bitmap upright=Bitmap.createBitmap(image,0,0,image.getWidth(),image.getHeight(),m,true);if(upright!=image){image.recycle();image=upright;}}
   return PhoneVisualRenderer.jpeg(image);
  }finally{image.recycle();}
 }
 public static void send(CompanionWire wire,int slot,byte[] jpeg,SessionLog log,Progress progress)throws Exception {
  if(slot<0||slot>2||!wire.visuals)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0676,"사진 전송을 지원하는 새 CFW가 필요해요."));
  long key=(SystemClock.elapsedRealtime()&0xffffffffL);if(key==0)key=1;
  // fsync completes before BEGIN/FINISH can authorize a persistent change.
  log.append("photo_replace_intent",SessionLog.fields("opcode","121","epoch",Long.toString(wire.epoch),"transaction",Long.toString(key),"length",Integer.toString(jpeg.length),"offset",Integer.toString(slot)));
  VisualTransfer transfer=new VisualTransfer(wire,slot+3,key,jpeg);
  boolean done=false;
  while(!done){wire.poll();done=transfer.pump(SystemClock.elapsedRealtime());
   progress.update(transfer.received()==transfer.total()?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0677,"사진을 기기에서 검증하고 저장하고 있어요…"):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0678,"사진 전송 · ")+transfer.received()+" / "+transfer.total()+" bytes");
   if(!done)Thread.sleep(20);
  }
  log.append("photo_replace_saved",SessionLog.fields("transaction",Long.toString(key),"result","0"));
 }
}
