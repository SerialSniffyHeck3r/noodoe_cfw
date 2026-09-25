package io.opennoodoe.app.companion;

import android.app.*;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.service.notification.StatusBarNotification;

/** An explicitly requested local notification exercises the real listener,
 * CJK renderer and normal SPP queue. It never sends a message to another person. */
public final class NotificationTest {
 private static final String TAG="noodoe.notification.test",CHANNEL="noodoe-test";
 private static final int ID=7401;
 private NotificationTest(){}
 public static boolean matches(Context c,StatusBarNotification n){
  return c.getPackageName().equals(n.getPackageName())&&TAG.equals(n.getTag())&&n.getId()==ID;
 }
 public static String send(Context c){
  if(CompanionNotifications.instance==null)return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0664,"권한·연동 준비에서 알림 접근을 허용해 주세요.");
  NotificationManager nm=(NotificationManager)c.getSystemService(Context.NOTIFICATION_SERVICE);
  if(nm==null||(Build.VERSION.SDK_INT>=24&&!nm.areNotificationsEnabled())||(Build.VERSION.SDK_INT>=33&&c.checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS)!=PackageManager.PERMISSION_GRANTED))
   return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0665,"휴대폰 설정에서 이 앱의 알림 표시를 허용해 주세요.");
  if(Build.VERSION.SDK_INT>=26){nm.createNotificationChannel(new NotificationChannel(CHANNEL,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0666,"누도 알림 테스트"),NotificationManager.IMPORTANCE_DEFAULT));
   if(nm.getNotificationChannel(CHANNEL).getImportance()==NotificationManager.IMPORTANCE_NONE)return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0667,"휴대폰 설정에서 ‘누도 알림 테스트’ 알림을 켜 주세요.");}
  Notification.Builder b=Build.VERSION.SDK_INT>=26?new Notification.Builder(c,CHANNEL):new Notification.Builder(c);
  nm.notify(TAG,ID,b.setSmallIcon(io.opennoodoe.app.R.drawable.ic_phone_round).setContentTitle(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0668,"알림 테스트 · 通知テスト"))
    .setContentText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0669,"누도에 잘 도착했나요? 臺灣 · 日本 · 한국")).setAutoCancel(true).build());
  return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0670,"테스트 알림을 만들었어요. 연결 중인 누도의 휴대전화 화면에서 위·아래 버튼으로 확인해 주세요.");
 }
}
