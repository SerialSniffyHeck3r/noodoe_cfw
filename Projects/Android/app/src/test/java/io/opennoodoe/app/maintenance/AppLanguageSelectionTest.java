package io.opennoodoe.app.maintenance;
import android.content.*;
import android.content.res.*;
import android.view.*;
import android.widget.*;
import io.opennoodoe.app.*;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.annotation.*;
import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28)
public class AppLanguageSelectionTest {
 @After public void reset(){AppLanguageSettings.set(RuntimeEnvironment.getApplication(),AppLanguageSettings.SYSTEM);}
 @Test public void systemSelectionDoesNotInheritPreviouslyWrappedKoreanContext(){
  Context app=RuntimeEnvironment.getApplication();AppLanguageSettings.set(app,"ko");Context korean=AppLanguageSettings.wrap(app);
  AppLanguageSettings.set(korean,AppLanguageSettings.SYSTEM);
  assertEquals(AppLanguageSettings.resolvedTag(app),AppLanguageSettings.resolvedTag(korean));
  assertEquals(AppLanguageSettings.SYSTEM,AppLanguageSettings.tag(korean));
 }
 @Test public void languagePickerIsVisibleWithoutOpeningConnectionFold(){
  Context app=RuntimeEnvironment.getApplication();AppLanguageSettings.set(app,"en");
  Shadows.shadowOf(RuntimeEnvironment.getApplication()).declareComponentUnbindable(new ComponentName(app,MaintenanceService.class));
  try(org.robolectric.android.controller.ActivityController<InstallerHomeActivity> c=Robolectric.buildActivity(InstallerHomeActivity.class).setup()){
   Button picker=find(c.get().getWindow().getDecorView(),UiText.text(R.string.companion_0110,""));assertNotNull(picker);
   for(View v=picker;v!=null;v=v.getParent() instanceof View?(View)v.getParent():null)assertEquals(View.VISIBLE,v.getVisibility());
   assertTrue(picker.performClick());android.app.AlertDialog dialog=org.robolectric.shadows.ShadowAlertDialog.getLatestAlertDialog();assertNotNull(dialog);
   assertEquals(7,dialog.getListView().getAdapter().getCount());
  }
 }
 private static Button find(View view,String title){if(view instanceof Button&&title.contentEquals(((Button)view).getText()))return (Button)view;
  if(view instanceof ViewGroup)for(int i=0;i<((ViewGroup)view).getChildCount();i++){Button b=find(((ViewGroup)view).getChildAt(i),title);if(b!=null)return b;}return null;}
}
