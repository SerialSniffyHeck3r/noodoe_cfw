package io.opennoodoe.app.maintenance;
import android.os.Looper;
import io.opennoodoe.app.UiThemeSettings;
import org.junit.Test;import static org.junit.Assert.*;
import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.Config;
import java.io.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28)
public class ServiceResetTest {
 @Test public void duplicateResetKeepsEvidenceAndTheme()throws Exception{
  org.robolectric.android.controller.ServiceController<MaintenanceService> c=Robolectric.buildService(MaintenanceService.class).create();
  try{
   MaintenanceService service=c.get();service.selectDevice("AA:BB:CC:00:11:22");UiThemeSettings.setMode(service,2);
   File evidence=new File(service.getFilesDir(),"installer/keep.bin");try(FileOutputStream out=new FileOutputStream(evidence)){out.write(42);}
   new DeviceSelections(service).bundle(service.selectedDevice(),"selected");
   service.resetLocal();service.resetLocal();long deadline=System.nanoTime()+5_000_000_000L;
   while(service.selectedDevice()!=null&&System.nanoTime()<deadline){Thread.sleep(20);Shadows.shadowOf(Looper.getMainLooper()).idle();}
   assertNull(service.selectedDevice());assertNull(service.selectedBundle());assertTrue(evidence.isFile());assertEquals(2,UiThemeSettings.mode(service));
   assertTrue(new File(service.getFilesDir(),"installer/logs").listFiles().length>0);
  }finally{c.destroy();}
 }
}
