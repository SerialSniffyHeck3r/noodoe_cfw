package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;import io.opennoodoe.app.protocol.ndcp.NdcpClient;
import org.junit.Test;import static org.junit.Assert.*;import java.io.*;
public class DeviceProbeTest {
 static final class Radio implements InstallerTransport {
  final int role;byte[] pending;boolean closed;int commands;
  Radio(int role){this.role=role;}
  public void send(byte[] frame)throws IOException{commands++;if(role<0)throw new IOException("offline");assertEquals(0x58,frame[5]&255);byte[] id=new byte[88];ByteCodec.putU32le(id,4,1);ByteCodec.putU32le(id,8,role);pending=NdcpClient.encode(0x58,(int)ByteCodec.u32le(frame,8),id,1);}
  public byte[] receive(long timeout){return pending;}public void close(){closed=true;}
 }
 @Test public void productAndBootstrapDiscoveryUseOnlyIdentity()throws Exception{for(int r:new int[]{1,2}){Radio radio=new Radio(r);String[] role={"unknown"};DeviceProbe.run(()->radio,new StockUpdateSession.Progress(){public void update(String text){}public void role(String v){role[0]=v;}});assertEquals(r==1?"bootstrap":"product",role[0]);assertEquals(1,radio.commands);assertTrue(radio.closed);}}
 @Test public void malformedNdcpNeverFallsBackToStock()throws Exception{int[] opens={0};try{DeviceProbe.run(()->{opens[0]++;return new Radio(7);},s->{});fail();}catch(IOException expected){}assertEquals(1,opens[0]);}
 @Test public void noResponseDoesNotMeanStock()throws Exception{int[] opens={0};String[] role={"unknown"};try{DeviceProbe.run(()->{opens[0]++;return new Radio(-1);},new StockUpdateSession.Progress(){public void update(String text){}public void role(String v){role[0]=v;}});fail();}catch(IOException expected){}assertEquals(2,opens[0]);assertEquals("unknown",role[0]);}
 @Test public void corruptOrShortIdentityRejected()throws Exception{for(byte[] b:new byte[][]{new byte[12],new byte[88]})try{DeviceProbe.role(b);fail();}catch(IOException expected){}}
}
