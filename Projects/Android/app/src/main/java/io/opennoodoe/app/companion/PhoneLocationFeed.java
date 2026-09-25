package io.opennoodoe.app.companion;
import android.Manifest;
import android.content.*;
import android.content.pm.PackageManager;
import android.location.*;
import android.os.*;
import java.util.*;

/** Service-owned live location. Screen state never stops subscriptions. All
 * registrations/callbacks use one dedicated looper, independent of Activity.
 * GNSS and the platform fused provider share one newest-fix mailbox. */
public final class PhoneLocationFeed implements AutoCloseable {
 private final Context context;private final LocationManager manager;private final PowerManager power;
 private final Runnable wake;private final HandlerThread thread;private final Handler handler;
 private volatile boolean wanted,closed,foreground;private volatile long session;
 private volatile Location latest;private volatile String state="OFF";private volatile long callbacks,lastCallback,registrations;
 private long activeSession,started,retryAt;private boolean active;private final ArrayList<String> providers=new ArrayList<>();
 private CancellationSignal refresh;
 private final LocationListener listener=new LocationListener(){
  public void onLocationChanged(Location value){
   if(!wanted||closed||!foreground)return;
   callbacks++;lastCallback=SystemClock.elapsedRealtime();
   Location old=latest;
   // A coarse fused fix must not replace a fresh accurate GNSS fix.
   long oldAge=old==null?Long.MAX_VALUE:lastCallback-old.getElapsedRealtimeNanos()/1000000L;
   boolean keepGnss=good(old)&&"gps".equals(old.getProvider())&&!"gps".equals(value.getProvider())&&oldAge<2000;
   if(!keepGnss&&(old==null||value.getElapsedRealtimeNanos()>old.getElapsedRealtimeNanos()&&
      (good(value)||!good(old)||oldAge>5000)))latest=new Location(value);
   Location current=latest;
   state=good(current)&&lastCallback-current.getElapsedRealtimeNanos()/1000000L<=5000?"LIVE":"ACQUIRING";wake.run();
  }
  public void onProviderEnabled(String p){handler.post(PhoneLocationFeed.this::reconcile);}
  public void onProviderDisabled(String p){handler.post(PhoneLocationFeed.this::reconcile);}
  public void onStatusChanged(String p,int status,Bundle extras){}
 };
 public PhoneLocationFeed(Context c,Runnable wake){this(c,wake,null);}
 /** Supplied looper is for deterministic lifecycle tests; production owns its
  * thread and periodic reconciliation independently of every Activity. */
 PhoneLocationFeed(Context c,Runnable wake,Looper looper){context=c.getApplicationContext();manager=c.getSystemService(LocationManager.class);power=c.getSystemService(PowerManager.class);this.wake=wake;
  thread=looper==null?new HandlerThread("noodoe-location"):null;
  if(thread!=null){thread.start();looper=thread.getLooper();}handler=new Handler(looper);if(thread!=null)handler.post(check);
 }
 public void update(boolean ign,long rideSession,boolean locationForeground){wanted=ign;session=rideSession;foreground=locationForeground;}
 public Location latest(){Location value=latest;return value==null?null:new Location(value);}
 public boolean enabled(){return wanted&&foreground&&!closed&&state.equals("LIVE");}
 public String status(){return state;}
 static boolean good(Location l){return l!=null&&l.hasAccuracy()&&l.getAccuracy()>=0&&l.getAccuracy()<=30;}
 private final Runnable check=new Runnable(){public void run(){if(closed)return;reconcile();handler.postDelayed(this,1000);}};
 private void stop(){if(refresh!=null){refresh.cancel();refresh=null;}if(active&&manager!=null)try{manager.removeUpdates(listener);}catch(RuntimeException ignored){}active=false;providers.clear();}
 void reconcile(){
  if(closed)return;long now=SystemClock.elapsedRealtime();
  boolean permitted=context.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)==PackageManager.PERMISSION_GRANTED;
  if(!wanted||!foreground||!permitted||manager==null){stop();latest=null;state=!wanted?"OFF":!permitted?"PERMISSION":!foreground?"FOREGROUND_REQUIRED":"UNAVAILABLE";return;}
  ArrayList<String> enabled=new ArrayList<>();
  try{if(Build.VERSION.SDK_INT>=31&&manager.hasProvider(LocationManager.FUSED_PROVIDER)&&manager.isProviderEnabled(LocationManager.FUSED_PROVIDER))enabled.add(LocationManager.FUSED_PROVIDER);
   if(manager.isProviderEnabled(LocationManager.GPS_PROVIDER))enabled.add(LocationManager.GPS_PROVIDER);
  }catch(RuntimeException unavailable){stop();state="PROVIDER_ERROR";return;}
  if(enabled.isEmpty()){stop();latest=null;state="LOCATION_DISABLED";return;}
  boolean newSession=activeSession!=session;
  if(newSession){latest=null;activeSession=session;retryAt=0;}
  long age=lastCallback==0?now-started:now-lastCallback;
  // A failed optional fused provider must not tear down a working GNSS
  // subscription each second. A slow first satellite fix is not proof that
  // the listener died. Keep subscriptions while requesting a fresh fix.
  boolean changed=!enabled.containsAll(providers);
  if(!active||newSession||changed||(!providers.containsAll(enabled)&&now>=retryAt)){
   if(now<retryAt&&!newSession&&!changed)return;
   if(newSession||changed)stop();
   if(!active){started=now;lastCallback=0;}retryAt=now+30000;
   for(String provider:enabled){
    if(providers.contains(provider))continue;
    try{
     if(Build.VERSION.SDK_INT>=31)manager.requestLocationUpdates(provider,new LocationRequest.Builder(1000).setQuality(LocationRequest.QUALITY_HIGH_ACCURACY).setMinUpdateIntervalMillis(1000).setMinUpdateDistanceMeters(0).setMaxUpdateDelayMillis(0).build(),r->handler.post(r),listener);
     else manager.requestLocationUpdates(provider,1000,0,listener,handler.getLooper());
     active=true;providers.add(provider);
    }catch(SecurityException revoked){state="PERMISSION";}
    catch(RuntimeException unavailable){state="PROVIDER_ERROR";}
   }
   registrations++;if(active)state="ACQUIRING";
  }else if(age>5000){
   state=screenRestricted()?"POWER_RESTRICTED":"STALE";
   if(now>=retryAt){retryAt=now+30000;requestFreshFix();}
  }
 }
 /** Bounded async refresh for OEM provider stalls. Never re-label cached
  * coordinates as fresh; the encoder still checks their original timestamp. */
 private void requestFreshFix(){
  if(Build.VERSION.SDK_INT<30)return;
  if(context.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)!=PackageManager.PERMISSION_GRANTED){stop();latest=null;state="PERMISSION";return;}
  if(refresh!=null)refresh.cancel();refresh=new CancellationSignal();CancellationSignal request=refresh;
  for(String provider:providers)try{
   manager.getCurrentLocation(provider,request,r->handler.post(r),fix->{if(!request.isCanceled()&&fix!=null)listener.onLocationChanged(fix);});
  }catch(SecurityException revoked){stop();latest=null;state="PERMISSION";return;}
  catch(RuntimeException unavailable){/* Keep the registered listener alive. */}
  handler.postDelayed(request::cancel,10000);
 }
 private boolean screenRestricted(){if(power==null||Build.VERSION.SDK_INT<28||power.isInteractive())return false;int mode=power.getLocationPowerSaveMode();return mode==PowerManager.LOCATION_MODE_GPS_DISABLED_WHEN_SCREEN_OFF||mode==PowerManager.LOCATION_MODE_ALL_DISABLED_WHEN_SCREEN_OFF||mode==PowerManager.LOCATION_MODE_THROTTLE_REQUESTS_WHEN_SCREEN_OFF;}
 /** Diagnostic scalars only: never coordinates, bearing, or provider payloads. */
 public Map<String,String> diagnostics(){Map<String,String> d=new LinkedHashMap<>();Location fix=latest;d.put("state",state);d.put("callbacks",Long.toString(callbacks));d.put("registrations",Long.toString(registrations));d.put("fix_age_ms",Long.toString(fix==null?-1:Math.max(0,SystemClock.elapsedRealtime()-fix.getElapsedRealtimeNanos()/1000000L)));d.put("callback_age_ms",Long.toString(lastCallback==0?-1:Math.max(0,SystemClock.elapsedRealtime()-lastCallback)));d.put("screen",power!=null&&power.isInteractive()?"ON":"OFF");d.put("location_power_mode",Integer.toString(power!=null&&Build.VERSION.SDK_INT>=28?power.getLocationPowerSaveMode():0));d.put("background_permission",Boolean.toString(Build.VERSION.SDK_INT<29||context.checkSelfPermission(Manifest.permission.ACCESS_BACKGROUND_LOCATION)==PackageManager.PERMISSION_GRANTED));return d;}
 @Override public void close(){closed=true;wanted=false;handler.removeCallbacksAndMessages(null);handler.post(()->{stop();latest=null;if(thread!=null)thread.quitSafely();});wake.run();}
}
