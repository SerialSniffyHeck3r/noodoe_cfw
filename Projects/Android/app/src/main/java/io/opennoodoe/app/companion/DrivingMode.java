package io.opennoodoe.app.companion;

import android.content.Context;
import android.content.SharedPreferences;

/** Device-scoped intent survives process death. UPDATE is deliberately sticky
 * until the installer confirms the new firmware, never just a socket close. */
public final class DrivingMode {
 public static final int AUTO=0,MANUAL=1,UPDATE=2;
 private final SharedPreferences prefs;
 public DrivingMode(Context c){prefs=c.getSharedPreferences("driving-mode",Context.MODE_PRIVATE);}
 private String key(String address){return address==null?"":address.toUpperCase(java.util.Locale.ROOT);}
 public int get(String address){return prefs.getInt(key(address),AUTO);}
 public boolean automatic(String address){return address!=null&&!address.isEmpty()&&get(address)==AUTO;}
 public boolean set(String address,int mode){return address!=null&&!address.isEmpty()&&mode>=AUTO&&mode<=UPDATE&&prefs.edit().putInt(key(address),mode).commit();}
 public boolean beginUpdate(String address){return get(address)!=AUTO&&set(address,UPDATE);}
}
