package io.opennoodoe.app.companion;
import android.Manifest;
import android.content.*;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.*;
import android.provider.CallLog;
import android.telecom.TelecomManager;
import java.util.*;
import java.io.IOException;

/** Bounded phone authority for contact IDs, permissions and call commands.
 * No raw number is accepted from SPP; no personal data enters event logs. */
public final class PhoneCalls {
 public interface KeySource { long next(); }
 private final Context context;private final CompanionWire wire;private final KeySource keys;private final String address;
 private final Handler main=new Handler(Looper.getMainLooper());
 private List<CallFavorites.Entry> entries=new ArrayList<>();private int favorites,permissions;
 private String resolvedNumber="",resolvedName="";private int resolvedPermissions=-1;
 private long generation=1,nextId=1,desired,visualTarget,visualKey,lastLists=-10000;private String visualIdentity="";
 private List<Long> ids=new ArrayList<>();
 private NoodoeCallService.View call=NoodoeCallService.snapshot();
 public PhoneCalls(Context context,CompanionWire wire,KeySource keys,String address){this.context=context;this.wire=wire;this.keys=keys;this.address=address;}
 private boolean has(String permission){return context.checkSelfPermission(permission)==PackageManager.PERMISSION_GRANTED;}
 private boolean control(){
  return CallPermissions.canControl(context,address);
 }
 private void lists(long now){
  int next=(has(Manifest.permission.READ_CONTACTS)?1:0)|(has(Manifest.permission.READ_CALL_LOG)?2:0)|(has(Manifest.permission.CALL_PHONE)?4:0)|(control()?8:0);
  if(now-lastLists<5000&&next==permissions)return;lastLists=now;permissions=next;
  List<CallFavorites.Entry> fresh=new ArrayList<>();
  if((permissions&1)!=0)fresh.addAll(new CallFavorites(context).get());int split=fresh.size();
  if((permissions&2)!=0)try(Cursor c=context.getContentResolver().query(CallLog.Calls.CONTENT_URI,new String[]{CallLog.Calls.NUMBER,CallLog.Calls.DATE,CallLog.Calls.TYPE},null,null,CallLog.Calls.DATE+" DESC")){
   if(c!=null)while(c.moveToNext()&&fresh.size()-split<10){String number=c.getString(0);if(number!=null&&!number.isEmpty())fresh.add(new CallFavorites.Entry(CallIdentity.lookup(context,number),number,c.getLong(1),c.getInt(2)));}
  }catch(SecurityException e){permissions&=~2;}
  boolean equal=split==favorites&&fresh.size()==entries.size();
  for(int i=0;equal&&i<fresh.size();i++)equal=fresh.get(i).number.equals(entries.get(i).number)&&fresh.get(i).name.equals(entries.get(i).name)&&fresh.get(i).date==entries.get(i).date&&fresh.get(i).type==entries.get(i).type;
  if(!equal){entries=fresh;favorites=split;ids=new ArrayList<>();for(int i=0;i<fresh.size();i++)ids.add(nextId++);generation++;visualIdentity="";}
 }
 public VisualTransfer tick(long now)throws IOException{
  if(!wire.calls)return null;lists(now);call=NoodoeCallService.snapshot();
  if(!call.number.equals(resolvedNumber)||resolvedPermissions!=permissions){
   resolvedNumber=call.number;resolvedPermissions=permissions;resolvedName=CallIdentity.lookup(context,call.number);
  }
  if(!resolvedName.isEmpty())call=new NoodoeCallService.View(call.id,call.state,call.seconds,resolvedName,call.number);
  long target=call.state!=0?call.id:desired;int selected=ids.indexOf(target);
  if(call.state==0&&selected<0){target=ids.isEmpty()?0:ids.get(0);selected=ids.isEmpty()?-1:0;}
  String identity=CallIdentity.key(generation,target,call);
  VisualTransfer image=null;
  if(target!=0&&!identity.equals(visualIdentity)){
   visualIdentity=identity;visualTarget=target;visualKey=keys.next();
   image=PanelCodec.transfer(wire,visualKey,PhonePanelRenderer.calls(entries,favorites,selected,call));
  }else if(target==0){visualTarget=visualKey=0;visualIdentity="";}
  byte[] body=new byte[40+8*entries.size()];System.arraycopy(CompanionWire.words(wire.callCards?2:1,generation,permissions,entries.size(),call.id,call.state,call.seconds,visualTarget,visualKey,wire.callCards&&call.state==0&&selected>=0?entries.get(selected).type:0),0,body,0,40);
  for(int i=0;i<entries.size();i++)System.arraycopy(CompanionWire.words(ids.get(i),i<favorites?0:1),0,body,40+8*i,8);
  byte[] r=wire.send(0x94,body);if(r.length!=16||CompanionWire.u(r,4)!=1||CompanionWire.u(r,8)!=generation)throw new IOException("Call status generation differs");desired=CompanionWire.u(r,12);return image;
 }
 public long command(byte[] data){
  if(data.length!=20||CompanionWire.u(data,0)!=wire.epoch||CompanionWire.u(data,4)!=generation||!wire.ign)return 7;
  long id=CompanionWire.u(data,8),action=CompanionWire.u(data,12);if(action>2)return 1;
  if(action!=0)return control()?NoodoeCallService.act(id,(int)action):1;
  int at=ids.indexOf(id);if(at<0||!has(Manifest.permission.CALL_PHONE)||NoodoeCallService.snapshot().state!=0)return 1;
  String number=entries.get(at).number;long deadline=SystemClock.elapsedRealtime()+1200;
  java.util.concurrent.atomic.AtomicBoolean cancelled=new java.util.concurrent.atomic.AtomicBoolean();
  main.post(()->{if(cancelled.get()||SystemClock.elapsedRealtime()>=deadline)return;try{context.getSystemService(TelecomManager.class).placeCall(Uri.fromParts("tel",number,null),new Bundle());}catch(SecurityException|IllegalArgumentException ignored){}});
  try{while(SystemClock.elapsedRealtime()<deadline){NoodoeCallService.View v=NoodoeCallService.snapshot();if(v.state==2||v.state==3){if(android.telephony.PhoneNumberUtils.compare(number,v.number))return 0;}Thread.sleep(25);}}
  catch(InterruptedException e){Thread.currentThread().interrupt();}finally{cancelled.set(true);}return 7;
 }
}
