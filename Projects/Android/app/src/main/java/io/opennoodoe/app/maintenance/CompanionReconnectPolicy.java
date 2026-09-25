package io.opennoodoe.app.maintenance;

import java.io.IOException;
import java.io.InterruptedIOException;
import io.opennoodoe.app.protocol.ndcp.NdcpClient;

/** Retry transport loss only. A local exception or an explicit rejection is
 * not evidence that re-pairing/reopening RFCOMM will help. Successful complete
 * owner iterations must stay healthy for five minutes to reset the budget. */
public final class CompanionReconnectPolicy {
 private int failures;
 private long healthySince=-1,lastHealthy=-1;
 public boolean allowed(){return failures<8;}
 public int failures(){return failures;}
 public long delayMs(){return Math.min(30000,1000L<<Math.min(failures,5));}
 public boolean failed(Exception error){
  healthySince=lastHealthy=-1;
  if(!(error instanceof IOException)||error instanceof NdcpClient.DeviceRejected||error instanceof InterruptedIOException)return false;
  failures++;return true;
 }
 public void healthy(long now){
  if(healthySince<0||now<lastHealthy||now-lastHealthy>10000)healthySince=now;
  lastHealthy=now;
  if(now-healthySince>=300000)failures=0;
 }
}
