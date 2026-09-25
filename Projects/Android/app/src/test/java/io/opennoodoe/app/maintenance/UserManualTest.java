package io.opennoodoe.app.maintenance;
import android.content.*;import android.graphics.*;import android.view.*;import io.opennoodoe.app.installer.*;import org.junit.*;import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.*;import java.io.*;import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28,qualifiers="ko-w393dp-h852dp-mdpi") @GraphicsMode(GraphicsMode.Mode.NATIVE)
public class UserManualTest {
 void deviceFixture(InstallerHomeActivity host,String role,String operation,boolean busy){host.workflow(new SetupWorkflow.View("00:11:22:33:44:55",role,operation,"",busy,false));android.widget.Spinner spinner=org.robolectric.util.ReflectionHelpers.getField(host,"devices");java.util.ArrayList<String> addresses=org.robolectric.util.ReflectionHelpers.getField(host,"addresses");addresses.clear();addresses.add("00:11:22:33:44:55");spinner.setAdapter(new android.widget.ArrayAdapter<>(host,android.R.layout.simple_spinner_dropdown_item,new String[]{"기기 선택",role.equals("stock")?"KYMCO Noodoe 334455":"FuckNudo CFW 334455"}));spinner.setSelection(1);}
 String messagesPlaceholder(){return "설치와 정상 실행 확인이 완료됐어요.";}
 void capture(View root,String name)throws Exception{root.measure(View.MeasureSpec.makeMeasureSpec(393,1073741824),View.MeasureSpec.makeMeasureSpec(852,1073741824));root.layout(0,0,393,852);Bitmap b=Bitmap.createBitmap(393,852,Bitmap.Config.ARGB_8888);root.draw(new Canvas(b));File d=new File("build/manual-preview");assertTrue(d.isDirectory()||d.mkdirs());try(FileOutputStream f=new FileOutputStream(new File(d,name+".png"))){b.compress(Bitmap.CompressFormat.PNG,100,f);}b.recycle();}
 @Test public void manualIsReadonlyAndAvailableWithoutConnection()throws Exception{
 Shadows.shadowOf(RuntimeEnvironment.getApplication()).declareComponentUnbindable(new ComponentName(RuntimeEnvironment.getApplication(),MaintenanceService.class));
 try(org.robolectric.android.controller.ActivityController<InstallerHomeActivity> c=Robolectric.buildActivity(InstallerHomeActivity.class).setup()){
 UserManual.open(c.get());Intent intent=Shadows.shadowOf(c.get()).getNextStartedActivity();assertEquals(Intent.ACTION_VIEW,intent.getAction());assertEquals(UserManual.URL,intent.getDataString());capture(c.get().getWindow().getDecorView(),"app-home");}
 }
 @Test public void captureAllInstallerStepsWithoutTransferringAnything()throws Exception{
 Shadows.shadowOf(RuntimeEnvironment.getApplication()).declareComponentUnbindable(new ComponentName(RuntimeEnvironment.getApplication(),MaintenanceService.class));
 io.opennoodoe.app.UiThemeSettings.setMode(RuntimeEnvironment.getApplication(),2);
 String[] keys={"stock-transfer","stock-reboot","connect","audit","backup-metadata","file-resource","preserve","stage","confirm","reboot","health","done"};
 String[] messages={"설치 도구를 보내고 있어요.","누도 안내에 따라 키를 조작하고 기다려 주세요.","Bootstrap 연결을 확인했어요.","순정 파일과 겹치지 않는 저장 공간을 확인해요.","변경할 메타데이터의 원본을 보관해요.","CFW 자산을 기록하고 읽어 검증해요.","FAT와 파일을 최종 대조해요.","CFW 설치 이미지를 전송해요.","누도의 Install CFW? 화면에서 승인해 주세요.","NOODOE INSTALLER는 아직 설치 중이에요.","누도에 CFW UPDATE가 보이면 앱에서 확인해 주세요.","새 CFW의 정상 실행과 영구 확정을 확인했어요."};
 try(org.robolectric.android.controller.ActivityController<InstallerHomeActivity> c=Robolectric.buildActivity(InstallerHomeActivity.class).setup()){
 for(int i=0;i<keys.length;i++){InstallerPresentation p=new InstallerPresentation(()->10000);p.begin(i<2?"stock-install-bootstrap":"guided-bootstrap");p.role(i<2?"stock":i<10?"bootstrap":"product");p.stage(keys[i],messages[i],i>=8?393216:196608,393216,"B");p.reply();if(keys[i].equals("done"))p.finish(messagesPlaceholder(),false);deviceFixture(c.get(),i<2?"stock":i<10?"bootstrap":"product",i<2?"stock-install-bootstrap":"guided-bootstrap",i<11);c.get().progress(p.snapshot());capture(c.get().getWindow().getDecorView(),String.format(java.util.Locale.ROOT,"install-%02d-%s",i+1,keys[i]));}
 }
 }

 @Test public void captureUpdateAndPermissions()throws Exception{
 Shadows.shadowOf(RuntimeEnvironment.getApplication()).declareComponentUnbindable(new ComponentName(RuntimeEnvironment.getApplication(),MaintenanceService.class));
 io.opennoodoe.app.UiThemeSettings.setMode(RuntimeEnvironment.getApplication(),2);
 try(org.robolectric.android.controller.ActivityController<InstallerHomeActivity> c=Robolectric.buildActivity(InstallerHomeActivity.class).setup()){
 String[] keys={"connect","stage","confirm","reboot","health","done"};
 for(int i=0;i<keys.length;i++){InstallerPresentation p=new InstallerPresentation(()->10000);p.begin("update-cfw");p.role("product");p.stage(keys[i],new String[]{"기기와 현재 버전을 확인해요.","새 Product APP을 전송·검증해요.","검증한 업데이트를 확정해요.","누도가 다시 시작해요.","CFW UPDATE 화면을 앱에서 확인해 주세요.","업데이트가 완료됐어요."}[i],i>=2?393216:196608,393216,"B");p.reply();if(keys[i].equals("done"))p.finish(messagesPlaceholder(),false);deviceFixture(c.get(),"product","update-cfw",i<5);c.get().progress(p.snapshot());capture(c.get().getWindow().getDecorView(),"update-"+(i+1));}
 new PermissionCenter(c.get()).show();android.app.AlertDialog dialog=org.robolectric.shadows.ShadowAlertDialog.getLatestAlertDialog();assertTrue(dialog.isShowing());dialog.dismiss();
 }
 }
}
