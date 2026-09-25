package io.opennoodoe.app.companion;
import android.content.Context;
import org.junit.*;import org.junit.runner.RunWith;
import org.robolectric.*;import org.robolectric.annotation.Config;
import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28)
public class DrivingModeTest {
 @Before public void clear(){RuntimeEnvironment.getApplication().getSharedPreferences("driving-mode",0).edit().clear().commit();}
 @Test public void updateNeedsManualStopAndSurvivesProcessRecreation(){
  Context c=RuntimeEnvironment.getApplication();DrivingMode p=new DrivingMode(c);
  assertTrue(p.automatic("AA"));assertFalse(p.beginUpdate("AA"));
  assertTrue(p.set("AA",DrivingMode.MANUAL));assertFalse(p.automatic("AA"));assertTrue(p.beginUpdate("AA"));
  assertEquals(DrivingMode.UPDATE,new DrivingMode(c).get("aa"));assertTrue(p.automatic("BB"));
  c.getSharedPreferences("installer-selections-v2",0).edit().clear().commit();assertFalse(new DrivingMode(c).automatic("AA"));
  assertTrue(p.set("AA",DrivingMode.AUTO));assertTrue(new DrivingMode(c).automatic("AA"));
 }
 @Test public void recordingIsOffUntilOptInAndDoesNotReadTelemetry(){
  Context c=RuntimeEnvironment.getApplication();CompanionWire wire=new CompanionWire(null);wire.ign=true;wire.ignValid=true;
  try(RideSync sync=new RideSync(c,wire,"AA",5)){
   try{sync.tick();sync.enabled(false);sync.tick();}catch(Exception e){throw new AssertionError(e);}
   assertEquals(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0697,"폰 주행 기록 꺼짐"),sync.status);
  }
 }
}
