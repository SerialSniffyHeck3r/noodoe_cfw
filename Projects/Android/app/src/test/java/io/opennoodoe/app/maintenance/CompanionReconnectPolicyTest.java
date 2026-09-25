package io.opennoodoe.app.maintenance;
import java.io.*;
import org.junit.Test;
import static org.junit.Assert.*;
import io.opennoodoe.app.protocol.ndcp.NdcpClient;
public class CompanionReconnectPolicyTest {
 @Test public void localLogExceptionCannotTriggerRadioRetry(){
  CompanionReconnectPolicy p=new CompanionReconnectPolicy();
  assertFalse(p.failed(new IllegalArgumentException("Private/unrecognized log field")));
  assertFalse(p.failed(new NdcpClient.DeviceRejected(0x75,8)));
  assertFalse(p.failed(new InterruptedIOException()));assertEquals(0,p.failures());
 }
 @Test public void oneMinuteConnectionsCannotRepeatOneOfEightForever(){
  CompanionReconnectPolicy p=new CompanionReconnectPolicy();long now=0;
  for(int n=1;n<=8;n++){
   for(int i=0;i<65;i++){p.healthy(now);now+=1000;}
   assertTrue(p.failed(new IOException("radio")));assertEquals(n,p.failures());
  }
  assertFalse(p.allowed());assertEquals(30000,p.delayMs());
 }
 @Test public void onlyContinuousCompletedWorkReleasesBudget(){
  CompanionReconnectPolicy p=new CompanionReconnectPolicy();p.failed(new IOException());
  p.healthy(0);p.healthy(300001);assertEquals(1,p.failures());
  for(long n=301000;n<=601000;n+=1000)p.healthy(n);
  assertEquals(0,p.failures());
 }
}
