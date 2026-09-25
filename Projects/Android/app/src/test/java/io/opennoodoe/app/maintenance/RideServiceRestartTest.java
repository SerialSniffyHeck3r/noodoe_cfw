package io.opennoodoe.app.maintenance;
import org.junit.*;import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.Config;import static org.junit.Assert.*;
import io.opennoodoe.app.companion.DrivingMode;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28)
public class RideServiceRestartTest {
 @Test public void processReclaimResumesOnlyPreviouslyIdentifiedProduct(){RideServiceRestart r=new RideServiceRestart(RuntimeEnvironment.getApplication());String mac="AA:BB:CC:11:22:33";assertFalse(r.allowed(mac,true));r.productConnected(mac);assertTrue(new RideServiceRestart(RuntimeEnvironment.getApplication()).allowed(mac,true));assertFalse(r.allowed("AA:BB:CC:11:22:44",true));assertFalse(r.allowed(mac,false));}
 @Test public void manualStopUpdateAndResetCannotAutostart(){android.content.Context c=RuntimeEnvironment.getApplication();RideServiceRestart r=new RideServiceRestart(c);String mac="AA:BB:CC:11:22:33";r.productConnected(mac);DrivingMode mode=new DrivingMode(c);mode.set(mac,DrivingMode.MANUAL);assertFalse(r.allowed(mac,true));mode.set(mac,DrivingMode.UPDATE);assertFalse(r.allowed(mac,true));mode.set(mac,DrivingMode.AUTO);r.clear();assertFalse(r.allowed(mac,true));}
}
