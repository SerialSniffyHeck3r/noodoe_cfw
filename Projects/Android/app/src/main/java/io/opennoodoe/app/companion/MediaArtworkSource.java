package io.opennoodoe.app.companion;

import android.content.Context;
import android.graphics.*;
import android.media.MediaMetadata;
import android.net.Uri;
import android.os.SystemClock;
import java.io.*;
import java.util.concurrent.*;

/** Some players publish only an artwork URI. Decode off the radio owner so
 * a slow content provider cannot stop GPS, controls or the Bluetooth watchdog.
 * One in-flight load and one cached bitmap bound memory and pending work. */
final class MediaArtworkSource implements AutoCloseable {
 private final Context context;private final Runnable changed;
 private final ExecutorService worker=Executors.newSingleThreadExecutor(r->{Thread t=new Thread(r,"media-art");t.setDaemon(true);return t;});
 private Future<Bitmap> pending;private Bitmap cached;
 private Bitmap hardwareSource,software;private int sourceGeneration;
 private String requested="",cachedKey="";private long retryAt;
 MediaArtworkSource(Context context){this(context,()->{});}
 MediaArtworkSource(Context context,Runnable changed){this.context=context;this.changed=changed;}
 Bitmap get(MediaMetadata metadata,String track){
  Bitmap direct=metadata.getBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART);
  if(direct==null)direct=metadata.getBitmap(MediaMetadata.METADATA_KEY_ART);
  if(direct==null)direct=metadata.getBitmap(MediaMetadata.METADATA_KEY_DISPLAY_ICON);
  if(direct!=null){
   // Android hardware bitmaps cannot be read with getPixels or painted by a
   // software Canvas. Own only this copy; never recycle the player's bitmap.
   if(android.os.Build.VERSION.SDK_INT<26||direct.getConfig()!=Bitmap.Config.HARDWARE)return direct;
   if(hardwareSource==direct&&sourceGeneration==direct.getGenerationId())return software;
   if(software!=null){software.recycle();software=null;}hardwareSource=direct;sourceGeneration=direct.getGenerationId();
   if((long)direct.getWidth()*direct.getHeight()>4_000_000)return null;
   try{software=direct.copy(Bitmap.Config.ARGB_8888,false);}catch(RuntimeException unavailable){return null;}
   return software;
  }
  String uri=metadata.getString(MediaMetadata.METADATA_KEY_ALBUM_ART_URI);
  if(uri==null)uri=metadata.getString(MediaMetadata.METADATA_KEY_ART_URI);
  if(uri==null)uri=metadata.getString(MediaMetadata.METADATA_KEY_DISPLAY_ICON_URI);
  if(uri==null)return null;
  Uri source=Uri.parse(uri);
  // No arbitrary network requests on behalf of untrusted media metadata.
  if(!"content".equals(source.getScheme())&&!"android.resource".equals(source.getScheme()))return null;
  String key=track+":"+uri;
  if(pending!=null&&pending.isDone()){
   try{Bitmap next=pending.get();if(cached!=null)cached.recycle();cached=next;cachedKey=requested;}
   catch(Exception unavailable){if(cached!=null)cached.recycle();cached=null;cachedKey=requested;}
   pending=null;retryAt=SystemClock.elapsedRealtime()+5000;
  }
  if(key.equals(cachedKey)&&cached!=null)return cached;
  if(pending==null&&(!key.equals(requested)||SystemClock.elapsedRealtime()>=retryAt)){
   requested=key;FutureTask<Bitmap> task=new FutureTask<Bitmap>(()->load(source)){@Override protected void done(){changed.run();}};pending=task;worker.execute(task);
  }
  return null; // A previous track's completion never becomes this track's art.
 }
 private Bitmap load(Uri source){
  try(InputStream in=context.getContentResolver().openInputStream(source)){
   if(in==null)return null;
   ByteArrayOutputStream out=new ByteArrayOutputStream();byte[] chunk=new byte[8192];int n;
   while((n=in.read(chunk))!=-1){if(Thread.currentThread().isInterrupted()||out.size()+n>4*1024*1024)return null;out.write(chunk,0,n);}
   byte[] bytes=out.toByteArray();BitmapFactory.Options o=new BitmapFactory.Options();o.inJustDecodeBounds=true;
   BitmapFactory.decodeByteArray(bytes,0,bytes.length,o);if(o.outWidth<=0||o.outHeight<=0)return null;
   o.inSampleSize=1;while(Math.max(o.outWidth,o.outHeight)/o.inSampleSize>960)o.inSampleSize*=2;
   o.inJustDecodeBounds=false;o.inPreferredConfig=Bitmap.Config.ARGB_8888;
   return BitmapFactory.decodeByteArray(bytes,0,bytes.length,o);
  }catch(IOException|RuntimeException unavailable){return null;}
 }
 @Override public void close(){if(pending!=null)pending.cancel(true);worker.shutdownNow();if(cached!=null){cached.recycle();cached=null;}if(software!=null){software.recycle();software=null;}hardwareSource=null;}
}
