package io.opennoodoe.app.companion;
import org.junit.Test;
import static org.junit.Assert.*;
import java.io.*;
import java.util.*;
import java.util.zip.CRC32;
import io.opennoodoe.app.protocol.ByteCodec;
import io.opennoodoe.app.protocol.ndcp.NdcpClient;
import io.opennoodoe.app.transport.SppTransport;
public class VisualTransferTest {
 static class Radio implements SppTransport {
  Deque<byte[]> replies=new ArrayDeque<>();ByteArrayOutputStream bytes=new ByteArrayOutputStream();
  long key,length,crc,offset;int finishes,begins;boolean wrongOffset,wrongKey,failed,ready,busy,shortResult;long session=100;
  public void send(byte[] frame){int op=frame[5]&255,id=(int)ByteCodec.u32le(frame,8);byte[] p=Arrays.copyOfRange(frame,16,frame.length-4),r=CompanionWire.words(0);
   if(op==0x0d)r=CompanionWire.words(0,2,2,507);
   else if(op==0x70)r=CompanionWire.words(0,2,42,1,1,1,0,2,session);
   else if(op==0x79){begins++;if(busy){busy=false;r=CompanionWire.words(8);}else{key=CompanionWire.u(p,8);length=CompanionWire.u(p,12);crc=CompanionWire.u(p,16);offset=0;bytes.reset();}}
   else if(op==0x7a){assertEquals(42,CompanionWire.u(p,0));assertEquals(key,CompanionWire.u(p,4));assertEquals(offset,CompanionWire.u(p,8));int n=p.length-12;assertTrue(n>0&&n<=960);bytes.write(p,12,n);offset+=n;r=CompanionWire.words(0,wrongOffset?offset+1:offset);}
   else if(op==0x7b){finishes++;assertEquals(length,offset);CRC32 c=new CRC32();c.update(bytes.toByteArray());assertEquals(crc,c.getValue());}
   else if(op==0x7c)r=CompanionWire.words(0,1,wrongKey?key+1:key,failed?4:ready?3:2,failed?4:0,shortResult?offset-1:offset);
   replies.add(NdcpClient.encode(op,id,r,CompanionWire.u(r,0)==0?1:3));
  }
  public byte[] receive(long timeout)throws IOException{if(replies.isEmpty())throw new IOException("Empty");return replies.removeFirst();}
  public void close(){}
 }
 private CompanionWire connect(Radio r)throws Exception{CompanionWire w=new CompanionWire(new NdcpClient(r));w.connect();return w;}
 @Test public void artworkCannotBeRestartedByRepeatedTilePrefetch()throws Exception{
  Radio r=new Radio();r.ready=true;byte[] jpeg=new byte[49152];new Random(8).nextBytes(jpeg);
  VisualTransfer t=new VisualTransfer(connect(r),2,23,jpeg);assertTrue(t.pauseForTile());int pumps=0;
  while(!t.pump(100+(pumps++)*50)){assertFalse(t.pauseForTile());assertTrue(pumps<10);}
  assertEquals(1,r.begins);assertEquals(1,r.finishes);assertArrayEquals(jpeg,r.bytes.toByteArray());
 }
 @Test public void interruptedPanelRestartsWithoutLosingBytes()throws Exception{Radio r=new Radio();CompanionWire w=connect(r);byte[] panel=new byte[18432];new Random(2).nextBytes(panel);VisualTransfer p=new VisualTransfer(w,6,4,panel);assertFalse(p.pump(100));assertFalse(p.pauseForMusic());r.ready=true;while(!p.pump(300)){}assertArrayEquals(panel,r.bytes.toByteArray());assertFalse(p.pauseForMusic());}
 @Test public void readyWithShortLengthIsNotSuccess()throws Exception{Radio r=new Radio();r.shortResult=true;r.ready=true;VisualTransfer t=new VisualTransfer(connect(r),3,5,new byte[100]);try{t.pump(100);fail();}catch(IOException expected){}}
 @Test public void alphaNibbleOrderAndQuantization(){assertArrayEquals(new byte[]{(byte)0xf0,(byte)0x87},Alpha4.pack(new int[]{0xffffffff,0x00ffffff,0x88ffffff,0x77ffffff}));}
 @Test public void rideSessionChangesWithoutChangingConnection()throws Exception{Radio r=new Radio();CompanionWire w=connect(r);assertTrue(w.visuals);assertEquals(100,w.rideSession);r.session=200;w.poll();assertEquals(200,w.rideSession);assertEquals(42,w.epoch);}
 @Test public void exactBytesAndNoSuccessBeforeDurableResult()throws Exception{Radio r=new Radio();byte[] data=new byte[10944];new Random(3).nextBytes(data);VisualTransfer t=new VisualTransfer(connect(r),1,77,data);assertFalse(t.pump(100));assertEquals(7680,t.received());assertFalse(t.pump(200));assertEquals(1,r.finishes);assertFalse(t.pump(300));r.ready=true;assertTrue(t.pump(400));assertEquals(1,r.finishes);assertArrayEquals(data,r.bytes.toByteArray());}
 @Test public void explicitBusyCanRetryBeforeBegin()throws Exception{Radio r=new Radio();r.busy=true;VisualTransfer t=new VisualTransfer(connect(r),2,5,new byte[100]);assertFalse(t.pump(100));r.ready=true;assertTrue(t.pump(200));assertEquals(2,r.begins);}
 @Test public void mismatchedOffsetStopsBeforeCommit()throws Exception{Radio r=new Radio();r.wrongOffset=true;VisualTransfer t=new VisualTransfer(connect(r),2,5,new byte[100]);try{t.pump(100);fail();}catch(IOException expected){}assertEquals(0,r.finishes);}
 @Test public void resultMustBelongToThisImage()throws Exception{Radio r=new Radio();r.wrongKey=true;r.ready=true;VisualTransfer t=new VisualTransfer(connect(r),3,5,new byte[100]);try{t.pump(100);fail();}catch(IOException expected){}}
 @Test public void physicalVerificationErrorIsNotSuccess()throws Exception{Radio r=new Radio();r.failed=true;VisualTransfer t=new VisualTransfer(connect(r),4,5,new byte[100]);try{t.pump(100);fail();}catch(IOException expected){}}
 @Test public void boundedWaitDoesNotResendFinish()throws Exception{Radio r=new Radio();VisualTransfer t=new VisualTransfer(connect(r),5,5,new byte[100]);assertFalse(t.pump(100));try{t.pump(120101);fail();}catch(IOException expected){}assertEquals(1,r.finishes);}
}
