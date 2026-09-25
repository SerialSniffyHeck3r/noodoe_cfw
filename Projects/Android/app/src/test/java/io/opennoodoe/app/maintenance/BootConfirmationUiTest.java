package io.opennoodoe.app.maintenance;
import android.app.*;import android.content.*;import android.graphics.*;import android.os.Looper;import android.view.*;import android.widget.*;
import io.opennoodoe.app.installer.*;import io.opennoodoe.app.R;
import org.junit.*;import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.*;import org.robolectric.shadows.ShadowAlertDialog;
import java.io.*;import java.lang.reflect.*;import java.util.*;import java.util.concurrent.*;
import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class) @Config(sdk=28,qualifiers="w393dp-h852dp-mdpi") @GraphicsMode(GraphicsMode.Mode.NATIVE)
public class BootConfirmationUiTest {
 private void field(Object target,String name,Object v)throws Exception {Field f=target.getClass().getDeclaredField(name);f.setAccessible(true);f.set(target,v);}
 private Object field(Object target,String name)throws Exception {Field f=target.getClass().getDeclaredField(name);f.setAccessible(true);return f.get(target);}
 @Test public void requestIsPinnedAndDialogIsExplicitInBothThemes()throws Exception {
  Context context=RuntimeEnvironment.getApplication();Shadows.shadowOf(RuntimeEnvironment.getApplication()).declareComponentUnbindable(new ComponentName(context,MaintenanceService.class));
  for(int mode:new int[]{1,2}){
   io.opennoodoe.app.UiThemeSettings.setMode(context,mode);
   io.opennoodoe.app.AppLanguageSettings.set(context,"ko");
   try(org.robolectric.android.controller.ActivityController<InstallerHomeActivity> c=Robolectric.buildActivity(InstallerHomeActivity.class).setup()){
    InstallerHomeActivity a=c.get();BootConfirmationView view=(BootConfirmationView)field(a,"bootConfirmation");
    view.render(true,"candidate-A",145,true);AlertDialog dialog=ShadowAlertDialog.getLatestAlertDialog();assertTrue(dialog.isShowing());
    assertNotNull(dialog.getButton(AlertDialog.BUTTON_POSITIVE));assertTrue(view.isShown());
    capture(dialog.getWindow().getDecorView(),"dialog-"+mode);
    dialog.getButton(AlertDialog.BUTTON_NEGATIVE).performClick();Shadows.shadowOf(Looper.getMainLooper()).idle();assertFalse(dialog.isShowing());
    // The required action is above, not inside, the long scroll container.
    LinearLayout root=(LinearLayout)view.getParent();assertEquals(view,root.getChildAt(0));assertTrue(root.getChildAt(1)instanceof ScrollView);
    ((ScrollView)root.getChildAt(1)).scrollTo(0,2000);view.render(true,"candidate-A",144,true);
    assertFalse(dialog.isShowing());capture(a.getWindow().getDecorView(),"pinned-"+mode);
    view.render(false,"",0,true);assertEquals(View.GONE,view.getVisibility());
   }
  }
 }
 @Test public void oldDialogAndDuplicateClicksCannotApproveNewRequest(){
  try(org.robolectric.android.controller.ActivityController<Activity> c=Robolectric.buildActivity(Activity.class).setup()){
   List<String> approvals=new ArrayList<>();BootConfirmationView v=new BootConfirmationView(c.get(),approvals::add);c.get().setContentView(v);
   v.render(true,"A",140,true);AlertDialog old=ShadowAlertDialog.getLatestAlertDialog();Button stale=old.getButton(AlertDialog.BUTTON_POSITIVE);
   v.render(true,"B",130,true);assertFalse(old.isShowing());stale.performClick();Shadows.shadowOf(Looper.getMainLooper()).idle();assertTrue(approvals.isEmpty());
   AlertDialog current=ShadowAlertDialog.getLatestAlertDialog();current.getButton(AlertDialog.BUTTON_POSITIVE).performClick();Shadows.shadowOf(Looper.getMainLooper()).idle();assertEquals(Collections.singletonList("B"),approvals);
   v.render(false,"",0,true);stale.performClick();Shadows.shadowOf(Looper.getMainLooper()).idle();assertEquals(1,approvals.size());
  }
 }
 @Test public void serviceWaitPollsWithoutApprovingAndRejectsStaleTicket()throws Exception {
  org.robolectric.android.controller.ServiceController<MaintenanceService> c=Robolectric.buildService(MaintenanceService.class).create();MaintenanceService s=c.get();
  ExecutorService worker=Executors.newSingleThreadExecutor();
  try{
   field(s,"busy",true);SessionEpoch epochs=(SessionEpoch)field(s,"epochs");
   Method m=MaintenanceService.class.getDeclaredMethod("observer",long.class);m.setAccessible(true);
   StockUpdateSession.Progress p=(StockUpdateSession.Progress)m.invoke(s,epochs.current());CountDownLatch polled=new CountDownLatch(1);
   Future<?> f=worker.submit(()->{try{p.awaitScreen("exact-candidate",System.nanoTime()+10_000_000_000L,polled::countDown);}catch(Exception e){throw new RuntimeException(e);}});
   assertTrue(polled.await(2,TimeUnit.SECONDS));assertTrue(s.canConfirmScreen());String key=s.screenConfirmationKey();
   s.confirmScreen("old-connection");assertFalse(f.isDone());assertTrue(s.canConfirmScreen());
   s.confirmScreen(key);assertFalse(s.canConfirmScreen());f.get(2,TimeUnit.SECONDS);assertFalse(s.screenVisible());
   // Expired user action is not a generic transport timeout/reconnect.
   try{p.awaitScreen("next",System.nanoTime()-1,()->fail());fail();}catch(StockUpdateSession.ScreenConfirmationTimeout expected){}
   assertFalse(s.screenVisible());
  }finally{worker.shutdownNow();c.destroy();}
 }
 @Test public void backgroundDoesNotApproveAndForegroundRecreationReopensPrompt(){
  try(org.robolectric.android.controller.ActivityController<Activity> c=Robolectric.buildActivity(Activity.class).setup()){
   List<String> approvals=new ArrayList<>();BootConfirmationView v=new BootConfirmationView(c.get(),approvals::add);c.get().setContentView(v);
   v.render(true,"A",130,false);assertTrue(approvals.isEmpty());v.render(true,"A",129,true);assertTrue(ShadowAlertDialog.getLatestAlertDialog().isShowing());v.dismiss();
   BootConfirmationView recreated=new BootConfirmationView(c.get(),approvals::add);c.get().setContentView(recreated);recreated.render(true,"A",128,true);
   assertTrue(ShadowAlertDialog.getLatestAlertDialog().isShowing());assertTrue(approvals.isEmpty());recreated.render(false,"",0,true);
  }
 }
 private void capture(View root,String name)throws Exception {
  Shadows.shadowOf(Looper.getMainLooper()).idle();
  root.measure(View.MeasureSpec.makeMeasureSpec(393,View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(852,View.MeasureSpec.EXACTLY));root.layout(0,0,393,852);
  Bitmap b=Bitmap.createBitmap(393,852,Bitmap.Config.ARGB_8888);root.draw(new Canvas(b));File dir=new File("build/boot-confirmation-preview");assertTrue(dir.isDirectory()||dir.mkdirs());
  try(FileOutputStream out=new FileOutputStream(new File(dir,name+".png"))){b.compress(Bitmap.CompressFormat.PNG,100,out);}b.recycle();
 }
}
