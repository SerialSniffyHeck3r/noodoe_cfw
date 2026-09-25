package io.opennoodoe.app.companion;
import android.content.*;import android.content.pm.*;import android.view.*;import android.widget.*;
import java.util.*;import org.junit.*;import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.Config;import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28)
public class NotificationAppPickerTest {
 @Test public void iconLabelPackageAndChoiceKeepPackageIdentity(){
  Context c=RuntimeEnvironment.getApplication();PackageInfo info=new PackageInfo();info.packageName="com.discord";info.applicationInfo=new ApplicationInfo();info.applicationInfo.packageName=info.packageName;info.applicationInfo.nonLocalizedLabel="Discord";
  Shadows.shadowOf(c.getPackageManager()).installPackage(info);
  var rows=NotificationAppPicker.rows(c,Arrays.asList("com.discord","missing.app","com.discord"));assertEquals(2,rows.size());
  var row=rows.stream().filter(r->r.packageName.equals("com.discord")).findFirst().get();assertEquals("Discord",row.name);assertNotNull(row.icon);
  Set<String> selected=new HashSet<>();ViewGroup view=(ViewGroup)NotificationAppPicker.row(c,row,selected);
  assertTrue(view.getChildAt(0) instanceof ImageView);ViewGroup text=(ViewGroup)view.getChildAt(1);
  assertEquals("Discord",((TextView)text.getChildAt(0)).getText().toString());assertEquals("com.discord",((TextView)text.getChildAt(1)).getText().toString());
  view.performClick();assertTrue(selected.contains("com.discord"));view.performClick();assertTrue(selected.isEmpty());
 }
}
