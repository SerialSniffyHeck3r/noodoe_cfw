package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.*;
import java.io.*;
import java.util.*;
import org.junit.Test;
import static org.junit.Assert.*;

public class StockStreamTest {
 static class Radio implements InstallerTransport {
  final ArrayDeque<byte[]> queue=new ArrayDeque<>();final int split;int requests;
  Radio(int split){this.split=split;}
  public void send(byte[] bytes){
   if(bytes.length==5){
    byte[] raw=new byte[18];raw[0]=(byte)0x85;ByteCodec.putU32le(raw,1,13);ByteCodec.putU16le(raw,6,5);raw[10]=0x10;
    byte[] event=new SequenceFrame(0,128,127,0,new CommandFrame(2,CommandFrame.REPLY,new byte[]{0,0}).encode()).encode();
    byte[] all=Arrays.copyOf(raw,raw.length+event.length);System.arraycopy(event,0,all,raw.length,event.length);
    queue.add(Arrays.copyOfRange(all,0,split));queue.add(Arrays.copyOfRange(all,split,all.length));
   }else if(SequenceFrame.decode(bytes).getPayload().length>0){
    requests++;byte[] info=new byte[82];ByteCodec.putU16le(info,2,5);ByteCodec.putU16le(info,4,16);info[6]=0x10;
    queue.add(new SequenceFrame(0,129,0,0,new CommandFrame(5,CommandFrame.REPLY,info).encode()).encode());
   }
  }
  public byte[] receive(long t)throws IOException {if(queue.isEmpty())throw new IOException("No reply");return queue.remove();}public void close(){}
 }
 @Test public void rawHeaderAndFollowingEventMayShareArbitraryReadBoundaries()throws Exception{
  for(int split:new int[]{1,4,5,12,17,19,23}){
   Radio r=new Radio(split);DeviceInfo info=new StockUpdateSession(r).identify();assertEquals(5,info.firmwareMajor);assertEquals(16,info.firmwareMinor);assertEquals(1,r.requests);
  }
 }
}
