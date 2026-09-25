package io.opennoodoe.app.companion;
import android.location.Location;
import android.os.Build;
import java.util.*;
/** Motion bearing only. Phone orientation/magnetometer never steers the map.
 * Unknown/poor fixes explicitly invalidate GPS, never inject vehicle speed. */
public final class PhoneGpsEncoder {
 private PhoneGpsEncoder(){}
 private static boolean finite(double x){return !Double.isNaN(x)&&!Double.isInfinite(x);}
 public static byte[] encode(Location l){
  Calendar c=Calendar.getInstance(TimeZone.getTimeZone("UTC"));c.setTimeInMillis(l.getTime());
  long utc=((c.get(Calendar.HOUR_OF_DAY)*60L+c.get(Calendar.MINUTE))*60+c.get(Calendar.SECOND))*1000+c.get(Calendar.MILLISECOND);
  boolean good=l.hasAccuracy()&&finite(l.getAccuracy())&&l.getAccuracy()<=30&&l.getAccuracy()>=0&&finite(l.getLatitude())&&finite(l.getLongitude())&&Math.abs(l.getLatitude())<=90&&Math.abs(l.getLongitude())<=180;
  long fields=2|4,speed=0,bearing=0;
  if(good){fields|=1;if(l.hasSpeed()&&finite(l.getSpeed())&&l.getSpeed()>=0&&l.getSpeed()<=100){speed=Math.round(l.getSpeed()*1000);fields|=8;}
   if(speed>=2000&&l.hasBearing()&&finite(l.getBearing())&&(Build.VERSION.SDK_INT<26||!l.hasBearingAccuracy()||l.getBearingAccuracyDegrees()<=30)){bearing=Math.round(l.getBearing()*1000)%360000;fields|=16;}}
  return CompanionWire.words(fields,good?Math.round(l.getLatitude()*1e7):0,good?Math.round(l.getLongitude()*1e7):0,speed,bearing,0,utc,c.get(Calendar.YEAR)*10000+(c.get(Calendar.MONTH)+1)*100+c.get(Calendar.DAY_OF_MONTH),0,0,good?1:0);
 }
}
