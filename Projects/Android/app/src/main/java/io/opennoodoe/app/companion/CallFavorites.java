package io.opennoodoe.app.companion;
import android.content.*;
import java.util.*;
import org.json.*;
/** Private phone preferences; content is excluded from diagnostic logs. */
public final class CallFavorites {
 public static final class Entry {
  public final String name,number;public final long date;public final int type;
  public Entry(String name,String number){this(name,number,0,0);}
  public Entry(String name,String number,long date,int type){this.name=name==null?"":name;this.number=number==null?"":number;this.date=Math.max(0,date);this.type=type>=1&&type<=7?type:0;}
 }
 private final SharedPreferences prefs;
 public CallFavorites(Context c){prefs=c.getSharedPreferences("call-favorites",0);}
 public List<Entry> get(){List<Entry> out=new ArrayList<>();try{JSONArray a=new JSONArray(prefs.getString("entries","[]"));for(int i=0;i<Math.min(10,a.length());i++){JSONObject o=a.getJSONObject(i);out.add(new Entry(o.optString("name"),o.getString("number")));}}catch(JSONException ignored){}return out;}
 public boolean save(List<Entry> entries){if(entries.size()>10)return false;JSONArray a=new JSONArray();try{for(Entry e:entries){if(e.number.isEmpty()||e.number.length()>128||e.name.length()>256)return false;JSONObject o=new JSONObject();o.put("name",e.name);o.put("number",e.number);a.put(o);}}catch(JSONException e){return false;}return prefs.edit().putString("entries",a.toString()).commit();}
}
