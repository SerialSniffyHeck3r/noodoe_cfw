package io.opennoodoe.app.companion;
import io.opennoodoe.app.protocol.ndcp.NdcpClient;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.ArrayBlockingQueue;
/** One bounded command per service iteration. This class never creates a
 * socket or resends an uncertain mutation. UI sees copied, device-owned data. */
public final class DeviceSettingsSession {
 public static final class Edit {
  final int type;final long field,value,expected;final String text;
  Edit(int t,long f,long v,long e,String s){type=t;field=f;value=v;expected=e;text=s;}
 }
 public interface Audit {void record(String action,long field,long result)throws IOException;}
 private final Audit audit;
 private final CompanionWire wire;
 private final ArrayBlockingQueue<Edit> queue=new ArrayBlockingQueue<>(4);
 private final ArrayList<long[]> building=new ArrayList<>();
 private volatile List<long[]> rows=Collections.emptyList();
 private volatile Map<Long,String> texts=Collections.emptyMap();
 private volatile long[] odo=new long[0];
 public volatile String status=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0653,"기기 설정을 읽고 있어요.");
 public volatile boolean ready,available;
 private volatile boolean connected=true;
 private int page;private long pending,deadline,lastPoll,lastResultPoll;private volatile long revision;private boolean loading=true,namePending;
 public DeviceSettingsSession(CompanionWire wire){this(wire,(a,f,r)->{});}
 public DeviceSettingsSession(CompanionWire wire,Audit audit){this.wire=wire;this.audit=audit;available=wire.fullSettings;}
 public long revision(){return revision;}
 public List<long[]> rows(){List<long[]> copy=new ArrayList<>();for(long[] r:rows)copy.add(r.clone());return copy;}
 public long[] odometer(){return odo.clone();}
 public String text(long field){String value=texts.get(field);return value==null?"":value;}
 public boolean edit(long field,long value,long expected){return connected&&queue.offer(new Edit(0,field,value,expected,null));}
 public boolean name(String name){return connected&&available&&name.getBytes(StandardCharsets.UTF_8).length<=48&&queue.offer(new Edit(1,0x201,0,0,name));}
 public boolean decide(long revision,int choice){return connected&&available&&queue.offer(new Edit(2,0,choice,revision,null));}
 public boolean read(long field){return connected&&available&&queue.offer(new Edit(3,field,0,0,null));}
 public void refresh(){if(connected)queue.offer(new Edit(4,0,0,0,null));}
 private void reload(){building.clear();page=0;loading=true;}
 public void disconnected(){connected=false;queue.clear();ready=false;status=pending!=0?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0654,"저장 결과 미확인. 다시 연결해 현재값을 확인하세요. 자동 재실행하지 않아요."):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0000,"주행 연결 후 기기에서 읽은 설정을 표시해요.");}
 public void tick(long now)throws IOException {
  if(!connected)return;
  try {
   if(pending!=0){
    if(now-lastResultPoll<200)return;lastResultPoll=now;
    byte[] r=wire.send(available?0x9a:0x73,CompanionWire.words(pending));
    if(r.length!=20||CompanionWire.u(r,4)!=pending)throw new IOException("Settings result schema differs");
    if(CompanionWire.u(r,8)!=0){long result=CompanionWire.u(r,12),saved=CompanionWire.u(r,16);
     if(result!=0||saved!=0&&saved!=1&&saved!=8){status=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0655,"설정 적용 또는 저장 실패 · ")+result+" / "+saved;pending=0;reload();return;}
     if(saved==0){pending=0;status=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0656,"기기에 적용·저장했어요.");reload();return;}
     status=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0657,"기기에 적용했어요. 영구 저장을 기다리고 있어요.");
    }
    if(now>=deadline){pending=0;status=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0658,"저장 결과 미확인. 현재값을 다시 조회하세요. 자동 재실행하지 않아요.");reload();}return;
   }
   Edit e=queue.poll();
   if(e!=null){
    if(e.type==4){reload();return;}
    if(e.type==3){readText(e.field);return;}
    audit.record("settings_intent",e.type==2?0x204:e.field,0);
    if(e.type==2){wire.send(0x9b,CompanionWire.words(e.expected,e.value));status=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0659,"ODO 선택을 요청했어요. 저장 상태를 확인하고 있어요.");lastPoll=0;return;}
    byte[] r;
    if(e.type==1){byte[] utf=e.text.getBytes(StandardCharsets.UTF_8),data=new byte[4+utf.length];System.arraycopy(CompanionWire.words(0x201),0,data,0,4);System.arraycopy(utf,0,data,4,utf.length);r=wire.send(0x99,data);}
    else r=wire.send(available?0x99:0x72,available?CompanionWire.words(e.field,e.value,e.expected):CompanionWire.words(e.field,e.value));
    if(r.length!=8||CompanionWire.u(r,4)==0)throw new IOException("Invalid settings request ID");
    pending=CompanionWire.u(r,4);audit.record("settings_accepted",e.field,pending);deadline=now+45000;status=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0660,"요청을 접수했어요. 적용·저장 결과를 기다리고 있어요.");return;
   }
   if(loading){
    byte[] r=wire.client.request(available?0x97:0x71,CompanionWire.words(page));int header=available?16:12;
    if(r.length<header||CompanionWire.u(r,4)!=(available?2:1)||CompanionWire.u(r,8)>8||r.length!=header+24*CompanionWire.u(r,8))throw new IOException("Settings catalog differs");
    int count=(int)CompanionWire.u(r,8);if(available)ready=CompanionWire.u(r,12)!=0;
    for(int i=0;i<count;i++){long[] v=new long[6];for(int j=0;j<6;j++)v[j]=(int)CompanionWire.u(r,header+i*24+j*4);building.add(v);}
    page+=count;if(count<8){rows=Collections.unmodifiableList(new ArrayList<>(building));revision++;loading=false;namePending=available;status=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0661,"기기 설정을 읽었어요.");}
    else if(page>=128)throw new IOException("Settings catalog too large");return;
   }
   if(namePending){namePending=false;readText(0x201);return;}
   if(available&&now-lastPoll>=1000){
    byte[] r=wire.client.request(0x9b,new byte[0]);if(r.length!=40)throw new IOException("ODO schema differs");
    long[] v=new long[9];for(int i=0;i<9;i++)v[i]=CompanionWire.u(r,4+i*4);odo=v;ready=v[6]!=0;lastPoll=now;
   }
  }catch(NdcpClient.DeviceRejected rejected){
   pending=0;status=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0662,"기기가 요청을 거절했어요 · ")+rejected.result+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0663,". 정차 상태와 최신값을 확인하세요.");
  }
 }
 private void readText(long field)throws IOException {
  byte[] r=wire.client.request(0x98,CompanionWire.words(field));if(r.length<8||r.length>104||CompanionWire.u(r,4)!=field)throw new IOException("Settings text differs");
  Map<Long,String> copy=new HashMap<>(texts);copy.put(field,new String(r,8,r.length-8,StandardCharsets.UTF_8));texts=Collections.unmodifiableMap(copy);
 }
}
