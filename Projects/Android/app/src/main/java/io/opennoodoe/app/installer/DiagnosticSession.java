package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import java.io.*;
import java.util.Arrays;

/** Temporary role3 never passes through Product confirmation or preference
 * reset. The same NDCP connection/CRC and routine update sender are reused. */
public final class DiagnosticSession {
 private DiagnosticSession(){}
 private static long u(byte[] b,int n){return ByteCodec.u32le(b,n);}
 public static void verify(NdcpSession s,RecoveryBundle bundle,InstallJournal j)throws Exception {
  byte[] r=s.runningIdentity(3);
  String uid=u(r,12)+","+u(r,16)+","+u(r,20);
  if(!uid.equals(j.values.getProperty("uid"))||
     !Arrays.equals(Arrays.copyOfRange(r,24,56),NdcpSession.hex(RecoveryBundle.sha(bundle.image("diagnostic")))))
   throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0438,"진단 펌웨어 또는 기기가 전송 기록과 달라요. 본체 복구 메뉴를 확인해 주세요."));
  j.save("DIAGNOSTIC_RUNNING");
 }
 public static String waitForBoot(InstallerController.Connections c,RecoveryBundle b,InstallJournal j,StockUpdateSession.Progress p)throws Exception {
  Exception last=null;
  for(int attempt=1;attempt<=5;attempt++){
   p.stage("reboot",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0439,"진단 펌웨어에 다시 연결 중 · ")+attempt+"/5",0,0,"");Thread.sleep(attempt==1?2500:3000);
   try(InstallerTransport t=c.open(p)){
    NdcpSession s=new NdcpSession(t);verify(s,b,j);p.role("diagnostic");
    return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0440,"진단 펌웨어가 실행됐어요. 본체 UP/DOWN으로 항목을 선택하고 O로 시험하세요. 끝나면 Back to stock → O 2초로 순정 복귀합니다. 설정·사진은 초기화하지 않았어요.");
   }catch(Exception e){last=e;}
  }
  throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0441,"진단 연결을 확인하지 못했어요. 본체 화면에서 시험하거나 Back to stock을 선택할 수 있어요. 설치를 자동 반복하지 않았어요."),last);
 }
 public static String view(NdcpSession s,boolean stock)throws Exception {
  s.runningIdentity(3);byte[] r=s.request(0x96,new byte[0]);
  if(r.length!=76||u(r,4)!=1||u(r,8)!=3)throw new IOException("Unknown diagnostic status");
  if(stock){s.request(0x96,NdcpSession.words(u(r,72),7));
   return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0442,"누도에서 O를 놓았다가 새로 2초 눌러 순정 복귀를 확인해 주세요. 연결이 끊겨도 Gate의 버튼 복구는 사용할 수 있어요.");}
  return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0443,"진단 모드\nBT 상태 ")+u(r,28)+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0444," · 오류 ")+u(r,32)+
   io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0445,"\n조도 RAW ")+u(r,36)+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0444," · 오류 ")+u(r,48)+
   io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0446,"\n저장소 오류 ")+u(r,52)+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0447," · 로그 오류 ")+u(r,64)+
   io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0448,"\n시험 뒤 본체의 Back to stock에서 순정으로 돌아가세요.");
 }
 /** Complete read-only NOR export, fsync before claiming success. Partial
  * evidence is retained after interruption; no acknowledgements erase logs. */
 public static File logs(NdcpSession s,File directory,StockUpdateSession.Progress p)throws Exception {
  s.runningIdentity(3);if(!directory.isDirectory()&&!directory.mkdirs())throw new IOException("Log directory unavailable");
  File f=new File(directory,"diagnostic-"+System.currentTimeMillis()+".bin.partial");
  try(FileOutputStream out=new FileOutputStream(f)){
   long deadline=System.nanoTime()+10_000_000_000L;
   for(int off=0;off<0x40000;){int n=Math.min(960,0x40000-off);byte[] r;
    try{r=s.request(0x97,NdcpSession.words(off,n));}
    catch(NdcpSession.DeviceRejected wait){
     // Only a read-only first request may wait for the active journal write.
     if(off!=0||wait.result!=8||System.nanoTime()>=deadline)throw wait;
     Thread.sleep(200);continue;}
    if(r.length!=n+4)throw new IOException("Diagnostic log length differs");out.write(r,4,n);off+=n;
    p.stage("audit",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0449,"진단 로그를 폰에 저장 중"),off,0x40000,"B");}
   out.getFD().sync();
  }
  File complete=new File(directory,f.getName().replace(".partial",""));if(!f.renameTo(complete))throw new IOException("Log finalize failed");return complete;
 }
}
