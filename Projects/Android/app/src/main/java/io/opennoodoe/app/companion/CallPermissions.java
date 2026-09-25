package io.opennoodoe.app.companion;

import android.content.Context;
import android.os.Build;
import android.telecom.TelecomManager;

/** Companion call control is an app-op granted by CDM, not a runtime
 * permission grant. Both the settings UI and the wire use Telecom's answer. */
public final class CallPermissions {
 private CallPermissions(){}
 public static boolean canControl(Context context,String address){
  if(Build.VERSION.SDK_INT<31||!CompanionAssociation.associated(context,address))return false;
  try{
   TelecomManager telecom=context.getSystemService(TelecomManager.class);
   return telecom!=null&&telecom.hasManageOngoingCallsPermission();
  }catch(RuntimeException unavailable){return false;}
 }
}
