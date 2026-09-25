package io.opennoodoe.app.companion;
import android.content.Context;
import java.io.IOException;
import java.util.*;

/** Bounded, completion-first notification delivery. Only one immutable mask is
 * allocated in flight; desired metadata coalesces to the newest ten entries.
 * Finishing a transfer is never postponed by a new notification or battery tick.
 * The UI only receives keys after receiver CRC/decode has reported READY. */
final class NotificationDelivery {
 interface KeySource {long next();}
 private final Context context;private final CompanionWire wire;private final KeySource keys;
 private NotificationHistory history;private List<NotificationHistory.Entry> desired=Collections.emptyList();
 private final LinkedHashMap<Long,NotificationHistory.Entry> shown=new LinkedHashMap<>();
 private String device="",sentDevice="";private int battery=-1,sentBattery=-2;
 private ReplySettings replies;private long headerKey,replyKey,replyRevision,lastRevision;
 private int replyCount;private Job job;private boolean preferMessage;
 private static final class Job {
  final int kind,battery;final String device;final long key;final VisualTransfer transfer;
  final NotificationHistory.Entry entry;final ReplySettings replies;
  Job(int kind,int battery,String device,long key,VisualTransfer transfer,NotificationHistory.Entry entry,ReplySettings replies){this.kind=kind;this.battery=battery;this.device=device;this.key=key;this.transfer=transfer;this.entry=entry;this.replies=replies;}
 }
 NotificationDelivery(Context c,CompanionWire w,KeySource k){context=c;wire=w;keys=k;}
 /** Revoke disabled sources promptly without replaying old tray entries. The
  * delivered history stays readable when newer arrivals overflow the pending
  * ten slots; otherwise a busy stream could erase every completed page. */
 void refresh(NotificationHistory h,List<NotificationHistory.Entry> entries,String name,int percent,ReplySettings settings)throws IOException{
  history=h;desired=new ArrayList<>(entries);device=name;battery=percent;replies=settings;
  for(Iterator<NotificationHistory.Entry> it=shown.values().iterator();it.hasNext();){NotificationHistory.Entry e=it.next();if(!allowed(e)){wire.send(0x75,CompanionWire.words(e.id));it.remove();}}
  if(!wire.phonePanels){NotificationHistory.Entry e;while((e=candidate())!=null)publish(e,0);}
 }
 private boolean allowed(NotificationHistory.Entry e){return history!=null&&history.permitted(context,e);}
 private static boolean after(long a,long b){return b==0||(int)(a-b)>0;}
 /** Send revisions in chronological order so old and new firmware both keep
  * newest-at-front order without a protocol change. A live transfer is frozen,
  * but pending revisions are replaced before any image allocation/BEGIN. */
 private NotificationHistory.Entry candidate(){
  NotificationHistory.Entry found=null;
  for(NotificationHistory.Entry e:desired)if(allowed(e)&&after(e.revision,lastRevision)&&(found==null||after(found.revision,e.revision)))found=e;
  return found;
 }
 private boolean headerPending(){return headerKey==0||battery!=sentBattery||!device.equals(sentDevice);}
 private boolean replyPending(){return replies!=null&&(replyKey==0||replyRevision!=replies.revision);}
 boolean pending(){return wire.phonePanels&&(job!=null||headerPending()||replyPending()||candidate()!=null);}
 VisualTransfer next(){
  if(job!=null)return job.transfer;if(!wire.phonePanels||replies==null)return null;
  NotificationHistory.Entry e=candidate();int kind;
  if(headerKey==0)kind=1;else if(replyKey==0)kind=2;
  else if(e!=null&&(preferMessage||(!headerPending()&&!replyPending())))kind=3;
  else if(headerPending())kind=1;else if(replyPending())kind=2;else return null;
  long key=keys.next();byte[] pixels=wire.largePhonePanels?(kind==2?LargePhonePanel.replies(replies):wire.compactPhonePanels?LargePhonePanel.compactNotification(context,device,battery,kind==3?e:null):LargePhonePanel.notification(context,device,battery,kind==3?e:null)):(kind==2?PhonePanelRenderer.replies(replies):PhonePanelRenderer.notification(context,device,battery,kind==3?e:null,wire.packedPanels));
  job=new Job(kind,battery,device,key,wire.largePhonePanels?new VisualTransfer(wire,8,key,PanelCodec.encodeLarge(pixels,kind==2?4:wire.compactPhonePanels?5:3)):PanelCodec.transfer(wire,key,pixels),kind==3?e:null,replies);
  preferMessage=kind!=3;return job.transfer;
 }
 /** The owner calls this only after pump() verifies READY for this exact key.
  * Completion of revoked/replaced-listener work releases its buffer without
  * publishing. Never replace the active transaction halfway through its bytes. */
 void completed(VisualTransfer transfer)throws IOException{
  if(job==null||job.transfer!=transfer)return;Job done=job;
  if(history!=null)history.selectionChanged(context);
  if(done.kind==3){if(allowed(done.entry))publish(done.entry,done.key);}
  else{
   if(done.kind==1){headerKey=done.key;sentBattery=done.battery;sentDevice=done.device;}
   else{replyKey=done.key;replyRevision=done.replies.revision;replyCount=done.replies.replies.size();}
   wire.send(0x7d,CompanionWire.words(headerKey,replyKey,replyCount,replyRevision));
  }
  job=null;
 }
 private void publish(NotificationHistory.Entry e,long key)throws IOException{
  if(wire.phonePanels)wire.notification(e,key);else wire.notification(e.id,e.app,e.title,e.body);
  lastRevision=e.revision;shown.remove(e.id);shown.put(e.id,e);
  while(shown.size()>(wire.phonePanels?10:6))shown.remove(shown.keySet().iterator().next());
 }
 long reply(long id,long revision,String message,android.service.notification.StatusBarNotification[] active){
  NotificationHistory.Entry e=shown.get(id);
  return history==null||e==null||e.revision!=revision?7:history.replyVisible(context,e,active,message);
 }
 List<NotificationHistory.Entry> visible(){ArrayList<NotificationHistory.Entry> out=new ArrayList<>(shown.values());Collections.reverse(out);return out;}
}
