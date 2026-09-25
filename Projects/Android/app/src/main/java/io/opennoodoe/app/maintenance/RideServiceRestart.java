package io.opennoodoe.app.maintenance;
import android.content.*;
import io.opennoodoe.app.companion.DrivingMode;
/** Only an identified Product riding session may resume after OS process
 * reclaim. No installer action, firmware command, or photo job is replayed. */
final class RideServiceRestart {
 private final Context c;private final SharedPreferences prefs;
 RideServiceRestart(Context c){this.c=c;prefs=c.getSharedPreferences("ride-service-restart",0);}
 void productConnected(String address){prefs.edit().putString("address",address).commit();}
 void clear(){prefs.edit().remove("address").commit();}
 boolean allowed(String selected,boolean workflowAllows){return workflowAllows&&selected!=null&&selected.equals(prefs.getString("address",null))&&new DrivingMode(c).automatic(selected);}
}
