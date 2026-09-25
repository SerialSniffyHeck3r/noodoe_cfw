package io.opennoodoe.app.installer;
import android.graphics.*;
import android.view.*;
import android.widget.*;
import io.opennoodoe.app.maintenance.InstallerHomeActivity;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.annotation.*;
import java.io.*;
import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class) @Config(sdk=28,qualifiers="w393dp-h852dp-mdpi") @GraphicsMode(GraphicsMode.Mode.NATIVE)
public class InstallerScreenTest {
 @Test public void companionHomeAndTabsRenderAndSurviveRecreation()throws Exception{
  Shadows.shadowOf(RuntimeEnvironment.getApplication()).declareComponentUnbindable(new android.content.ComponentName(RuntimeEnvironment.getApplication(),io.opennoodoe.app.maintenance.MaintenanceService.class));
  for(int mode:new int[]{1,2}){
   io.opennoodoe.app.UiThemeSettings.setMode(RuntimeEnvironment.getApplication(),mode);
   try(org.robolectric.android.controller.ActivityController<InstallerHomeActivity> c=Robolectric.buildActivity(InstallerHomeActivity.class).setup()){
    c.get().workflow(new io.opennoodoe.app.maintenance.SetupWorkflow.View("AA:01","product","","",false,false));
    for(String tab:new String[]{io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0119,"주행"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0120,"꾸미기"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0121,"설치")}){
     View root=c.get().getWindow().getDecorView();Button b=findButton(root,tab);assertNotNull(b);b.performClick();
     root.measure(View.MeasureSpec.makeMeasureSpec(393,View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(852,View.MeasureSpec.EXACTLY));root.layout(0,0,393,852);
     Bitmap image=Bitmap.createBitmap(393,852,Bitmap.Config.ARGB_8888);root.draw(new Canvas(image));File dir=new File("build/installer-preview");assertTrue(dir.isDirectory()||dir.mkdirs());
     String name=tab.equals(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0119,"주행"))?"ride":tab.equals(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0120,"꾸미기"))?"custom":"install";
     try(FileOutputStream out=new FileOutputStream(new File(dir,name+"-"+mode+".png"))){image.compress(Bitmap.CompressFormat.PNG,100,out);}image.recycle();
     assertTrue(b.isSelected());
    }
    c.recreate();assertTrue(findButton(c.get().getWindow().getDecorView(),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0121,"설치")).isSelected());
   }
  }
  io.opennoodoe.app.UiThemeSettings.setMode(RuntimeEnvironment.getApplication(),0);
 }
 private Button findButton(View v,String label){if(v instanceof Button&&label.contentEquals(((Button)v).getText()))return (Button)v;
  if(v instanceof ViewGroup)for(int i=0;i<((ViewGroup)v).getChildCount();i++){Button b=findButton(((ViewGroup)v).getChildAt(i),label);if(b!=null)return b;}return null;}
 @Test public void installationProgressRendersInBothThemes()throws Exception{
  Shadows.shadowOf(RuntimeEnvironment.getApplication()).declareComponentUnbindable(new android.content.ComponentName(RuntimeEnvironment.getApplication(),io.opennoodoe.app.maintenance.MaintenanceService.class));
  for(int mode:new int[]{1,2}){
   io.opennoodoe.app.UiThemeSettings.setMode(RuntimeEnvironment.getApplication(),mode);
   try(org.robolectric.android.controller.ActivityController<InstallerHomeActivity> c=Robolectric.buildActivity(InstallerHomeActivity.class).setup()){
    InstallerHomeActivity a=c.get();InstallerPresentation p=new InstallerPresentation(()->5000);p.begin("guided-bootstrap");
    p.role("bootstrap");p.stage("file-resource","폰트와 Bluetooth 패치를 확인하고 있어요.",36864,86016,"B");
    p.installStatus(new InstallProgressSnapshot(NdcpSession.words(0,1,42,10,3,2,8,7,1,9,36864,86016,32768,9,21,5000,0,1,0,1)));a.progress(p.snapshot());
    View root=a.getWindow().getDecorView();root.measure(View.MeasureSpec.makeMeasureSpec(393,View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(852,View.MeasureSpec.EXACTLY));root.layout(0,0,393,852);
    Bitmap image=Bitmap.createBitmap(393,852,Bitmap.Config.ARGB_8888);root.draw(new Canvas(image));
    File dir=new File("build/installer-preview");assertTrue(dir.isDirectory()||dir.mkdirs());
    try(FileOutputStream out=new FileOutputStream(new File(dir,mode==2?"dark.png":"light.png"))){image.compress(Bitmap.CompressFormat.PNG,100,out);}image.recycle();
    assertEquals(mode==2,io.opennoodoe.app.UiThemeSettings.isDark(a));assertEquals(2,countBars(root));
    c.recreate();assertEquals(mode,io.opennoodoe.app.UiThemeSettings.mode(c.get()));
   }
  }io.opennoodoe.app.UiThemeSettings.setMode(RuntimeEnvironment.getApplication(),0);
 }
 @Test public void rapidDoubleTapCreatesOneConfirmation()throws Exception{
  Shadows.shadowOf(RuntimeEnvironment.getApplication()).declareComponentUnbindable(new android.content.ComponentName(RuntimeEnvironment.getApplication(),io.opennoodoe.app.maintenance.MaintenanceService.class));
  try(org.robolectric.android.controller.ActivityController<InstallerHomeActivity> controller=Robolectric.buildActivity(InstallerHomeActivity.class).setup()){
   InstallerHomeActivity a=controller.get();
   java.lang.reflect.Method confirm=InstallerHomeActivity.class.getDeclaredMethod("confirm",String.class,String.class);confirm.setAccessible(true);
   confirm.invoke(a,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0521,"설치 확인 완료"),"guided-bootstrap");android.app.AlertDialog first=org.robolectric.shadows.ShadowAlertDialog.getLatestAlertDialog();
   confirm.invoke(a,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0521,"설치 확인 완료"),"guided-bootstrap");assertSame(first,org.robolectric.shadows.ShadowAlertDialog.getLatestAlertDialog());
   first.dismiss();Shadows.shadowOf(android.os.Looper.getMainLooper()).idle();
   confirm.invoke(a,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0521,"설치 확인 완료"),"guided-bootstrap");assertNotSame(first,org.robolectric.shadows.ShadowAlertDialog.getLatestAlertDialog());
   org.robolectric.shadows.ShadowAlertDialog.getLatestAlertDialog().dismiss();
  }
 }
 @Test public void twoIndependentBarsAndReasonRenderAtPhoneWidth()throws Exception{
  Shadows.shadowOf(RuntimeEnvironment.getApplication()).declareComponentUnbindable(new android.content.ComponentName(RuntimeEnvironment.getApplication(),io.opennoodoe.app.maintenance.MaintenanceService.class));
  try(org.robolectric.android.controller.ActivityController<InstallerHomeActivity> controller=Robolectric.buildActivity(InstallerHomeActivity.class).setup()){
   InstallerHomeActivity a=controller.get();long[] time={0};InstallerPresentation p=new InstallerPresentation(()->time[0]);p.begin("guided-bootstrap");
   p.stage("baseline","누도 내부에서 원본 해시를 계산 중이에요.",0,134217728,"B");time[0]=7000;p.reply();p.stage("baseline","기기 응답 정상 · 최근 진행 변화 0초 전",2748416,134217728,"B");a.progress(p.snapshot());
   View root=a.findViewById(android.R.id.content);root.measure(View.MeasureSpec.makeMeasureSpec(393,View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(852,View.MeasureSpec.EXACTLY));root.layout(0,0,393,852);
   assertEquals(2,countBars(root));Bitmap bitmap=Bitmap.createBitmap(393,852,Bitmap.Config.ARGB_8888);Canvas canvas=new Canvas(bitmap);canvas.drawColor(Color.WHITE);root.draw(canvas);
   File dir=new File("build/installer-preview");assertTrue(dir.isDirectory()||dir.mkdirs());try(FileOutputStream out=new FileOutputStream(new File(dir,"baseline.png"))){assertTrue(bitmap.compress(Bitmap.CompressFormat.PNG,100,out));}bitmap.recycle();
  }
 }
 private int countBars(View v){int n=v instanceof ProgressBar?1:0;if(v instanceof ViewGroup)for(int i=0;i<((ViewGroup)v).getChildCount();i++)n+=countBars(((ViewGroup)v).getChildAt(i));return n;}
}
