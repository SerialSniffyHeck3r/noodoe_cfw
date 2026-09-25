package io.opennoodoe.app.companion;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.annotation.Config;
import android.app.*;
import android.content.*;
import android.os.*;
import android.service.notification.StatusBarNotification;
import java.util.*;
import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28)
public class NotificationHistoryTest {
 private Context context;private NotificationHistory history;private Intent delivered;
 // This fixture runs only on API 28 (@Config above). Its private test.reply
 // receiver is not packaged in the APK; the API 33 branch retains explicit flags.
 @android.annotation.SuppressLint("UnspecifiedRegisterReceiverFlag")
 @Before public void setup(){context=RuntimeEnvironment.getApplication();history=new NotificationHistory();context.getSharedPreferences("companion",0).edit().putStringSet("notification.apps",new HashSet<>(Arrays.asList("test.messenger"))).commit();history.activate(context);
  BroadcastReceiver receiver=new BroadcastReceiver(){public void onReceive(Context c,Intent i){delivered=i;}};
  if(Build.VERSION.SDK_INT>=33)context.registerReceiver(receiver,new IntentFilter("test.reply"),Context.RECEIVER_NOT_EXPORTED);else context.registerReceiver(receiver,new IntentFilter("test.reply"));}
 private StatusBarNotification message(int id,String body,boolean reply){return messageAt(id,body,reply,System.currentTimeMillis());}
 private StatusBarNotification messageAt(int id,String body,boolean reply,long posted){
  Notification.Builder b=new Notification.Builder(context,"test").setSmallIcon(android.R.drawable.ic_dialog_email).setContentTitle("台灣・日本・한국").setContentText(body);
  if(reply){PendingIntent p=PendingIntent.getBroadcast(context,id,new Intent("test.reply").setPackage(context.getPackageName()),PendingIntent.FLAG_UPDATE_CURRENT);b.addAction(new Notification.Action.Builder(android.R.drawable.ic_menu_send,"Reply",p).addRemoteInput(new RemoteInput.Builder("answer").setAllowFreeFormInput(true).build()).build());}
  return new StatusBarNotification("test.messenger","test.messenger",id,null,android.os.Process.myUid(),0,0,b.build(),android.os.Process.myUserHandle(),posted);
 }
 @Test public void tenNewestAndRemovedRemainReadable(){for(int i=0;i<15;i++)history.posted(context,message(i,"Text "+i,true));List<NotificationHistory.Entry> list=history.snapshot(context);assertEquals(10,list.size());assertEquals("Text 14",list.get(0).body);NotificationHistory.Entry e=list.get(0);history.removed(e.source.getKey());assertEquals(10,history.snapshot(context).size());assertNull(history.snapshot(context).get(0).reply);assertEquals(7,history.reply(context,e.id,e.revision,"Never send"));assertNull(delivered);}
 @Test public void staleRevisionCannotReplyToUpdatedMessage(){history.posted(context,message(1,"old",true));NotificationHistory.Entry e=history.snapshot(context).get(0);history.posted(context,message(1,"new",true));assertEquals(7,history.reply(context,e.id,e.revision,"stale"));assertNull(delivered);}
 @Test public void unsupportedAndDisallowedDoNotSend(){history.posted(context,message(1,"Text",false));NotificationHistory.Entry e=history.snapshot(context).get(0);assertEquals(7,history.reply(context,e.id,e.revision,"x"));context.getSharedPreferences("companion",0).edit().putStringSet("notification.apps",Collections.emptySet()).commit();assertTrue(history.snapshot(context).isEmpty());}
 @Test public void signatureAndActualRemoteInputEnvelope(){ReplySettings.save(context,new String[]{"稍後回覆","","了解しました","",""},ReplySettings.DEFAULT_SIGNATURE);ReplySettings s=new ReplySettings(context);assertEquals(2,s.replies.size());history.posted(context,message(2,"Text",true));NotificationHistory.Entry e=history.snapshot(context).get(0);assertEquals(0,history.reply(context,e.id,e.revision,s.message(0)));Shadows.shadowOf(Looper.getMainLooper()).idle();assertNotNull(delivered);assertEquals("稍後回覆\n\nSent from my Noodoe dashboard.",RemoteInput.getResultsFromIntent(delivered).getString("answer"));assertEquals("handed-to-app",context.getSharedPreferences("reply-intent",0).getString("result",""));}
 @Test public void blankSignatureAndFiveReplyBound(){ReplySettings.save(context,new String[]{"1","2","3","4","5"},"");ReplySettings s=new ReplySettings(context);assertEquals(5,s.replies.size());assertEquals("5",s.message(4));try{s.message(5);fail();}catch(IllegalArgumentException expected){}try{ReplySettings.save(context,new String[6],"");fail();}catch(IllegalArgumentException expected){}}
 @Test public void liveRemovalAndActionChangeInvalidateReply(){history.posted(context,message(1,"same",true));NotificationHistory.Entry e=history.snapshot(context).get(0);history.posted(context,message(1,"same",false));assertNotEquals(e.revision,history.snapshot(context).get(0).revision);assertEquals(7,history.reply(context,e.id,e.revision,"x"));history.posted(context,message(2,"active",true));e=history.snapshot(context).get(0);assertFalse(history.refresh(context,null));assertTrue(history.refresh(context,new StatusBarNotification[0]));assertEquals(7,history.reply(context,e.id,e.revision,"x"));assertNull(delivered);}
 @Test public void contentMemoryIsBounded(){char[] huge=new char[100000];Arrays.fill(huge,'漢');history.posted(context,message(1,new String(huge),true));assertEquals(1024,history.snapshot(context).get(0).body.length());}
 @Test public void pollingCannotReplayEvictedNotifications(){
  StatusBarNotification[] active=new StatusBarNotification[25];
  for(int i=0;i<active.length;i++){active[i]=message(i,"message "+i,false);history.posted(context,active[i]);}
  List<NotificationHistory.Entry> before=history.snapshot(context);
  for(int n=0;n<20;n++)history.refresh(context,active);
  List<NotificationHistory.Entry> after=history.snapshot(context);
  assertEquals(before.size(),after.size());
  for(int i=0;i<before.size();i++){assertEquals(before.get(i).id,after.get(i).id);assertEquals(before.get(i).revision,after.get(i).revision);}
 }
 @Test public void liveOnlyActivationAndReenable(){
  StatusBarNotification old=messageAt(1,"before",false,System.currentTimeMillis()-1000);
  SystemClock.sleep(10);history.activate(context);
  history.refresh(context,new StatusBarNotification[]{old});history.posted(context,old);
  assertTrue(history.snapshot(context).isEmpty());
  history.posted(context,message(2,"fresh",false));NotificationHistory.Entry fresh=history.snapshot(context).get(0);
  context.getSharedPreferences("companion",0).edit().putStringSet("notification.apps",Collections.emptySet()).commit();history.selectionChanged(context);
  assertFalse(history.permitted(context,fresh));assertTrue(history.snapshot(context).isEmpty());
  SystemClock.sleep(10);context.getSharedPreferences("companion",0).edit().putStringSet("notification.apps",Collections.singleton("test.messenger")).commit();history.selectionChanged(context);
  history.posted(context,messageAt(2,"fresh",false,System.currentTimeMillis()-1000));assertTrue(history.snapshot(context).isEmpty());
  history.posted(context,message(2,"after enable",false));assertEquals(1,history.snapshot(context).size());assertFalse(history.permitted(context,fresh));
 }
 @Test public void identicalRepostsAndRemovalsAreIdempotent(){
  history.posted(context,message(1,"same",false));NotificationHistory.Entry e=history.snapshot(context).get(0);history.markRead(e.id,e.revision);
  for(int i=0;i<100;i++){history.posted(context,messageAt(1,"same",false,System.currentTimeMillis()+i));}
  assertSame(e,history.snapshot(context).get(0));assertTrue(e.read);
  history.removed(e.source.getKey());long revision=history.snapshot(context).get(0).revision;
  for(int i=0;i<100;i++)history.removed(e.source.getKey());assertEquals(revision,history.snapshot(context).get(0).revision);
 }
 @Test public void disconnectedListenerCannotAdmitOrPublish(){history.posted(context,message(1,"live",false));NotificationHistory.Entry e=history.snapshot(context).get(0);history.deactivate();history.posted(context,message(2,"late callback",false));assertTrue(history.snapshot(context).isEmpty());assertFalse(history.permitted(context,e));history.activate(context);assertFalse(history.permitted(context,e));}
 @Test public void wearableReplyWithoutStandardActionIsUsable(){
  StatusBarNotification n=message(40,"wearable",false);
  PendingIntent pending=PendingIntent.getBroadcast(context,40,new Intent("test.reply").setPackage(context.getPackageName()),PendingIntent.FLAG_UPDATE_CURRENT);
  Notification.Action action=new Notification.Action.Builder(android.R.drawable.ic_menu_send,"Respond",pending).addRemoteInput(new RemoteInput.Builder("wear.reply").build()).build();
  Notification.Builder builder=Notification.Builder.recoverBuilder(context,n.getNotification());builder.extend(new Notification.WearableExtender().addAction(action));
  n=new StatusBarNotification("test.messenger","test.messenger",40,null,0,0,0,builder.build(),android.os.Process.myUserHandle(),System.currentTimeMillis());
  history.posted(context,n);NotificationHistory.Entry e=history.snapshot(context).get(0);assertNotNull(e.reply);
  assertEquals(0,history.reply(context,e.id,e.revision,"wear message"));Shadows.shadowOf(Looper.getMainLooper()).idle();assertEquals("wear message",RemoteInput.getResultsFromIntent(delivered).getString("wear.reply"));
 }
 @Test public void visibleCardSurvivesPendingEvictionButRequiresExactLiveSource(){
  StatusBarNotification source=message(55,"displayed",true);history.posted(context,source);NotificationHistory.Entry e=history.snapshot(context).get(0);
  for(int i=0;i<25;i++)history.posted(context,message(100+i,"new"+i,true));
  assertEquals(10,history.snapshot(context).size());assertEquals(0,history.replyVisible(context,e,new StatusBarNotification[]{source},"still displayed"));
  assertEquals(7,history.replyVisible(context,e,new StatusBarNotification[0],"removed"));
  assertEquals(7,history.replyVisible(context,e,new StatusBarNotification[]{message(55,"changed",true)},"changed"));
  history.deactivate();history.activate(context);assertEquals(7,history.replyVisible(context,e,new StatusBarNotification[]{source},"old grant"));
 }
 @Test public void summariesAndOngoingServiceUpdatesAreNotMessages(){for(int flag:new int[]{Notification.FLAG_GROUP_SUMMARY,Notification.FLAG_FOREGROUND_SERVICE,Notification.FLAG_ONGOING_EVENT}){StatusBarNotification n=message(flag,"service",false);n.getNotification().flags|=flag;history.posted(context,n);}assertTrue(history.snapshot(context).isEmpty());}
}
