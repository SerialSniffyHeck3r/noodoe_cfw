package io.opennoodoe.app.companion;
import android.app.*;import android.content.*;import android.os.*;import android.service.notification.StatusBarNotification;
import java.util.*;import java.util.concurrent.atomic.AtomicLong;
import io.opennoodoe.app.protocol.ndcp.NdcpClient;
import org.junit.*;import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.*;
import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28) @GraphicsMode(GraphicsMode.Mode.NATIVE)
public class NotificationDeliveryTest {
 static class Radio extends VisualTransferTest.Radio {
  final List<Long> published=new ArrayList<>(),revisions=new ArrayList<>();int headers,removes;boolean legacy;
  @Override public void send(byte[] frame){int op=frame[5]&255;byte[] p=Arrays.copyOfRange(frame,16,frame.length-4);
   if(op==0x74&&legacy){published.add(CompanionWire.u(p,4));}
   if(op==0x74&&!legacy){assertTrue("No metadata before visual READY",ready);int tail=11+(p[8]&255)+(p[9]&255)+(p[10]&255);assertEquals(key,CompanionWire.u(p,tail+4));published.add(CompanionWire.u(p,4));revisions.add(CompanionWire.u(p,tail));}
   if(op==0x7d){assertTrue(ready);headers++;}if(op==0x75)removes++;super.send(frame);
  }
 }
 Context context;NotificationHistory history;NotificationDelivery delivery;Radio radio;int battery=87;
 @Before public void setup(){context=RuntimeEnvironment.getApplication();CjkFonts.initialize(context);context.getSharedPreferences("companion",0).edit().putStringSet("notification.apps",Collections.singleton("test.messages")).commit();history=new NotificationHistory();history.activate(context);radio=new Radio();CompanionWire wire=new CompanionWire(new NdcpClient(radio));wire.epoch=42;wire.phonePanels=true;delivery=new NotificationDelivery(context,wire,new AtomicLong(100)::incrementAndGet);}
 void post(int id,String body){Notification n=new Notification.Builder(context,"test").setSmallIcon(android.R.drawable.ic_dialog_email).setContentTitle("臺灣・日本の通知").setContentText(body).build();history.posted(context,new StatusBarNotification("test.messages","test.messages",id,null,0,0,0,n,android.os.Process.myUserHandle(),System.currentTimeMillis()));}
 void refresh()throws Exception{delivery.refresh(history,history.snapshot(context),"Galaxy S24 Ultra",battery,new ReplySettings(context));}
 void finish()throws Exception{VisualTransfer v=delivery.next();assertNotNull(v);radio.ready=true;int pumps=0;while(!v.pump(100+pumps++))assertTrue(pumps<10);delivery.completed(v);}
 void init()throws Exception{refresh();finish();finish();assertEquals(2,radio.headers);}
 @Test public void contentOnlyPublishedAfterReceiverReady()throws Exception{
  init();post(1,"first");refresh();VisualTransfer v=delivery.next();radio.ready=false;
  for(int i=0;i<5;i++){assertFalse(v.pump(200+i));refresh();assertSame(v,delivery.next());assertTrue(delivery.visible().isEmpty());assertTrue(radio.published.isEmpty());}
  radio.ready=true;assertTrue(v.pump(210));delivery.completed(v);assertEquals(1,delivery.visible().size());assertEquals(1,radio.published.size());
 }
 @Test public void perpetualArrivalsCannotStarveCompletedContent()throws Exception{
  init();int message=0;
  for(int frame=0;frame<50;frame++){
   for(int i=0;i<20;i++)post(message++,"message "+message);refresh();VisualTransfer active=delivery.next();
   assertFalse(active.pump(100));for(int i=0;i<20;i++)post(message++,"message "+message);refresh();assertSame(active,delivery.next());
   radio.ready=true;while(!active.pump(200)){}delivery.completed(active);
   assertEquals(Math.min(10,frame+1),delivery.visible().size());assertEquals(frame+1,radio.published.size());assertTrue(history.snapshot(context).size()<=10);
  }
  for(int i=1;i<radio.revisions.size();i++)assertTrue(radio.revisions.get(i)>radio.revisions.get(i-1));
 }
 @Test public void repeatedEditsCoalesceBeforeBeginAndNeverRestartActive()throws Exception{
  init();post(1,"v0");refresh();VisualTransfer first=delivery.next();assertFalse(first.pump(100));
  for(int i=1;i<=100;i++){post(1,"v"+i);refresh();assertSame(first,delivery.next());}
  radio.ready=true;while(!first.pump(200)){}delivery.completed(first);assertEquals("v0",delivery.visible().get(0).body);
  finish();assertEquals("v100",delivery.visible().get(0).body);assertEquals(2,radio.published.size());assertFalse(delivery.pending());
 }
 @Test public void batteryDoesNotRetransmitAllMessages()throws Exception{
  init();for(int i=0;i<10;i++)post(i,"message "+i);refresh();for(int i=0;i<10;i++)finish();
  assertEquals(10,radio.published.size());battery=86;refresh();finish();assertFalse(delivery.pending());assertEquals(10,radio.published.size());assertEquals(3,radio.headers);
 }
 @Test public void reenableCannotPublishOldInFlightMask()throws Exception{
  init();post(1,"old grant");refresh();VisualTransfer old=delivery.next();assertFalse(old.pump(100));history.deactivate();history.activate(context);refresh();
  radio.ready=true;while(!old.pump(200)){}delivery.completed(old);assertTrue(radio.published.isEmpty());assertTrue(delivery.visible().isEmpty());
  post(2,"new grant");refresh();finish();assertEquals("new grant",delivery.visible().get(0).body);
  history.deactivate();refresh();assertTrue(delivery.visible().isEmpty());assertEquals(1,radio.removes);
 }
 @Test public void activeTrayPollingDoesNotRepeatAnyCompletedImage()throws Exception{
  init();for(int i=0;i<25;i++)post(i,"message "+i);refresh();for(int i=0;i<10;i++)finish();int begins=radio.begins;
  StatusBarNotification[] active=history.snapshot(context).stream().map(e->e.source).toArray(StatusBarNotification[]::new);
  for(int i=0;i<100;i++){history.refresh(context,active);refresh();assertFalse(delivery.pending());}
  assertEquals(begins,radio.begins);assertEquals(10,radio.published.size());
 }
 @Test public void oldTextOnlyFirmwareGetsNoVisualCommandsOrReplays()throws Exception{
  radio.legacy=true;CompanionWire wire=new CompanionWire(new NdcpClient(radio));wire.epoch=42;delivery=new NotificationDelivery(context,wire,new AtomicLong(10)::incrementAndGet);
  for(int i=0;i<15;i++)post(i,"legacy "+i);refresh();assertEquals(6,delivery.visible().size());assertEquals(0,radio.begins);assertFalse(delivery.pending());
  int sent=radio.published.size();for(int i=0;i<30;i++)refresh();assertEquals(sent,radio.published.size());
  history.deactivate();refresh();assertEquals(6,radio.removes);assertTrue(delivery.visible().isEmpty());
 }
 @Test public void compactLayoutIsSentOnlyWhenTheDeviceAdvertisesIt()throws Exception{
  radio.ready=true;CompanionWire wire=new CompanionWire(new NdcpClient(radio));wire.epoch=42;
  wire.phonePanels=true;wire.packedPanels=true;wire.largePhonePanels=true;wire.compactPhonePanels=true;
  delivery=new NotificationDelivery(context,wire,new AtomicLong(900)::incrementAndGet);
  init();post(11,"누도에 잘 도착했나요? 臺灣 · 日本 · 한국");refresh();finish();
  assertEquals(144,radio.bytes.toByteArray()[0]&255);assertEquals(5,radio.bytes.toByteArray()[1]&255);
  wire.compactPhonePanels=false;delivery=new NotificationDelivery(context,wire,new AtomicLong(1000)::incrementAndGet);
  refresh();finish();assertEquals(3,radio.bytes.toByteArray()[1]&255);finish();
  post(12,"legacy layout");refresh();finish();
  assertEquals(144,radio.bytes.toByteArray()[0]&255);assertEquals(3,radio.bytes.toByteArray()[1]&255);
 }
}
