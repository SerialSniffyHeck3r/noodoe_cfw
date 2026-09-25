package io.opennoodoe.app.companion;
import android.app.*;
import android.content.*;
import android.os.*;
import android.service.notification.StatusBarNotification;
import java.util.*;
/** Ten recent messages in RAM. Removed notifications remain readable but cannot
 * receive replies. IDs are allocated, not truncated hashes of notification keys. */
public final class NotificationHistory {
 public static final class Entry {
  public final long id,revision;long admission;public final StatusBarNotification source;public final String app,title,body;
  public boolean read;public long received=SystemClock.elapsedRealtime();public final boolean active;public final Notification.Action reply;
  Entry(long id,long revision,StatusBarNotification source,String app,String title,String body,boolean active){this.id=id;this.revision=revision;this.source=source;this.received=SystemClock.elapsedRealtime()-Math.min(86400000L,Math.max(0L,System.currentTimeMillis()-source.getPostTime()));this.app=app;this.title=title;this.body=body;this.active=active;this.reply=active?replyAction(source.getNotification()):null;}
 }
 private static final java.util.concurrent.atomic.AtomicLong sequence=new java.util.concurrent.atomic.AtomicLong();
 private static final class Grant {final long token=next(),since=System.currentTimeMillis();}
 private final LinkedHashMap<String,Entry> entries=new LinkedHashMap<>();
 private final Map<String,Grant> grants=new HashMap<>();private boolean enabled;
 /** A listener activation starts a new live-only history. Never import the
  * Android tray: its old entries are not new messages, even after regranting. */
 public synchronized void activate(Context c){entries.clear();grants.clear();enabled=true;selectionChanged(c);}
 public synchronized void deactivate(){enabled=false;entries.clear();grants.clear();}
 /** Called on preference changes, before subsequent listener callbacks. Each
  * newly enabled app receives its own cutoff/token; stale transfers cannot
  * become visible after disable/re-enable. Existing enabled apps keep history. */
 public synchronized void selectionChanged(Context c){
  if(!enabled)return;
  Set<String> allowed=new HashSet<>(c.getSharedPreferences("companion",0).getStringSet("notification.apps",Collections.emptySet()));
  allowed.add(c.getPackageName()); // Only the explicit test notification passes below.
  grants.keySet().retainAll(allowed);
  for(String app:allowed)if(!grants.containsKey(app))grants.put(app,new Grant());
  for(Iterator<Entry> it=entries.values().iterator();it.hasNext();)if(!permitted(c,it.next()))it.remove();
 }
 public synchronized boolean permitted(Context c,Entry e){
  Grant g=grants.get(e.source.getPackageName());
  return enabled&&g!=null&&g.token==e.admission;
 }
 public synchronized void posted(Context c,StatusBarNotification n){
  if(!enabled||n==null)return;selectionChanged(c);
  Grant g=grants.get(n.getPackageName());Notification notification=n.getNotification();
  if(g==null||n.getPostTime()<g.since||notification==null)return;
  if(c.getPackageName().equals(n.getPackageName())&&!NotificationTest.matches(c,n))return;
  if((notification.flags&(Notification.FLAG_GROUP_SUMMARY|Notification.FLAG_FOREGROUND_SERVICE|Notification.FLAG_ONGOING_EVENT))!=0)return;
  Bundle b=notification.extras;if(b==null)return;
  String title=bounded(b.getCharSequence(Notification.EXTRA_TITLE,""),256);
  String body=bounded(b.getCharSequence(Notification.EXTRA_BIG_TEXT,b.getCharSequence(Notification.EXTRA_TEXT,"")),1024);
  if(title.isEmpty()&&body.isEmpty())return;
  String app=n.getPackageName();try{app=c.getPackageManager().getApplicationLabel(c.getPackageManager().getApplicationInfo(app,0)).toString();}catch(Exception ignored){}
  Entry old=entries.get(n.getKey());Notification.Action action=replyAction(notification);
  // Reposting identical content with a new Android postTime is not a new
  // message. Keep read state/identity; changed reply authorization is versioned.
  if(old!=null&&old.active&&old.title.equals(title)&&old.body.equals(body)&&sameAction(old.reply,action))return;
  long id=old==null?next():old.id,revision=next();entries.remove(n.getKey());
  Entry value=new Entry(id,revision,n,app,title,body,true);value.admission=g.token;
  if(old!=null&&old.title.equals(title)&&old.body.equals(body)){value.read=old.read;value.received=old.received;}
  entries.put(n.getKey(),value);while(entries.size()>10)entries.remove(entries.keySet().iterator().next());
 }
 private static String bounded(CharSequence s,int limit){if(s==null)return "";int end=Math.min(s.length(),limit);if(end>0&&Character.isHighSurrogate(s.charAt(end-1)))end--;return s.subSequence(0,end).toString();}
 private static boolean sameAction(Notification.Action a,Notification.Action b){
  if(a==null||b==null)return a==b;
  if(!a.actionIntent.equals(b.actionIntent))return false;
  RemoteInput[] x=a.getRemoteInputs(),y=b.getRemoteInputs();if(x.length!=y.length)return false;
  for(int i=0;i<x.length;i++)if(!x[i].getResultKey().equals(y[i].getResultKey())||x[i].getAllowFreeFormInput()!=y[i].getAllowFreeFormInput())return false;
  return true;
 }
 /** Only revoke replies for disappeared entries. Polling NEVER admits tray
  * contents: cycling25 active entries through a10-entry cache used to create
  * fresh IDs forever and continuously invalidate every pending visual. */
 public synchronized boolean refresh(Context c,StatusBarNotification[] active){
  selectionChanged(c);if(active==null)return false;Set<String> keys=new HashSet<>();
  for(StatusBarNotification n:active)keys.add(n.getKey());
  for(String key:new ArrayList<>(entries.keySet()))if(entries.get(key).active&&!keys.contains(key))removed(key);
  return true;
 }
 private static long next(){long value;do{value=sequence.incrementAndGet()&0xffffffffL;}while(value==0);return value;}
 public synchronized void removed(String key){Entry e=entries.get(key);if(e!=null&&e.active){Entry v=new Entry(e.id,next(),e.source,e.app,e.title,e.body,false);v.admission=e.admission;v.received=e.received;v.read=e.read;entries.put(key,v);}}
 public synchronized void markRead(long id,long revision){for(Entry e:entries.values())if(e.id==id&&e.revision==revision)e.read=true;}
 public synchronized List<Entry> snapshot(Context c){selectionChanged(c);ArrayList<Entry> out=new ArrayList<>(entries.values());Collections.reverse(out);return out;}
 /** Standard and wearable reply actions are independent Android surfaces.
  * Prefer an explicit semantic REPLY, then a free-form input; never guess an
  * action by translated button text, package name, or a content Intent. */
 private static Notification.Action replyAction(Notification n){
  if(n==null)return null;
  ArrayList<Notification.Action> actions=new ArrayList<>();
  if(n.actions!=null)Collections.addAll(actions,n.actions);
  try{actions.addAll(new Notification.WearableExtender(n).getActions());}catch(RuntimeException malformed){/* A malformed extension must not hide a valid standard action. */}
  Notification.Action fallback=null;
  for(Notification.Action a:actions)if(a!=null&&a.actionIntent!=null&&a.getRemoteInputs()!=null)
   for(RemoteInput input:a.getRemoteInputs())if(input.getAllowFreeFormInput()){
    if(Build.VERSION.SDK_INT>=28&&a.getSemanticAction()==Notification.Action.SEMANTIC_ACTION_REPLY)return a;
    if(fallback==null)fallback=a;break;
   }
  return fallback;
 }
 /** Called once for a validated device selection. Android accepting PendingIntent
  * is not proof the messaging service delivered the reply to its recipient. */
 public synchronized long replyVisible(Context c,Entry shown,StatusBarNotification[] active,String message){
  selectionChanged(c);if(shown==null||!permitted(c,shown)||!shown.active||shown.reply==null||active==null)return 7;
  for(StatusBarNotification live:active)if(live.getKey().equals(shown.source.getKey())){
   // Updating an existing notification may target a different conversation.
   // Require the same action and the same displayed content, not just its key.
   Bundle b=live.getNotification().extras;
   if(b==null||!sameAction(shown.reply,replyAction(live.getNotification()))||
      !shown.title.equals(bounded(b.getCharSequence(Notification.EXTRA_TITLE,""),256))||
      !shown.body.equals(bounded(b.getCharSequence(Notification.EXTRA_BIG_TEXT,b.getCharSequence(Notification.EXTRA_TEXT,"")),1024)))return 7;
   return sendReply(c,shown,message);
  }return 7;
 }
 public synchronized long reply(Context c,long id,long revision,String message){
  for(Entry e:snapshot(c))if(e.id==id){
   if(e.revision!=revision||!e.active||e.reply==null)return 7;
   return sendReply(c,e,message);
  }return 7;
 }
 private long sendReply(Context c,Entry e,String message){
  long id=e.id,revision=e.revision;
   try{Bundle result=new Bundle();boolean found=false;for(RemoteInput input:e.reply.getRemoteInputs())if(input.getAllowFreeFormInput()){result.putCharSequence(input.getResultKey(),message);found=true;}
    if(!found)return 7;Intent fill=new Intent();RemoteInput.addResultsToIntent(e.reply.getRemoteInputs(),fill,result);
    if(Build.VERSION.SDK_INT>=28)RemoteInput.setResultsSource(fill,RemoteInput.SOURCE_CHOICE);
    // Persist only intent identity before the irreversible send, never message text.
    if(!c.getSharedPreferences("reply-intent",0).edit().putLong("id",id).putLong("revision",revision).putLong("time",System.currentTimeMillis()).putString("result","unknown").commit())return 4;
    e.reply.actionIntent.send(c,0,fill);c.getSharedPreferences("reply-intent",0).edit().putString("result","handed-to-app").commit();return 0;
   }catch(Exception failure){return 7;}
 }
}
