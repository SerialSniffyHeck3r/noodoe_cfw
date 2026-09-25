package io.opennoodoe.app.companion;
import android.content.SharedPreferences;
import android.service.notification.NotificationListenerService;
/** Live notification admission only. Bluetooth and pixel transfer stay with
 * the single connection owner, independent of this Android callback thread. */
public final class CompanionNotifications extends NotificationListenerService {
 public static volatile CompanionNotifications instance;
 public final NotificationHistory history=new NotificationHistory();
 private final SharedPreferences.OnSharedPreferenceChangeListener settings=(p,key)->{if("notification.apps".equals(key))history.selectionChanged(this);};
 @Override public void onListenerConnected(){
  history.activate(this);getSharedPreferences("companion",0).registerOnSharedPreferenceChangeListener(settings);instance=this;
 }
 @Override public void onNotificationPosted(android.service.notification.StatusBarNotification n){history.posted(this,n);}
 @Override public void onNotificationRemoved(android.service.notification.StatusBarNotification n){if(n!=null)history.removed(n.getKey());}
 private void detach(){if(instance==this)instance=null;history.deactivate();getSharedPreferences("companion",0).unregisterOnSharedPreferenceChangeListener(settings);}
 @Override public void onListenerDisconnected(){detach();}
 @Override public void onDestroy(){detach();super.onDestroy();}
}
