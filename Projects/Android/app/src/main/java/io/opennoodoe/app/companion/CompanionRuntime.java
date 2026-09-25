package io.opennoodoe.app.companion;
import android.Manifest;
import android.app.Notification;
import android.content.*;
import android.content.pm.PackageManager;
import android.graphics.*;
import android.location.*;
import android.media.MediaMetadata;
import android.media.session.*;
import android.os.*;
import android.service.notification.StatusBarNotification;
import io.opennoodoe.app.protocol.ndcp.NdcpClient;
import java.io.*;
import java.util.*;
import java.util.concurrent.*;
/** Android adapters: no socket owner, NOR writes, UI controls or content logs. */
public final class CompanionRuntime implements AutoCloseable {
 public interface LocationForeground {boolean ready();}
 private final Context context;private final CompanionWire wire;
 private final CompanionSignal signal=new CompanionSignal();private final PhoneLocationFeed gps;private final MediaUpdates media;private final LocationForeground locationForeground;
 private long mediaRevision=-1,mediaWork=-1,missingMediaSince=-1,trackChanges,artRequests;
 private final Handler main=new Handler(Looper.getMainLooper());
 private volatile boolean closed;private long sentLocation;
 private String artKey="";
 private volatile MediaController player;
 private final MediaArtworkSource artwork;
 private final MediaArtEncoder artEncoder=new MediaArtEncoder(signal::changed);
 private long lastMusic=-200,lastStatus=-200;
 private final PhoneCalls calls;
 private VisualTransfer pendingCallVisual;
 private boolean activeCallVisual,preferPanel;
 private final NotificationDelivery notificationDelivery;
 private MusicTextTiles musicTiles;private long tilePoll=-250,tileView,tileWaitUntil;
 private final ArrayDeque<VisualTransfer> visualQueue=new ArrayDeque<>();private VisualTransfer activeVisual;
 private String visualIdentity="";private long musicVisualKey;private long visualKey=(SystemClock.elapsedRealtime() & 0xffffffffL),locationSince,rideSession;
 private static final long indicatorSource=(new java.security.SecureRandom().nextInt()&0xffffffffL)|1L;
 private long lastContent=-1000;
 private String deviceName;
 private long nextVisualKey(){visualKey=(visualKey+1)&0xffffffffL;if(visualKey==0)visualKey=1;return visualKey;}
 public boolean hasPendingVisuals(){return SystemClock.elapsedRealtime()<tileWaitUntil||artEncoder.busy()||activeVisual!=null||!visualQueue.isEmpty()||notificationDelivery.pending()||pendingCallVisual!=null;}
 public CompanionRuntime(Context context,CompanionWire wire){this(context,wire,null,()->true);}
 public CompanionRuntime(Context context,CompanionWire wire,String address){this(context,wire,address,()->true);}
 public CompanionRuntime(Context context,CompanionWire wire,String address,LocationForeground locationForeground){this.context=context;this.wire=wire;notificationDelivery=new NotificationDelivery(context,wire,this::nextVisualKey);this.locationForeground=locationForeground;gps=new PhoneLocationFeed(context,signal::changed);media=new MediaUpdates(context,signal::changed);artwork=new MediaArtworkSource(context,signal::changed);calls=new PhoneCalls(context,wire,this::nextVisualKey,address);CjkFonts.initialize(context);deviceName=context.getSharedPreferences("companion",0).getString("device.name",Build.MODEL);wire.client.setRequestHandler(this::mediaCommand);}
 public long signalVersion(){return signal.version();}
 public void awaitChange(long observed,long millis)throws InterruptedException{signal.await(observed,millis);}
 public Map<String,String> gpsDiagnostics(){Map<String,String> d=gps.diagnostics();d.put("media_events",Long.toString(media.version()));d.put("track_changes",Long.toString(trackChanges));d.put("art_requests",Long.toString(artRequests));return d;}
 public String gpsNotice(){switch(gps.status()){case "FOREGROUND_REQUIRED":return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0648," · GPS: 앱을 열고 위치 권한 확인");case "PERMISSION":return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0649," · GPS: 정확한 위치 권한 필요");case "POWER_RESTRICTED":return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0650," · GPS: 휴대폰 절전 모드 제한");case "LOCATION_DISABLED":return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0651," · GPS: 휴대폰 위치 켜기");case "STALE":return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0652," · GPS 수신 대기");default:return "";}}
 public void tick()throws IOException {
  // Control/IGN state is refreshed at5Hz, not once per image chunk. Repeating
  // a status round trip for every960bytes needlessly serialized the radio.
  long tick=SystemClock.elapsedRealtime();if(tick-lastStatus>=200){wire.poll();lastStatus=tick;}
  if(rideSession!=wire.rideSession){rideSession=wire.rideSession;sentLocation=0;locationSince=SystemClock.elapsedRealtimeNanos();}
  gps.update(wire.ign,wire.rideSession,locationForeground.ready());sendLocation();
  long now=SystemClock.elapsedRealtime();boolean refresh=now-lastContent>=1000;
  if(refresh){lastContent=now;VisualTransfer call=calls.tick(now);if(call!=null)pendingCallVisual=call;}
  media.check(now);long revision=media.version(),work=signal.version();
  if(revision!=mediaRevision||work!=mediaWork||now-lastMusic>=200){mediaRevision=revision;mediaWork=work;lastMusic=now;sendMusic();}
  pollMusicTiles(now);
  // Reorder only transactions which have not sent BEGIN. Once accepted, a
  // bounded transfer finishes instead of restarting when a tile arrives.
  if(!visualQueue.isEmpty()&&activeVisual!=null&&!activeCallVisual&&activeVisual.pauseForMusic()){
   activeVisual=null; // Delivery retains its not-yet-started immutable job.
  }
  if(activeVisual==null){activeCallVisual=pendingCallVisual!=null;if(activeCallVisual){activeVisual=pendingCallVisual;pendingCallVisual=null;}else {boolean panel=notificationDelivery.pending()&&(preferPanel||visualQueue.isEmpty());activeVisual=panel?notificationDelivery.next():visualQueue.poll();preferPanel=!panel;}}
  if(activeVisual!=null&&activeVisual.pump(SystemClock.elapsedRealtime())){notificationDelivery.completed(activeVisual);activeVisual=null;activeCallVisual=false;}
  if(!refresh)return;
  sendNotifications();
  Intent battery=context.registerReceiver(null,new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
  if(battery!=null){int scale=battery.getIntExtra(BatteryManager.EXTRA_SCALE,100),level=battery.getIntExtra(BatteryManager.EXTRA_LEVEL,-1);if(level>=0&&scale>0)wire.send(0x78,CompanionWire.words(Math.min(100,level*100/scale),battery.getIntExtra(BatteryManager.EXTRA_PLUGGED,0)!=0?1:0));}
 }
 private void sendLocation()throws IOException{Location l=gps.latest();if(!wire.ign||l==null||l.getElapsedRealtimeNanos()<locationSince||l.getElapsedRealtimeNanos()==sentLocation||SystemClock.elapsedRealtimeNanos()-l.getElapsedRealtimeNanos()>5_000_000_000L)return;
  byte[] data=PhoneGpsEncoder.encode(l);
  wire.client.request(0x09,data);sentLocation=l.getElapsedRealtimeNanos();
 }
 private void sendNotifications()throws IOException {
  CompanionNotifications bridge=CompanionNotifications.instance;
  List<NotificationHistory.Entry> entries=Collections.emptyList();
  if(bridge!=null){try{bridge.history.refresh(context,bridge.getActiveNotifications());}catch(SecurityException revoked){bridge.history.deactivate();}catch(RuntimeException unavailable){}entries=bridge.history.snapshot(context);}
  ReplySettings settings=new ReplySettings(context);Intent battery=context.registerReceiver(null,new IntentFilter(Intent.ACTION_BATTERY_CHANGED));int percent=-1;
  if(battery!=null){int scale=battery.getIntExtra(BatteryManager.EXTRA_SCALE,100),level=battery.getIntExtra(BatteryManager.EXTRA_LEVEL,-1);if(scale>0&&level>=0)percent=Math.min(100,level*100/scale);}
  notificationDelivery.refresh(bridge==null?null:bridge.history,entries,deviceName,percent,settings);
  // Count/read acknowledgements refer to complete visible content, never keys
  // still uploading. Header battery updates do not regenerate ten text masks.
  wire.indicators(indicatorSource,gps.enabled()&&wire.ign,notificationDelivery.visible(),bridge==null?null:bridge.history);
 }

 private void sendMusic()throws IOException {
  player=media.current();
  MediaMetadata m=player==null?null:player.getMetadata();
  // Players briefly publish null metadata during a session handoff. Do not
  // blank and reanimate the dashboard between two snapshots of the same song.
  if(player==null||m==null){long now=SystemClock.elapsedRealtime();if(missingMediaSince<0)missingMediaSince=now;if(now-missingMediaSince<1500)return;}
  else missingMediaSince=-1;
  if(player==null){wire.music(false,false,0,0,"","");artKey="";visualIdentity="";visualQueue.clear();musicTiles=null;return;}
  PlaybackState p=player.getPlaybackState();if(m==null){wire.music(false,false,0,0,"","");visualIdentity="";visualQueue.clear();musicTiles=null;return;}
  long duration=Math.max(0,m.getLong(MediaMetadata.METADATA_KEY_DURATION)),position=p==null?0:Math.max(0,p.getPosition());if(p!=null&&p.getState()==PlaybackState.STATE_PLAYING&&p.getLastPositionUpdateTime()>0)position+=Math.max(0,(long)((SystemClock.elapsedRealtime()-p.getLastPositionUpdateTime())*p.getPlaybackSpeed()));if(duration>0)position=Math.min(position,duration);
  String title=m.getString(MediaMetadata.METADATA_KEY_TITLE),artist=m.getString(MediaMetadata.METADATA_KEY_ARTIST);
  String key=player.getPackageName()+":"+title+":"+artist;
  // Text identity does not include duration or asynchronously arriving art.
  // Late cover art must not cancel/reupload text or restart the page animation.
  // MEDIA_ID is optional and can oscillate null/non-null for one song. Text
  // pixels depend on the displayed strings; artwork has its own content hash.
  Bitmap art=artwork.get(m,key);
  String imageKey=key+":"+PhoneVisualRenderer.artSignature(art);
  if(wire.visuals){
   if(!key.equals(visualIdentity)){
    visualIdentity=key;musicVisualKey=nextVisualKey();trackChanges++;artKey="";visualQueue.clear();
    if(activeVisual!=null&&activeVisual.isMusic())activeVisual=null;
    if(wire.marquee){musicTiles=new MusicTextTiles(musicVisualKey,title,artist);tilePoll=-250;tileWaitUntil=SystemClock.elapsedRealtime()+1000;}
    else visualQueue.add(new VisualTransfer(wire,1,musicVisualKey,PhoneVisualRenderer.music(title,artist)));
   }
   // Publish metadata before any image work. Pixel encoding cannot hold the
   // title, current track key, playback clock or control queue hostage.
   wire.music(true,p!=null&&p.getState()==PlaybackState.STATE_PLAYING,position,duration,title,artist,musicVisualKey);
   if(art!=null&&!imageKey.equals(artKey)){
    artEncoder.request(imageKey,art);byte[] jpeg=artEncoder.take(imageKey);
    if(jpeg==null)return;
    for(Iterator<VisualTransfer> it=visualQueue.iterator();it.hasNext();)if(it.next().isArt())it.remove();
    if(activeVisual!=null&&activeVisual.isArt())activeVisual=null;
    visualQueue.add(new VisualTransfer(wire,2,musicVisualKey,jpeg));artKey=imageKey;artRequests++;
   }
   return;
  }
  wire.music(true,p!=null&&p.getState()==PlaybackState.STATE_PLAYING,position,duration,title,artist);
  if(art!=null&&!imageKey.equals(artKey)){Bitmap scaled=Bitmap.createScaledBitmap(art,32,32,true);int[] pixels=new int[1024];scaled.getPixels(pixels,0,32,0,0,32,32);if(scaled!=art)scaled.recycle();byte[] bytes=new byte[2048];for(int i=0;i<1024;i++){int rgb=pixels[i],v=((rgb>>19)&31)<<11|((rgb>>10)&63)<<5|((rgb>>3)&31);bytes[i*2]=(byte)v;bytes[i*2+1]=(byte)(v>>8);}for(int offset=0;offset<2048;offset+=512){byte[] chunk=new byte[516];System.arraycopy(CompanionWire.words(offset),0,chunk,0,4);System.arraycopy(bytes,offset,chunk,4,512);wire.send(0x77,chunk);}artKey=imageKey;}
 }
 /** Device owns cursor/page generation. Prefetch exactly the two requested
  * tiles; metadata/control are serviced between bounded binary chunks. */
 private void pollMusicTiles(long now)throws IOException{
  if(!wire.marquee||now-tilePoll<(now<tileWaitUntil?50:250))return;tilePoll=now;
  long key=musicTiles==null?0:musicTiles.key;
  byte[] r=wire.send(0x95,CompanionWire.words(key,musicTiles==null?0:musicTiles.width));
  if(r.length!=32||CompanionWire.u(r,4)!=1||CompanionWire.u(r,8)!=key||CompanionWire.u(r,16)>1||CompanionWire.u(r,28)>3)throw new IOException("Music tile state differs");
  long view=CompanionWire.u(r,12);boolean on=CompanionWire.u(r,16)==1;
  for(Iterator<VisualTransfer> it=visualQueue.iterator();it.hasNext();){VisualTransfer v=it.next();if(v.isTile()&&(!on||!v.tileMatches(key,view,-1)))it.remove();}
  if(activeVisual!=null&&activeVisual.isTile()&&(!on||!activeVisual.tileMatches(key,view,-1))){activeVisual=null;activeCallVisual=false;}
  tileView=view;if(on&&CompanionWire.u(r,28)==3)tileWaitUntil=0;if(!on||musicTiles==null)return;
  for(int i=0;i<2;i++)if((CompanionWire.u(r,28)&(1<<i))==0){
   long index=CompanionWire.u(r,20+4*i);
   if(index>=(musicTiles.width+383)/384)throw new IOException("Music tile index differs");
   boolean queued=activeVisual!=null&&activeVisual.tileMatches(key,view,index);
   for(VisualTransfer v:visualQueue)queued|=v.tileMatches(key,view,index);if(queued)continue;
   if(activeVisual!=null&&activeVisual.pauseForTile()){visualQueue.addLast(activeVisual);activeVisual=null;}
   visualQueue.addFirst(new VisualTransfer(wire,wire.packedMusic?9:7,nextVisualKey(),wire.packedMusic?PanelCodec.music(musicTiles.tile(view,(int)index)):musicTiles.tile(view,(int)index)));break;
  }
 }
 private long mediaCommand(int op,byte[] data){
  if(op==0x12)return wire.calls?calls.command(data):1;
  if(op==0x11){if(data.length!=20||CompanionWire.u(data,0)!=wire.epoch||!wire.ign||wire.speed>50)return 7;
   ReplySettings r=new ReplySettings(context);long index=CompanionWire.u(data,16);CompanionNotifications n=CompanionNotifications.instance;
   if(n==null||r.revision!=CompanionWire.u(data,12)||index>=r.replies.size())return 7;
   try{return notificationDelivery.reply(CompanionWire.u(data,4),CompanionWire.u(data,8),r.message((int)index),n.getActiveNotifications());}
   catch(RuntimeException unavailable){return 7;}
  }
  if(op!=0x10||data.length!=4||player==null)return 7;long action=CompanionWire.u(data,0);if(action>2)return 1;
  MediaController current=player;PlaybackState before=current.getPlaybackState();boolean playing=before!=null&&before.getState()==PlaybackState.STATE_PLAYING;CountDownLatch changed=new CountDownLatch(1);
  MediaController.Callback callback=new MediaController.Callback(){public void onPlaybackStateChanged(PlaybackState state){if(action==0&&state!=null&&(state.getState()==PlaybackState.STATE_PLAYING)!=playing)changed.countDown();}public void onMetadataChanged(MediaMetadata m){if(action!=0)changed.countDown();}};
  try{current.registerCallback(callback,main);MediaController.TransportControls c=current.getTransportControls();if(action==0){if(playing)c.pause();else c.play();}else if(action==1)c.skipToPrevious();else c.skipToNext();return changed.await(1200,TimeUnit.MILLISECONDS)?0:7;}catch(Exception e){return 7;}finally{current.unregisterCallback(callback);}
 }
 @Override public void close(){closed=true;gps.close();media.close();artEncoder.close();artwork.close();player=null;wire.client.setRequestHandler(null);signal.changed();}
}
