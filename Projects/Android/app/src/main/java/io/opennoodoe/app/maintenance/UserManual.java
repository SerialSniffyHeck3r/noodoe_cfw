package io.opennoodoe.app.maintenance;
import android.app.Activity;import android.content.Intent;import android.content.ActivityNotFoundException;import android.net.Uri;import android.widget.Toast;import io.opennoodoe.app.R;
/** Read-only documentation navigation. Never changes the connection or install session. */
public final class UserManual {
 public static final String URL="https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/User/README.md";
 private UserManual(){}
 public static void open(Activity host){try{host.startActivity(new Intent(Intent.ACTION_VIEW,Uri.parse(URL)));}catch(ActivityNotFoundException|SecurityException e){Toast.makeText(host,R.string.manual_browser_missing,Toast.LENGTH_LONG).show();}}
}
