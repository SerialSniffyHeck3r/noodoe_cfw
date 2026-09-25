package io.opennoodoe.app.companion;
import android.Manifest;
import android.content.Context;
import android.location.*;
import android.os.*;
import java.util.concurrent.Executor;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.annotation.*;
import org.robolectric.shadows.*;
import static org.junit.Assert.*;

/** Shipping subscription/callback code under screen-off, provider failure and
 * permission loss. Simulated Android providers, not Samsung RF validation. */
@RunWith(RobolectricTestRunner.class) @Config(sdk=31,shadows=PhoneLocationFeedTest.Providers.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class PhoneLocationFeedTest {
 @Implements(LocationManager.class) public static class Providers extends ShadowLocationManager {
  static boolean failFused,denyRefresh;static int removes;
  @Implementation protected void requestLocationUpdates(String p,LocationRequest r,Executor e,LocationListener l){if(failFused&&"fused".equals(p))throw new IllegalArgumentException("unavailable");super.requestLocationUpdates(p,r,e,l);}
  @Implementation protected void removeUpdates(LocationListener l){removes++;super.removeUpdates(l);}
  @Implementation protected void getCurrentLocation(String p,CancellationSignal s,Executor e,java.util.function.Consumer<Location> done){if(denyRefresh)throw new SecurityException("revoked during refresh");e.execute(()->done.accept(null));}
 }
 private Context context;private LocationManager manager;private PhoneLocationFeed feed;
 @Before public void setup(){context=RuntimeEnvironment.getApplication();Shadows.shadowOf(RuntimeEnvironment.getApplication()).grantPermissions(Manifest.permission.ACCESS_FINE_LOCATION);manager=context.getSystemService(LocationManager.class);Providers.failFused=false;Providers.denyRefresh=false;Providers.removes=0;Shadows.shadowOf(manager).setProviderEnabled("gps",true);Shadows.shadowOf(manager).setProviderEnabled("fused",true);feed=new PhoneLocationFeed(context,()->{},Looper.getMainLooper());}
 @After public void close(){feed.close();Shadows.shadowOf(Looper.getMainLooper()).idle();}
 private void fix(long timestamp){Location l=new Location("gps");l.setLatitude(37);l.setLongitude(127);l.setAccuracy(4);l.setElapsedRealtimeNanos(timestamp);l.setTime(1700000000000L);Shadows.shadowOf(manager).simulateLocation(l);Shadows.shadowOf(Looper.getMainLooper()).idle();}
 @Test public void screenOffContinuesAndIgnOffStops(){
  feed.update(true,1,true);feed.reconcile();Shadows.shadowOf(context.getSystemService(PowerManager.class)).setIsInteractive(false);
  ShadowSystemClock.advanceBy(java.time.Duration.ofSeconds(2));fix(SystemClock.elapsedRealtimeNanos());assertEquals("LIVE",feed.status());assertNotNull(feed.latest());assertTrue(feed.enabled());
  feed.reconcile();assertEquals(0,Providers.removes);feed.update(false,1,true);feed.reconcile();assertNull(feed.latest());assertEquals("OFF",feed.status());assertTrue(Shadows.shadowOf(manager).getLocationUpdateListeners().isEmpty());
 }
 @Test public void optionalFailureNeverCancelsWorkingGps(){
  Providers.failFused=true;feed.update(true,1,true);feed.reconcile();assertFalse(Shadows.shadowOf(manager).getLocationUpdateListeners("gps").isEmpty());
  for(int i=0;i<90;i++){ShadowSystemClock.advanceBy(java.time.Duration.ofSeconds(1));feed.reconcile();}
  assertEquals("GNSS acquisition must not restart every 30 seconds",0,Providers.removes);fix(SystemClock.elapsedRealtimeNanos());assertTrue(feed.enabled());
 }
 @Test public void revokedPermissionAndMissingForegroundDoNotPublishGps(){
  feed.update(true,1,true);feed.reconcile();fix(SystemClock.elapsedRealtimeNanos());
  Shadows.shadowOf(RuntimeEnvironment.getApplication()).denyPermissions(Manifest.permission.ACCESS_FINE_LOCATION);feed.reconcile();assertEquals("PERMISSION",feed.status());assertNull(feed.latest());
  Shadows.shadowOf(RuntimeEnvironment.getApplication()).grantPermissions(Manifest.permission.ACCESS_FINE_LOCATION);feed.update(true,1,false);feed.reconcile();assertEquals("FOREGROUND_REQUIRED",feed.status());assertNull(feed.latest());
 }
 @Test public void staleCallbackRetainsOriginalAge(){feed.update(true,1,true);feed.reconcile();long then=SystemClock.elapsedRealtimeNanos();ShadowSystemClock.advanceBy(java.time.Duration.ofSeconds(10));fix(then);assertFalse(feed.enabled());assertEquals(then,feed.latest().getElapsedRealtimeNanos());}
 @Test public void revocationDuringFreshRequestClearsFixWithoutCrash(){feed.update(true,1,true);feed.reconcile();fix(SystemClock.elapsedRealtimeNanos());Providers.denyRefresh=true;ShadowSystemClock.advanceBy(java.time.Duration.ofSeconds(31));feed.reconcile();assertEquals("PERMISSION",feed.status());assertNull(feed.latest());assertTrue(Shadows.shadowOf(manager).getLocationUpdateListeners().isEmpty());}
}
