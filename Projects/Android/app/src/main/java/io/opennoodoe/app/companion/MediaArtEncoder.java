package io.opennoodoe.app.companion;
import android.graphics.*;
import java.io.*;
import java.util.concurrent.*;
/** One worker + one latest pending480px snapshot. No socket or UI ownership. */
final class MediaArtEncoder implements AutoCloseable {
 private final ExecutorService worker=Executors.newSingleThreadExecutor(r->{Thread t=new Thread(r,"media-jpeg");t.setDaemon(true);return t;});
 private final Runnable changed;
 MediaArtEncoder(){this(()->{});}
 MediaArtEncoder(Runnable changed){this.changed=changed;}
 private String wanted="",readyKey="";private Bitmap pending;private byte[] ready;
 private boolean running,closed;
 synchronized void request(String key,Bitmap source){
  if(closed||key.equals(wanted))return;
  // Snapshot before returning: URI cache/player bitmap may be recycled later.
  Bitmap b=PhoneVisualRenderer.square(source);
  wanted=key;ready=null;readyKey="";
  if(pending!=null)pending.recycle();pending=b;
  if(!running){running=true;worker.execute(this::drain);}
 }
 private void drain(){
  for(;;){Bitmap b;String key;
   synchronized(this){if(closed||pending==null){running=false;return;}b=pending;pending=null;key=wanted;}
   byte[] result=null;try{result=PhoneVisualRenderer.artwork(b);}catch(IOException|RuntimeException unavailable){}finally{b.recycle();}
   synchronized(this){if(!closed&&key.equals(wanted)){ready=result;readyKey=key;if(result==null)wanted="";}}
   changed.run();
  }
 }
 synchronized byte[] take(String key){if(!key.equals(readyKey))return null;byte[] b=ready;ready=null;return b;}
 synchronized boolean busy(){return running||pending!=null;}
 @Override public synchronized void close(){closed=true;if(pending!=null){pending.recycle();pending=null;}ready=null;worker.shutdownNow();}
}
