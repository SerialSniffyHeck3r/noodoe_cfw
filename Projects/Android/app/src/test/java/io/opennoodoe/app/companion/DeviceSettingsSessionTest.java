package io.opennoodoe.app.companion;
import io.opennoodoe.app.protocol.ByteCodec;
import io.opennoodoe.app.protocol.ndcp.NdcpClient;
import io.opennoodoe.app.transport.SppTransport;
import java.io.*;import java.util.*;
import org.junit.Test;import static org.junit.Assert.*;
public class DeviceSettingsSessionTest {
 static final class Radio implements SppTransport {
  final Deque<byte[]> replies=new ArrayDeque<>();final List<Integer> commands=new ArrayList<>();
  boolean legacy,drop,malformed;long saved=1,result=0;byte[] mutation;
  public void send(byte[] packet)throws IOException{
   int op=packet[5]&255;commands.add(op);int id=(int)ByteCodec.u32le(packet,8);byte[] r=CompanionWire.words(0);
   if(op==0x97||op==0x71)r=legacy?CompanionWire.words(0,1,1,0x1008,0,-5,5,1,0):CompanionWire.words(0,2,1,1,0x1008,0,-5,5,1,0);
   if(malformed&&(op==0x97||op==0x71))r=CompanionWire.words(0,2,255,1);
   if(op==0x98)r=CompanionWire.words(0,0x201);
   if(op==0x9b)r=CompanionWire.words(0,1,1000,2000,1,1,1,1,9,0);
   if(op==0x99||op==0x72){mutation=Arrays.copyOfRange(packet,20,packet.length-4);r=CompanionWire.words(0,7);if(drop)return;}
   if(op==0x9a||op==0x73)r=CompanionWire.words(0,7,1,result,saved);
   replies.add(NdcpClient.encode(op,id,r,1));
  }
  public byte[] receive(long timeout)throws IOException{if(replies.isEmpty())throw new IOException("Link lost");return replies.removeFirst();}
  public void close(){}
 }
 private static CompanionWire wire(Radio r){CompanionWire w=new CompanionWire(new NdcpClient(r));w.fullSettings=!r.legacy;w.epoch=42;return w;}
 @Test public void signedCatalogAndDurableResultAreSeparate()throws Exception{
  Radio r=new Radio();List<String> audit=new ArrayList<>();DeviceSettingsSession s=new DeviceSettingsSession(wire(r),(a,f,v)->audit.add(a));
  s.tick(1000);assertEquals(-5,s.rows().get(0)[2]);assertEquals(5,s.rows().get(0)[3]);assertTrue(s.ready);
  assertTrue(s.edit(0x1008,-3,0));s.tick(1200);assertEquals(Arrays.asList("settings_intent","settings_accepted"),audit);
  s.tick(1400);assertTrue(s.status.contains("영구 저장"));r.saved=0;s.tick(1600);assertTrue(s.status.contains("저장했어요"));
  assertEquals(1,Collections.frequency(r.commands,0x99));
 }
 @Test public void lostMutationIsNeverResentAndGenerationCanBeAbandoned()throws Exception{
  Radio r=new Radio();r.drop=true;DeviceSettingsSession s=new DeviceSettingsSession(wire(r));s.edit(0x1008,1,0);
  try{s.tick(1000);fail();}catch(IOException expected){}
  s.disconnected();assertFalse(s.ready);s.tick(2000);assertEquals(1,Collections.frequency(r.commands,0x99));
 }
 @Test public void auditFailurePreventsSendingMutation()throws Exception{
  Radio r=new Radio();DeviceSettingsSession s=new DeviceSettingsSession(wire(r),(a,f,v)->{throw new IOException("Disk full");});s.edit(0x1008,2,0);
  try{s.tick(1000);fail();}catch(IOException expected){}assertTrue(r.commands.isEmpty());
 }
 @Test public void legacyFirmwareAndMalformedCatalog()throws Exception{
  Radio r=new Radio();r.legacy=true;DeviceSettingsSession s=new DeviceSettingsSession(wire(r));s.tick(1000);assertFalse(s.available);assertFalse(s.name("Rider"));
  s.edit(0x1008,2,0);s.tick(1200);r.saved=0;s.tick(1400);assertEquals(Arrays.asList(0x71,0x72,0x73),r.commands);
  r=new Radio();r.malformed=true;s=new DeviceSettingsSession(wire(r));try{s.tick(1000);fail();}catch(IOException expected){}
 }
 @Test public void queueAndNameAreBounded()throws Exception{
  DeviceSettingsSession s=new DeviceSettingsSession(wire(new Radio()));assertFalse(s.name(String.join("",Collections.nCopies(17,"한"))));
  for(int i=0;i<4;i++)assertTrue(s.edit(0x1008,i,0));assertFalse(s.edit(0x1008,5,0));s.disconnected();assertFalse(s.edit(0x1008,1,0));assertFalse(s.name("Old dialog"));assertFalse(s.read(0x4003));
 }
}
