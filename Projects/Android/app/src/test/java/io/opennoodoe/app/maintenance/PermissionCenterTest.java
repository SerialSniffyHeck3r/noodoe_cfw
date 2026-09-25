package io.opennoodoe.app.maintenance;
import android.Manifest;import android.content.*;import android.provider.Settings;
import io.opennoodoe.app.companion.CompanionNotifications;
import org.junit.*;import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.Config;import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28)
public class PermissionCenterTest {
 @Before public void unbound(){Shadows.shadowOf(RuntimeEnvironment.getApplication()).declareComponentUnbindable(new ComponentName(RuntimeEnvironment.getApplication(),MaintenanceService.class));}
 @Test public void otherNotificationListenersCannotSatisfyOurs(){try(org.robolectric.android.controller.ActivityController<InstallerHomeActivity> c=Robolectric.buildActivity(InstallerHomeActivity.class).setup()){
  PermissionCenter p=new PermissionCenter(c.get());Settings.Secure.putString(c.get().getContentResolver(),"enabled_notification_listeners","other.app/.Listener");assertFalse(p.notifications());
  Settings.Secure.putString(c.get().getContentResolver(),"enabled_notification_listeners",new ComponentName(c.get(),CompanionNotifications.class).flattenToString());assertTrue(p.notifications());
 }}
 @Test public void missingLocationDoesNotPretendGpsReady(){try(org.robolectric.android.controller.ActivityController<InstallerHomeActivity> c=Robolectric.buildActivity(InstallerHomeActivity.class).setup()){
  PermissionCenter p=new PermissionCenter(c.get());assertTrue(p.summary().contains("GPS "+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0252,"설정 필요")));Shadows.shadowOf(RuntimeEnvironment.getApplication()).grantPermissions(Manifest.permission.ACCESS_FINE_LOCATION);assertTrue(p.summary().contains("GPS "+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0251,"준비됨")));
 }}
 @Test @Config(sdk=34) public void modernPermissionsRemainUserMediated(){try(org.robolectric.android.controller.ActivityController<InstallerHomeActivity> c=Robolectric.buildActivity(InstallerHomeActivity.class).setup()){
  PermissionCenter p=new PermissionCenter(c.get());p.show();android.app.AlertDialog d=org.robolectric.shadows.ShadowAlertDialog.getLatestAlertDialog();
  android.widget.Button location=find(d.getWindow().getDecorView(),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0256,"정확한 위치 · GPS 궤적"));assertNotNull(location);location.performClick();
  String[] requested=Shadows.shadowOf(c.get()).getLastRequestedPermission().requestedPermissions;
  assertArrayEquals(new String[]{Manifest.permission.ACCESS_FINE_LOCATION,Manifest.permission.ACCESS_COARSE_LOCATION},requested);
  assertFalse(p.granted(Manifest.permission.ACCESS_FINE_LOCATION));
  p.show();d=org.robolectric.shadows.ShadowAlertDialog.getLatestAlertDialog();assertNotNull(find(d.getWindow().getDecorView(),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0267,"연결·진행 알림 표시")));assertNotNull(find(d.getWindow().getDecorView(),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0263,"백그라운드 자동 연결 GPS · 항상 위치 허용")));p.dismiss();
 }}
 private android.widget.Button find(android.view.View v,String text){if(v instanceof android.widget.Button&&((android.widget.Button)v).getText().toString().contains(text))return (android.widget.Button)v;if(v instanceof android.view.ViewGroup)for(int i=0;i<((android.view.ViewGroup)v).getChildCount();i++){android.widget.Button b=find(((android.view.ViewGroup)v).getChildAt(i),text);if(b!=null)return b;}return null;}
}
