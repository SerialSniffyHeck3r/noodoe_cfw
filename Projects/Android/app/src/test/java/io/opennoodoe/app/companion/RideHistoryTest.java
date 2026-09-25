package io.opennoodoe.app.companion;
import android.content.Context;import io.opennoodoe.app.protocol.ByteCodec;
import org.junit.*;import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.Config;
import java.io.*;import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28)
public class RideHistoryTest {
 private RideTelemetry sample(int speed,boolean fresh)throws Exception{byte[] b=new byte[80];ByteCodec.putU32le(b,8,3);ByteCodec.putU32le(b,12,fresh?0:1);ByteCodec.putU32le(b,24,speed);ByteCodec.putU32le(b,28,36475);return new RideTelemetry(b);}
 @Test public void movingAndStoppedUseUARTAndUnknownGapsDoNotBecomeDistance()throws Exception{RideAccumulator a=new RideAccumulator();a.add(0,sample(36,true),5);a.add(1000,sample(36,true),5);assertEquals(.01,a.distanceKm,.00001);assertEquals(1000,a.movingMs);a.add(11000,sample(36,true),5);assertEquals(.01,a.distanceKm,.00001);assertEquals(10000,a.unknownMs);a.add(12000,sample(0,false),5);assertEquals(11000,a.unknownMs);}
 @Test public void thresholdIsStrictAndConfigurable()throws Exception{RideAccumulator a=new RideAccumulator();a.add(0,sample(4,true),5);a.add(1000,sample(5,true),5);a.add(2000,sample(5,true),5);assertEquals(1000,a.stoppedMs);assertEquals(1000,a.movingMs);}
 @Test public void invalidLengthAndImplausibleSpeedDoNotCount()throws Exception{try{new RideTelemetry(new byte[79]);fail();}catch(IOException expected){}assertFalse(sample(999,true).fresh);}
 @Test public void processDeathIsKeptAsInterruptedNotComplete()throws Exception{Context c=RuntimeEnvironment.getApplication();c.deleteDatabase("ride-history.db");
  try(RideHistory db=new RideHistory(c)){long id=db.begin("AA",1,1000);RideAccumulator a=new RideAccumulator();a.add(0,sample(36,true),5);a.add(1000,sample(36,true),5);db.save(id,a,sample(36,true),2000);}
  try(RideHistory db=new RideHistory(c)){long id=db.begin("AA",1,3000);db.finish(id,"ended");assertEquals(2,db.recent().size());ByteArrayOutputStream out=new ByteArrayOutputStream();db.export(out);String csv=out.toString("UTF-8");assertTrue(csv.contains("interrupted"));assertTrue(csv.contains("ended"));assertTrue(csv.contains("36475"));assertFalse(csv.contains("latitude"));}
 }
 @Test public void historySurvivesAppSelectionReset()throws Exception{Context c=RuntimeEnvironment.getApplication();c.deleteDatabase("ride-history.db");try(RideHistory db=new RideHistory(c)){long id=db.begin("AA",1,1000);db.finish(id,"ended");}c.getSharedPreferences("installer-selections-v2",0).edit().clear().commit();try(RideHistory db=new RideHistory(c)){assertEquals(1,db.recent().size());}}
}
