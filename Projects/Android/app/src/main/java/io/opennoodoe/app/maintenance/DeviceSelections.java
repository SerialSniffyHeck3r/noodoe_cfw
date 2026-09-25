package io.opennoodoe.app.maintenance;

import android.content.Context;
import android.content.SharedPreferences;

/** Only presentation choices live here. Durable UID/transaction evidence remains
 * in InstallJournal/TargetBinding and is never erased by resetting these choices. */
final class DeviceSelections {
    private final SharedPreferences prefs;
    DeviceSelections(Context context) { prefs=context.getSharedPreferences("installer-selections-v2",0); }
    static String key(String address) { return address==null ? "unselected" : address.replace(":", "").toUpperCase(java.util.Locale.ROOT); }
    String address() { return prefs.getString("active", null); }
    void select(String address) { prefs.edit().putString("active",address).apply(); }
    String bundle(String address) { return prefs.getString("bundle."+key(address),null); }
    void bundle(String address,String hash) { prefs.edit().putString("bundle."+key(address),hash).apply(); }
    void reset() { String address=address(); prefs.edit().remove("active").remove("bundle."+key(address)).remove("bundle.unselected").apply(); }
    String display(String address) { return prefs.getString("display."+key(address),null); }
    void display(String address,String text) { prefs.edit().putString("display."+key(address),text).apply(); }
}
