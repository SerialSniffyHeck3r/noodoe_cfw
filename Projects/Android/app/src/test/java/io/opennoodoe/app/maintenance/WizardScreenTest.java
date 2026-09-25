package io.opennoodoe.app.maintenance;
import android.graphics.*;import android.view.*;import android.widget.*;import android.content.*;
import org.junit.*;import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.*;
import java.io.*;import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28,qualifiers="w393dp-h852dp-mdpi") @GraphicsMode(GraphicsMode.Mode.NATIVE)
public class WizardScreenTest {
 @Test public void productRecoveryOffersUpdateAndNewZipWithoutDowngradingApk(){
  Context context=RuntimeEnvironment.getApplication();
  Shadows.shadowOf(RuntimeEnvironment.getApplication()).declareComponentUnbindable(new ComponentName(context,MaintenanceService.class));
  try(org.robolectric.android.controller.ActivityController<InstallerHomeActivity> c=Robolectric.buildActivity(InstallerHomeActivity.class).setup()){
   java.util.List<String> actions=new java.util.ArrayList<>();
   SetupWizardView wizard=new SetupWizardView(c.get(),actions::add);
   wizard.update(new SetupWorkflow.View("AA:01","product","cfw-verify","old APP mismatch",false,true),"a".repeat(64),false);
   assertTrue(actions.isEmpty());
   find(wizard,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0298,"3. CFW 업데이트 / 이어가기")).performClick();
   assertEquals(java.util.Collections.singletonList("update-cfw"),actions);
   assertNotNull(find(wizard,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0299,"다른 업데이트 ZIP 선택")));
  }
 }
 @Test public void wizardRolesErrorsAndPermissionsRenderWithoutStartingWork()throws Exception{
  Context context=RuntimeEnvironment.getApplication();Shadows.shadowOf(RuntimeEnvironment.getApplication()).declareComponentUnbindable(new ComponentName(context,MaintenanceService.class));
  for(int mode:new int[]{1,2}){
   io.opennoodoe.app.UiThemeSettings.setMode(context,mode);
   try(org.robolectric.android.controller.ActivityController<InstallerHomeActivity> c=Robolectric.buildActivity(InstallerHomeActivity.class).setup()){
    InstallerHomeActivity a=c.get();capture(a.getWindow().getDecorView(),"welcome-"+mode);
    for(String role:new String[]{"stock","bootstrap","product"}){
     new DeviceSelections(context).select("AA:01");new DeviceSelections(context).bundle("AA:01",null);
     a.workflow(new SetupWorkflow.View("AA:01",role,"","",false,false));capture(a.getWindow().getDecorView(),role+"-file-"+mode);
     new DeviceSelections(context).bundle("AA:01","8325d5e4ff9b68f1491a3ec00b2f26ff65cf1343b730359103139f2f247d6460");
     a.workflow(new SetupWorkflow.View("AA:01",role,"","",false,false));capture(a.getWindow().getDecorView(),role+"-ready-"+mode);
    }
    a.workflow(new SetupWorkflow.View("AA:01","bootstrap","guided-bootstrap","연결이 끊겨 설치 결과를 아직 확인하지 못했어요.",false,true));capture(a.getWindow().getDecorView(),"recovery-"+mode);
    assertNotNull(find(a.getWindow().getDecorView(),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0291,"1. 현재 기기 확인")));assertNull(find(a.getWindow().getDecorView(),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0338,"업데이트 확인")));
    find(a.getWindow().getDecorView(),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0165,"권한·연동 준비  ›")).performClick();
    android.app.AlertDialog d=org.robolectric.shadows.ShadowAlertDialog.getLatestAlertDialog();assertNotNull(d);Shadows.shadowOf(android.os.Looper.getMainLooper()).idle();capture(d.getWindow().getDecorView(),"permissions-"+mode);d.dismiss();
   }
   context.getSharedPreferences("installer-selections-v2",0).edit().clear().commit();
  }
 }
 private Button find(View v,String text){if(v.getVisibility()!=View.VISIBLE)return null;if(v instanceof Button&&text.contentEquals(((Button)v).getText()))return (Button)v;if(v instanceof ViewGroup)for(int i=0;i<((ViewGroup)v).getChildCount();i++){Button b=find(((ViewGroup)v).getChildAt(i),text);if(b!=null)return b;}return null;}
 private void capture(View root,String name)throws Exception{root.measure(View.MeasureSpec.makeMeasureSpec(393,View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(852,View.MeasureSpec.EXACTLY));root.layout(0,0,393,852);Bitmap b=Bitmap.createBitmap(393,852,Bitmap.Config.ARGB_8888);root.draw(new Canvas(b));File dir=new File("build/wizard-preview");assertTrue(dir.isDirectory()||dir.mkdirs());try(FileOutputStream out=new FileOutputStream(new File(dir,name+".png"))){b.compress(Bitmap.CompressFormat.PNG,100,out);}b.recycle();}
}
