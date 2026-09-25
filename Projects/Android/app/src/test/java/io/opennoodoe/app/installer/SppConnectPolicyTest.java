package io.opennoodoe.app.installer;
import org.junit.Test;
import static org.junit.Assert.*;
import java.io.*;

public class SppConnectPolicyTest {
 @Test public void reconnectsFreshBeforeAnyProtocolAndStopsOnSuccess()throws Exception{
  int[] opened={0},refresh={0};
  String socket=SppConnectPolicy.connect(new SppConnectPolicy.Attempt<String>(){
   public String open(int attempt)throws IOException{opened[0]++;if(attempt<3)throw new IOException("socket not ready");return "secure socket";}
   public void prepareRetry(int attempt){refresh[0]++;}
  });assertEquals("secure socket",socket);assertEquals(3,opened[0]);assertEquals(2,refresh[0]);
 }
 @Test public void successDoesNotCreateExtraConnections()throws Exception{
  assertEquals("ready",SppConnectPolicy.connect(new SppConnectPolicy.Attempt<String>(){
   public String open(int a){assertEquals(1,a);return "ready";}
   public void prepareRetry(int a){fail("unnecessary reconnect");}
  }));
 }
 @Test public void failuresBoundedAndCausesRetained()throws Exception{
  int[] opened={0};
  try{SppConnectPolicy.connect(new SppConnectPolicy.Attempt<String>(){
   public String open(int a)throws IOException{opened[0]++;throw new IOException("native connect");}
   public void prepareRetry(int a){}
  });fail();}catch(IOException e){assertTrue(e.getMessage().contains("페어링은 삭제하지"));assertNotNull(e.getCause());}
  assertEquals(3,opened[0]);
 }
}
