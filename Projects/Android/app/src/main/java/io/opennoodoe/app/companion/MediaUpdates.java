package io.opennoodoe.app.companion;
import android.content.*;
import android.media.MediaMetadata;
import android.media.session.*;
import android.os.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;
/** Android callbacks only mark a revision/wake the socket owner. No SPP calls
 * on the callback thread; bounded subscriptions survive Activity destruction. */
final class MediaUpdates implements AutoCloseable {
 private final MediaSessionManager manager;private final ComponentName component;private final Runnable wake;
 private final Handler handler=new Handler(Looper.getMainLooper());private final AtomicLong revision=new AtomicLong();
 private final ArrayList<MediaController> controllers=new ArrayList<>();private volatile MediaController selected;
 private volatile boolean closed;private boolean listening;private long refreshAt;
 private final MediaController.Callback callback=new MediaController.Callback(){
  public void onMetadataChanged(MediaMetadata m){changed();}
  public void onPlaybackStateChanged(PlaybackState p){select();changed();}
  public void onSessionDestroyed(){refresh();changed();}
 };
 private final MediaSessionManager.OnActiveSessionsChangedListener sessions=this::replace;
 MediaUpdates(Context c,Runnable wake){manager=c.getSystemService(MediaSessionManager.class);component=new ComponentName(c,CompanionNotifications.class);this.wake=wake;handler.post(this::refresh);}
 private void changed(){revision.incrementAndGet();wake.run();}
 long version(){return revision.get();}
 MediaController current(){return selected;}
 void check(long now){if(now>=refreshAt){refreshAt=now+2000;handler.post(this::refresh);}}
 private void select(){
  MediaController before=selected;selected=null;
  // Multiple active players can reorder the system list. A still-playing
  // selected session must not alternate the title/art on every status read.
  for(MediaController c:controllers){PlaybackState p=c.getPlaybackState();if(before!=null&&before.getSessionToken().equals(c.getSessionToken())&&p!=null&&p.getState()==PlaybackState.STATE_PLAYING){selected=c;return;}}
  for(MediaController c:controllers){if(selected==null)selected=c;PlaybackState p=c.getPlaybackState();if(p!=null&&p.getState()==PlaybackState.STATE_PLAYING){selected=c;break;}}
 }
 private void replace(List<MediaController> next){
  if(closed)return;List<MediaController> list=next==null?Collections.emptyList():next.subList(0,Math.min(8,next.size()));
  boolean same=list.size()==controllers.size();for(int i=0;same&&i<list.size();i++)same=list.get(i).getSessionToken().equals(controllers.get(i).getSessionToken());
  if(same)return;
  for(MediaController c:controllers)c.unregisterCallback(callback);controllers.clear();
  for(MediaController c:list){c.registerCallback(callback,handler);controllers.add(c);}select();changed();
 }
 private void refresh(){if(closed||manager==null)return;try{if(!listening){manager.addOnActiveSessionsChangedListener(sessions,component,handler);listening=true;}replace(manager.getActiveSessions(component));}catch(SecurityException revoked){replace(Collections.emptyList());if(listening){manager.removeOnActiveSessionsChangedListener(sessions);listening=false;}}}
 @Override public void close(){closed=true;handler.post(()->{for(MediaController c:controllers)c.unregisterCallback(callback);controllers.clear();selected=null;if(listening)manager.removeOnActiveSessionsChangedListener(sessions);listening=false;});}
}
