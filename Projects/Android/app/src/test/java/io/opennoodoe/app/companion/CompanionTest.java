package io.opennoodoe.app.companion;
import org.junit.Test;
import static org.junit.Assert.*;
import io.opennoodoe.app.protocol.ByteCodec;
import io.opennoodoe.app.protocol.ndcp.NdcpClient;
import io.opennoodoe.app.transport.SppTransport;
import io.opennoodoe.app.installer.InstallJournal;
import java.io.*;
import java.nio.file.*;
import java.util.*;
public class CompanionTest {
 static class Radio implements SppTransport {
  Deque<byte[]> q=new ArrayDeque<>();long epoch=42;boolean events;int acks,eventOp=0x10;
  public void send(byte[] b){int op=b[5]&255,id=(int)ByteCodec.u32le(b,8);if((ByteCodec.u16le(b,6)&1)!=0){acks++;return;}
   byte[] reply=CompanionWire.words(0);
   if(op==0x0d)reply=CompanionWire.words(0,2,2,251);
   if(op==0x70)reply=CompanionWire.words(0,1,epoch,1,1,1,80,2);
   if(op==0x73)reply=CompanionWire.words(0,7,1,0,0);
   if(events){for(long event:new long[]{0x80000002L,0x80000002L,0x80000001L})q.add(NdcpClient.encode(eventOp,(int)event,eventOp==0x11||eventOp==0x12?CompanionWire.words(42,10,20,30,4):CompanionWire.words(0),0));}
   q.add(NdcpClient.encode(op,id,reply,1));
  }
  public byte[] receive(long timeout)throws IOException{if(q.isEmpty())throw new IOException("No reply");return q.removeFirst();}
  public void close(){}
 }
 @Test public void liveStatusAndStaleGeneration()throws Exception{Radio r=new Radio();CompanionWire w=new CompanionWire(new NdcpClient(r));w.connect();assertTrue(w.ign);assertEquals(80,w.speed);assertEquals(42,w.epoch);r.epoch=43;try{w.poll();fail();}catch(IOException expected){}}
 @Test public void connectedBeforeUiReadyWaitsOnSameSocket()throws Exception{
  int[] clear={0},closed={0};Radio r=new Radio(){
   @Override public void send(byte[] b){if((b[5]&255)==0x75&&++clear[0]<=2){q.add(NdcpClient.encode(0x75,(int)ByteCodec.u32le(b,8),CompanionWire.words(8),3));}else super.send(b);}
   @Override public void close(){closed[0]++;}
  };
  CompanionWire w=new CompanionWire(new NdcpClient(r));w.initialize();
  assertEquals(3,clear[0]);assertEquals(0,closed[0]);assertEquals(42,w.epoch);
 }
 @Test public void readinessDoesNotReplayAnUncertainWrite()throws Exception{
  int[] clear={0};Radio r=new Radio(){@Override public void send(byte[] b){if((b[5]&255)==0x75)clear[0]++;else super.send(b);}};
  try{new CompanionWire(new NdcpClient(r)).initialize();fail();}catch(IOException expected){}
  assertEquals(1,clear[0]);
 }
 @Test public void mediaDuplicateAndOldEventDoNotToggleTwice()throws Exception{Radio r=new Radio();r.events=true;NdcpClient c=new NdcpClient(r);int[] called={0};c.setRequestHandler((op,p)->{called[0]++;return 0;});c.request(0,new byte[0]);assertEquals(1,called[0]);assertEquals(2,r.acks);}
 @Test public void callDuplicateIsAcknowledgedWithoutDialingTwice()throws Exception{Radio r=new Radio();r.events=true;r.eventOp=0x12;NdcpClient c=new NdcpClient(r);int[] called={0};c.setRequestHandler((op,p)->{called[0]++;assertEquals(0x12,op);assertEquals(20,p.length);return 7;});c.request(0,new byte[0]);c.request(0,new byte[0]);assertEquals(1,called[0]);assertEquals(4,r.acks);}
 @Test public void replyDuplicateDoesNotSendTwiceAndSharesMusicSequence()throws Exception{Radio r=new Radio();r.events=true;r.eventOp=0x11;NdcpClient c=new NdcpClient(r);int[] called={0};c.setRequestHandler((op,p)->{called[0]++;assertEquals(0x11,op);assertEquals(20,p.length);assertEquals(4,CompanionWire.u(p,16));return 7;});c.request(0,new byte[0]);assertEquals(1,called[0]);assertEquals(2,r.acks);r.eventOp=0x10;c.request(0,new byte[0]);assertEquals(1,called[0]);}
 @Test public void utf8ClippingDoesNotSplitCodepoints(){assertEquals("가",new String(CompanionWire.text("가나다",4),java.nio.charset.StandardCharsets.UTF_8));assertEquals("a b",new String(CompanionWire.text("a\nb",20),java.nio.charset.StandardCharsets.UTF_8));}
 @Test public void journalCorruptionNeverSilentlyBecomesReady()throws Exception{File f=Files.createTempDirectory("journal-crc").resolve("intent").toFile();InstallJournal j=new InstallJournal(f);j.save("PRODUCT_COMMIT_RESULT_UNKNOWN");assertEquals(j.state(),new InstallJournal(f).state());String text=new String(Files.readAllBytes(f.toPath()),java.nio.charset.StandardCharsets.ISO_8859_1);Files.write(f.toPath(),text.replace("PRODUCT_COMMIT_RESULT_UNKNOWN","CFW_CONFIRMED").getBytes(java.nio.charset.StandardCharsets.ISO_8859_1));try{new InstallJournal(f);fail();}catch(IOException expected){}}
}
