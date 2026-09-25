package io.opennoodoe.app.companion;
import android.app.*;
import android.content.*;
import android.service.notification.StatusBarNotification;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.annotation.Config;
import java.util.Collections;
import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28)
public class NotificationTestPathTest {
 @Test public void testNotificationUsesHistoryWithoutEnablingOtherAppNotifications(){
  Context c=RuntimeEnvironment.getApplication();c.getSharedPreferences("companion",0).edit().putStringSet("notification.apps",Collections.emptySet()).commit();
  NotificationHistory h=new NotificationHistory();h.activate(c);
  Notification n=new Notification.Builder(c,"test").setSmallIcon(android.R.drawable.ic_dialog_email).setContentTitle("通知テスト").setContentText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0669,"누도에 잘 도착했나요? 臺灣 · 日本 · 한국")).build();
  StatusBarNotification test=new StatusBarNotification(c.getPackageName(),c.getPackageName(),7401,"noodoe.notification.test",android.os.Process.myUid(),0,0,n,android.os.Process.myUserHandle(),System.currentTimeMillis());
  h.posted(c,test);assertEquals(1,h.snapshot(c).size());assertNull(h.snapshot(c).get(0).reply);
  StatusBarNotification ordinary=new StatusBarNotification(c.getPackageName(),c.getPackageName(),7402,null,android.os.Process.myUid(),0,0,n,android.os.Process.myUserHandle(),System.currentTimeMillis());
  h.posted(c,ordinary);assertEquals(1,h.snapshot(c).size());
  StatusBarNotification spoof=new StatusBarNotification("other.app","other.app",7401,"noodoe.notification.test",0,0,0,n,android.os.Process.myUserHandle(),System.currentTimeMillis());
  assertFalse(NotificationTest.matches(c,spoof));
  assertTrue(c.getSharedPreferences("companion",0).getStringSet("notification.apps",Collections.emptySet()).isEmpty());
 }
 @Test public void missingListenerIsExplained(){CompanionNotifications.instance=null;assertTrue(NotificationTest.send(RuntimeEnvironment.getApplication()).contains(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0664,"권한·연동 준비에서 알림 접근을 허용해 주세요.")));}
}
