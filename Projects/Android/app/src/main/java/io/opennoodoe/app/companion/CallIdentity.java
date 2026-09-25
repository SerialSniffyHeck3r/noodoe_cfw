package io.opennoodoe.app.companion;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.ContactsContract;

/** Match the actual call number using Android's normalized PhoneLookup.
 * Never attach an unrelated cached call-log name to a new number. */
final class CallIdentity {
 static String lookup(Context context,String number){
  if(number==null||number.isEmpty())return "";
  try(Cursor c=context.getContentResolver().query(Uri.withAppendedPath(ContactsContract.PhoneLookup.CONTENT_FILTER_URI,Uri.encode(number)),new String[]{ContactsContract.PhoneLookup.DISPLAY_NAME},null,null,null)){
   return c!=null&&c.moveToFirst()&&c.getString(0)!=null?c.getString(0):"";
  }catch(RuntimeException unavailable){return "";}
 }
 static String key(long generation,long target,NoodoeCallService.View call){return generation+":"+target+":"+call.state+":"+call.name+":"+call.number;}
}
