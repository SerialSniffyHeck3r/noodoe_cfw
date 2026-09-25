package io.opennoodoe.app.companion;
import android.location.Location;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28)
public class PhoneGpsEncoderTest {
 private Location fix(){Location l=new Location("gps");l.setLatitude(37);l.setLongitude(127);l.setTime(1700000000000L);l.setAccuracy(5);l.setSpeed(12);l.setBearing(359.5f);l.setBearingAccuracyDegrees(5);return l;}
 @Test public void sendsMotionBearingAndMillimetresNotFakeZeroSpeed(){byte[] p=PhoneGpsEncoder.encode(fix());assertEquals(31,CompanionWire.u(p,0));assertEquals(12000,CompanionWire.u(p,12));assertEquals(359500,CompanionWire.u(p,16));}
 @Test public void inaccurateFixExplicitlyInvalidatesPosition(){Location l=fix();l.setAccuracy(70);assertEquals(6,CompanionWire.u(PhoneGpsEncoder.encode(l),0));}
 @Test public void stationaryOrUncertainBearingDoesNotRotateMap(){Location l=fix();l.setSpeed(.4f);assertEquals(15,CompanionWire.u(PhoneGpsEncoder.encode(l),0));l.setSpeed(12);l.setBearingAccuracyDegrees(60);assertEquals(15,CompanionWire.u(PhoneGpsEncoder.encode(l),0));}
}
