package io.opennoodoe.app.maintenance;
import android.os.Looper;
import io.opennoodoe.app.installer.SessionEpoch;
import java.lang.reflect.Field;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.annotation.Config;
import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28)
public class CompanionServiceHandoffTest {
 private static Field field(String name)throws Exception{Field f=MaintenanceService.class.getDeclaredField(name);f.setAccessible(true);return f;}
 @Test public void terminalProgressIsDeliveredBeforeAutoRideCanInvalidateEpoch()throws Exception{
  var controller=Robolectric.buildService(MaintenanceService.class).create();MaintenanceService s=controller.get();
  try{
   var epochs=(SessionEpoch)field("epochs").get(s);long token=epochs.current();
   var p=(io.opennoodoe.app.installer.InstallerPresentation)field("presentation").get(s);
   field("presenting").set(s,true);p.begin("update-cfw");p.stage("health","checking",0,0,"");
   byte[] deviceProgress=new byte[80];deviceProgress[4]=1;deviceProgress[20]=7;deviceProgress[24]=8;
   p.installStatus(new io.opennoodoe.app.installer.InstallProgressSnapshot(deviceProgress));
   java.util.List<Integer> percentages=new java.util.ArrayList<>();
   MaintenanceService.Listener listener=new MaintenanceService.Listener(){public void changed(String text,boolean busy){}public void progress(io.opennoodoe.app.installer.InstallerPresentation.Snapshot snap){percentages.add(snap.overallPercent);}};
   s.subscribe(listener);assertEquals(87,(int)percentages.get(0));assertTrue(p.snapshot().overall.contains("7 / 8"));
   p.stage("done","durably confirmed",1,1,"");p.finish("done",false);field("busy").set(s,true);
   // Completion occurs between periodic ticker updates.
   s.finishConnectionService(token,true);Shadows.shadowOf(Looper.getMainLooper()).idle();assertEquals(100,(int)percentages.get(percentages.size()-1));assertTrue(p.snapshot().overall.contains("8 / 8"));
   field("presenting").set(s,false);percentages.clear();s.subscribe(listener);assertEquals(100,(int)percentages.get(0));
  }finally{controller.destroy();}
 }
 @Test public void oldCompletionCannotStopNewServiceOwner()throws Exception{
  var controller=Robolectric.buildService(MaintenanceService.class).create();MaintenanceService s=controller.get();
  try{
   SessionEpoch epochs=(SessionEpoch)field("epochs").get(s);long old=epochs.current();
   s.finishConnectionService(old,false);epochs.invalidate();field("busy").set(s,true);
   Shadows.shadowOf(Looper.getMainLooper()).idle();assertFalse(Shadows.shadowOf(s).isStoppedBySelf());
  }finally{controller.destroy();}
 }
 @Test public void handoffKeepsAnActiveOwnerAndIdleCompletionStopsService()throws Exception{
  var controller=Robolectric.buildService(MaintenanceService.class).create();MaintenanceService s=controller.get();
  try{
   long token=((SessionEpoch)field("epochs").get(s)).current();field("busy").set(s,true);
   s.finishConnectionService(token,true);Shadows.shadowOf(Looper.getMainLooper()).idle();assertFalse(Shadows.shadowOf(s).isStoppedBySelf());
   field("busy").set(s,false);s.finishConnectionService(token,false);Shadows.shadowOf(Looper.getMainLooper()).idle();assertTrue(Shadows.shadowOf(s).isStoppedBySelf());
  }finally{controller.destroy();}
 }
}
