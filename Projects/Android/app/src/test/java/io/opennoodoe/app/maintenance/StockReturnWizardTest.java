package io.opennoodoe.app.maintenance;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.annotation.Config;
import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class) @Config(sdk=28)
public class StockReturnWizardTest {
 private Button find(View view,String title){
  if(view instanceof Button&&((Button)view).getText().toString().contains(title))return (Button)view;
  if(view instanceof ViewGroup)for(int n=0;n<((ViewGroup)view).getChildCount();n++){Button b=find(((ViewGroup)view).getChildAt(n),title);if(b!=null)return b;}
  return null;
 }
 @Test public void staleProductAndUnknownRoleExposeReadOnlyPhysicalReturn(){
  Activity a=Robolectric.buildActivity(Activity.class).setup().get();String[] action={""};
  for(String role:new String[]{"unknown","product","bootstrap"}){
   SetupWizardView ui=new SetupWizardView(a,value->action[0]=value);
   ui.update(new SetupWorkflow.View("AA",role,"guided-bootstrap","WAIT_CFW_BOOT",false,true),"abcdef",false);
   Button b=find(ui,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0293,"본체에서 순정으로 돌아왔어요 · 확인"));assertNotNull(role,b);b.performClick();assertEquals("stock-return-check",action[0]);
  }
 }
 @Test public void verifiedReturnClearsPersistedUiWaitButDiscoveryAloneDoesNot(){
  SetupWorkflow w=new SetupWorkflow(RuntimeEnvironment.getApplication());w.select("AA");w.role("product");
  w.begin("guided-bootstrap");w.finish("guided-bootstrap",true,"WAIT_CFW_BOOT");
  w.begin("inspect-device");w.role("stock");w.finish("inspect-device",false,"stock");assertTrue(w.view().recovery);
  w.begin("stock-return-check");w.finish("stock-return-check",true,"offline");assertTrue(w.view().recovery);
  w.begin("stock-return-check");w.role("stock");w.finish("stock-return-check",false,"verified");assertFalse(w.view().recovery);
  w=new SetupWorkflow(RuntimeEnvironment.getApplication());w.select("AA");assertFalse(w.view().recovery);
 }
}
