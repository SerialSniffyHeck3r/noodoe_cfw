package io.opennoodoe.app.companion;

import android.net.Uri;
import android.os.*;
import android.telecom.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicBoolean;

/** Non-UI companion InCallService. The installed dialer keeps ringtone/UI and
 * audio routing. Call objects are touched only on Android's main thread. */
public final class NoodoeCallService extends InCallService {
 public static final class View {
  public final long id,seconds;public final int state;public final String name,number;
  View(long id,int state,long seconds,String name,String number){this.id=id;this.state=state;this.seconds=seconds;this.name=bound(name,256);this.number=bound(number,128);}
 }
 private static String bound(String value,int max){return value==null?"":value.substring(0,value.offsetByCodePoints(0,Math.min(max,value.codePointCount(0,value.length()))));}
 private static volatile NoodoeCallService instance;
 private static volatile View view=new View(0,0,0,"","");
 private static long sequence;
 private final LinkedHashMap<Call,Long> ids=new LinkedHashMap<>();
 private final Handler main=new Handler(Looper.getMainLooper());
 private final Call.Callback callback=new Call.Callback(){
  @Override public void onStateChanged(Call call,int state){publish();}
  @Override public void onDetailsChanged(Call call,Call.Details details){publish();}
 };
 @Override public void onCreate(){super.onCreate();instance=this;}
 @Override public void onCallAdded(Call call){
  if(ids.size()>=8)return;
  long id=(++sequence&0x7fffffffL)|0x80000000L;ids.put(call,id);call.registerCallback(callback,main);publish();
 }
 @Override public void onCallRemoved(Call call){call.unregisterCallback(callback);ids.remove(call);publish();}
 @Override public void onDestroy(){for(Call c:ids.keySet())c.unregisterCallback(callback);ids.clear();if(instance==this){instance=null;view=new View(0,0,0,"","");}super.onDestroy();}
 @SuppressWarnings("deprecation") private static int state(Call c){
  switch(c.getState()){
   case Call.STATE_RINGING:return 1;
   case Call.STATE_DIALING:case Call.STATE_CONNECTING:case Call.STATE_NEW:case Call.STATE_SELECT_PHONE_ACCOUNT:return 2;
   case Call.STATE_ACTIVE:return 3;case Call.STATE_HOLDING:return 4;case Call.STATE_DISCONNECTING:return 5;default:return 0;
  }
 }
 private void publish(){
  Call chosen=null;for(Call c:ids.keySet())if(state(c)!=0&&(chosen==null||state(c)==1))chosen=c;
  if(chosen==null){view=new View(0,0,0,"","");return;}
  Call.Details d=chosen.getDetails();Uri handle=d==null?null:d.getHandle();String number=handle==null?"":handle.getSchemeSpecificPart();
  String name=d==null?null:d.getCallerDisplayName();long since=d==null?0:d.getConnectTimeMillis();
  view=new View(ids.get(chosen),state(chosen),since>0?Math.max(0,(System.currentTimeMillis()-since)/1000):0,name==null||name.isEmpty()?number:name,number);
 }
 public static View snapshot(){NoodoeCallService s=instance;if(s!=null)s.main.post(s::publish);return view;}
 public static boolean available(){return instance!=null;}
 /** A timed-out dispatch cannot execute later. Once sent to Telecom, result
  * is confirmed by a changed callback snapshot; UNKNOWN is never retried. */
 public static long act(long id,int action){
  NoodoeCallService s=instance;View beforeView=view;if(s==null||beforeView.id!=id||(action==1&&beforeView.state!=1))return 7;
  AtomicBoolean cancelled=new AtomicBoolean(),issued=new AtomicBoolean();
  long deadline=SystemClock.elapsedRealtime()+1200;
  s.main.post(()->{try{if(cancelled.get()||SystemClock.elapsedRealtime()>=deadline)return;
   for(Map.Entry<Call,Long> e:s.ids.entrySet())if(e.getValue()==id){Call c=e.getKey();int before=state(c);
    if(action==1&&before==1){c.answer(VideoProfile.STATE_AUDIO_ONLY);issued.set(true);}
    else if(action==2&&before!=0){if(before==1)c.reject(false,null);else c.disconnect();issued.set(true);}break;}
  }catch(SecurityException|IllegalStateException ignored){}});
  try{while(SystemClock.elapsedRealtime()<deadline){View current=view;if(issued.get()&&(action==2&&current.id!=id||action==1&&current.id==id&&current.state==3))return 0;Thread.sleep(25);}}
  catch(InterruptedException e){Thread.currentThread().interrupt();}finally{cancelled.set(true);}return 7;
 }
}
