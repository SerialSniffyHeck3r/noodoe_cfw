package io.opennoodoe.app.companion;
import android.Manifest;import android.content.*;import android.location.*;import android.media.*;import android.media.session.*;import android.os.*;
import org.junit.*;import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.Config;import org.robolectric.shadows.*;
import java.lang.reflect.*;import java.time.Duration;import java.util.*;import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=34)
public class BackgroundFeedTest {
 private Context c;private LocationManager locations;private ShadowLocationManager gps;
 @Before public void setup(){c=RuntimeEnvironment.getApplication();Shadows.shadowOf(RuntimeEnvironment.getApplication()).grantPermissions(Manifest.permission.ACCESS_FINE_LOCATION,Manifest.permission.ACCESS_COARSE_LOCATION);locations=c.getSystemService(LocationManager.class);gps=Shadows.shadowOf(locations);gps.setProviderEnabled("gps",true);}
 private ShadowLooper looper(PhoneLocationFeed f)throws Exception{Field a=PhoneLocationFeed.class.getDeclaredField("handler");a.setAccessible(true);return Shadows.shadowOf(((Handler)a.get(f)).getLooper());}
 private Location fix(long t,float accuracy){Location l=new Location("gps");l.setLatitude(37);l.setLongitude(127);l.setAccuracy(accuracy);l.setTime(1700000000000L+t);l.setElapsedRealtimeNanos(t*1000000);return l;}
 @Test public void screenOffStillDeliversAndIgnOffUnsubscribes()throws Exception{
  int[] events={0};try(PhoneLocationFeed f=new PhoneLocationFeed(c,()->events[0]++)){
   f.update(true,1,true);looper(f).idle();assertFalse(gps.getLocationUpdateListeners("gps").isEmpty());
   LocationRequest r=gps.getLocationRequests("gps").get(0);assertEquals(1000,r.getIntervalMillis());assertEquals(0,r.getMaxUpdateDelayMillis());assertEquals(LocationRequest.QUALITY_HIGH_ACCURACY,r.getQuality());
   Shadows.shadowOf(c.getSystemService(PowerManager.class)).setIsInteractive(false);
   for(int i=0;i<20;i++){looper(f).idleFor(Duration.ofSeconds(1));gps.simulateLocation(fix(SystemClock.elapsedRealtime(),5));looper(f).idle();assertNotNull(f.latest());assertEquals("LIVE",f.status());}
   assertTrue(events[0]>=20);assertEquals("OFF",f.diagnostics().get("screen"));
   f.update(false,1,true);looper(f).idleFor(Duration.ofSeconds(1));assertNull(f.latest());assertTrue(gps.getLocationUpdateListeners("gps").isEmpty());assertEquals("OFF",f.status());
  }
 }
 @Test public void staleSubscriptionKeepsListenerAndNeverFabricatesFixes()throws Exception{
  try(PhoneLocationFeed f=new PhoneLocationFeed(c,()->{})){
   f.update(true,1,true);looper(f).idle();looper(f).idleFor(Duration.ofSeconds(31));assertNull(f.latest());assertEquals("STALE",f.status());assertEquals("1",f.diagnostics().get("registrations"));
   looper(f).idleFor(Duration.ofSeconds(10));assertEquals("1",f.diagnostics().get("registrations"));
   f.update(true,1,false);looper(f).idleFor(Duration.ofSeconds(1));assertEquals("FOREGROUND_REQUIRED",f.status());assertTrue(gps.getLocationUpdateListeners("gps").isEmpty());
  }
 }
 @Test public void permissionRevocationRemovesLocationButDoesNotThrow()throws Exception{
  try(PhoneLocationFeed f=new PhoneLocationFeed(c,()->{})){
   f.update(true,2,true);looper(f).idle();Shadows.shadowOf(RuntimeEnvironment.getApplication()).denyPermissions(Manifest.permission.ACCESS_FINE_LOCATION);
   looper(f).idleFor(Duration.ofSeconds(1));assertEquals("PERMISSION",f.status());assertTrue(gps.getLocationUpdateListeners("gps").isEmpty());
  }
 }
 @Test public void newRideClearsCachedFixAndPoorFusedFixCannotReplaceFreshGnss()throws Exception{
  try(PhoneLocationFeed f=new PhoneLocationFeed(c,()->{})){
   f.update(true,1,true);looper(f).idle();looper(f).idleFor(Duration.ofSeconds(1));gps.simulateLocation(fix(SystemClock.elapsedRealtime(),5));looper(f).idle();assertEquals(5,f.latest().getAccuracy(),0);
   looper(f).idleFor(Duration.ofSeconds(1));gps.simulateLocation(fix(SystemClock.elapsedRealtime(),200));looper(f).idle();assertEquals(5,f.latest().getAccuracy(),0);
   f.update(true,2,true);looper(f).idleFor(Duration.ofSeconds(1));assertNull(f.latest());
  }
 }
 @Test public void sessionCallbacksWakeWithoutPollingAndKeepSelectedPlayer()throws Exception{
  int[] wakes={0};MediaSession one=new MediaSession(c,"one"),two=new MediaSession(c,"two");MediaController a=one.getController(),b=two.getController();
  PlaybackState playing=new PlaybackState.Builder().setState(PlaybackState.STATE_PLAYING,0,1).build();Shadows.shadowOf(a).setPlaybackState(playing);Shadows.shadowOf(b).setPlaybackState(playing);
  ShadowMediaSessionManager manager=Shadows.shadowOf(c.getSystemService(MediaSessionManager.class));manager.addController(a);manager.addController(b);
  try(MediaUpdates feed=new MediaUpdates(c,()->wakes[0]++)){
   Shadows.shadowOf(Looper.getMainLooper()).idle();assertEquals(a.getSessionToken(),feed.current().getSessionToken());long version=feed.version();
   Shadows.shadowOf(a).executeOnMetadataChanged(new MediaMetadata.Builder().putString(MediaMetadata.METADATA_KEY_TITLE,"Changed").build());Shadows.shadowOf(Looper.getMainLooper()).idle();assertTrue(feed.version()>version);
   Method replace=MediaUpdates.class.getDeclaredMethod("replace",List.class);replace.setAccessible(true);
   for(int i=0;i<100;i++)replace.invoke(feed,i%2==0?Arrays.asList(b,a):Arrays.asList(a,b));assertEquals(a.getSessionToken(),feed.current().getSessionToken());
  }finally{Shadows.shadowOf(Looper.getMainLooper()).idle();assertTrue(Shadows.shadowOf(a).getCallbacks().isEmpty());one.release();two.release();}
 }
 @Test public void wakeBetweenTickAndWaitCannotBeLost()throws Exception{CompanionSignal signal=new CompanionSignal();long old=signal.version();signal.changed();long start=System.nanoTime();signal.await(old,10000);assertTrue(System.nanoTime()-start<1000000000L);}
}
