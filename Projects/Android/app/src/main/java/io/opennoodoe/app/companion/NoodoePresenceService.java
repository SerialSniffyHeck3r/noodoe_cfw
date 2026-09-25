package io.opennoodoe.app.companion;

import android.companion.CompanionDeviceService;
import android.content.Intent;
import io.opennoodoe.app.maintenance.MaintenanceService;

/** Presence is only a wake-up hint. The connection owner still identifies the
 * peer and refuses stock/bootstrap roles before sending companion content. */
@android.annotation.TargetApi(31)
public final class NoodoePresenceService extends CompanionDeviceService {
 @Override public void onDeviceAppeared(String address){
  if(!new DrivingMode(this).automatic(address))return;
  try{startForegroundService(new Intent(this,MaintenanceService.class).putExtra("auto-device",address));}
  catch(RuntimeException denied){getSharedPreferences("companion",MODE_PRIVATE).edit().putBoolean("background-start-denied",true).apply();}
 }
 @Override public void onDeviceDisappeared(String address){/* Bounded reconnect belongs to the socket owner. */}
}
