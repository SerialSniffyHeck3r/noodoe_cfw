package io.opennoodoe.app.companion;
import android.content.*;
import java.util.*;
/** User-authored text stays in app-private preferences, never diagnostic logs. */
public final class ReplySettings {
 public static final String DEFAULT_SIGNATURE="Sent from my Noodoe dashboard.";
 public final List<String> replies;public final String signature;public final long revision;
 public ReplySettings(Context context){android.content.SharedPreferences p=context.getSharedPreferences("quick-reply",0);
  ArrayList<String> r=new ArrayList<>();for(int i=0;i<5;i++){String s=p.getString("reply"+i,"").trim();if(!s.isEmpty())r.add(s);}replies=Collections.unmodifiableList(r);
  signature=p.getString("signature",DEFAULT_SIGNATURE);revision=p.getLong("revision",1);}
 public static void save(Context c,String[] text,String signature){if(text.length!=5||signature.length()>160)throw new IllegalArgumentException();
  android.content.SharedPreferences p=c.getSharedPreferences("quick-reply",0);android.content.SharedPreferences.Editor e=p.edit();
  for(int i=0;i<5;i++){if(text[i].length()>120)throw new IllegalArgumentException();e.putString("reply"+i,text[i].trim());}
  long rev=(p.getLong("revision",1)+1)&0xffffffffL;if(rev==0)rev=1;
  if(!e.putString("signature",signature.trim()).putLong("revision",rev).commit())throw new IllegalStateException("Settings could not be saved");}
 public String message(int index){if(index<0||index>=replies.size())throw new IllegalArgumentException();return replies.get(index)+(signature.isEmpty()?"":"\n\n"+signature);}
}
