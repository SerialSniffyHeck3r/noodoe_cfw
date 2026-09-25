package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import io.opennoodoe.app.protocol.ndcp.MaintenanceStatus;
import java.io.IOException;
import java.util.Arrays;

/** First identify, then await local intent on the SAME connection, then provision.
 * It never resets an uncertain file-publication or COMMIT journal to "new". */
public final class BootstrapInstallFlow {
 private BootstrapInstallFlow(){}
 public static void identified(InstallJournal journal)throws IOException {
  // Older APKs entered BACKUP before their first authorised read. Up to the
  // durable CREATE_BEGIN intent this phase only reads NOR (including 0x81).
  // Preserve its evidence and start a new read plan; never reuse partial data.
  if(journal.state().equals("BACKUP_IN_PROGRESS")&&!journal.values.containsKey("create.kind")){
   String previous=journal.values.getProperty("backup.directory");
   if(previous!=null)journal.values.setProperty("backup.abandoned."+System.currentTimeMillis(),previous);
   journal.save("BOOTSTRAP_IDENTIFIED");return;
  }
  if(Arrays.asList("IMPORTED","STOCK_IDENTIFIED","STOCK_BEGIN_RESULT_UNKNOWN","STOCK_START_RESULT_UNKNOWN",
    "STOCK_DATA_RESULT_UNKNOWN","STOCK_TERMINATE_RESULT_UNKNOWN","STOCK_DONE_RESULT_UNKNOWN","STOCK_ACCEPTED_WAIT_IGN_OFF").contains(journal.state()))
   journal.save("BOOTSTRAP_IDENTIFIED");
 }
 public static boolean resume(String state)throws IOException {
  if(state.equals("BOOTSTRAP_IDENTIFIED"))return false;
  if(Arrays.asList("PROVISIONED","NDCP_BEGIN_RESULT_UNKNOWN","NDCP_DATA_RESULT_UNKNOWN","NDCP_FINISH_RESULT_UNKNOWN","NDCP_VERIFIED","INSTALL_CONFIRM_CANCELLED").contains(state))return true;
  throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0389,"저장된 작업은 ")+state+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0390," 상태예요. 새 설치로 덮어쓰지 않았어요. 응답이 끊긴 작업 확인 또는 진단 자료 공유를 사용해 주세요."));
 }
 /** v2 re-audits the entire allocation graph and physically validates each
  * already published file. Legacy evidence never acquires this permission. */
 public static boolean resume(InstallJournal journal)throws IOException {
  if("scoped-v2".equals(journal.values.getProperty("backup.mode"))&&
     Arrays.asList("FILE_CREATED","CREATE_BEGIN_RESULT_UNKNOWN","CREATE_COMMIT_RESULT_UNKNOWN","POSTIMAGE_VERIFY_IN_PROGRESS").contains(journal.state()))return true;
  return resume(journal.state());
 }
 interface Wait {long now();void pause()throws InterruptedException;}
 public static void awaitLocalInstall(NdcpSession session,StockUpdateSession.Progress progress)throws Exception {
  awaitLocalInstall(session,progress,new Wait(){public long now(){return System.nanoTime()/1_000_000L;}public void pause()throws InterruptedException{Thread.sleep(250);}},180000);
 }
 static void awaitLocalInstall(NdcpSession session,StockUpdateSession.Progress progress,Wait clock,long timeout)throws Exception {
  long started=clock.now();
  while(clock.now()-started<timeout){
   MaintenanceStatus view=new MaintenanceStatus(session.request(0x5a,new byte[0]));
   if(view.state==8)throw new IOException(view.message());
   if(view.phase>=101||view.state==5||view.state==7)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0391,"누도가 설치 또는 복구 단계에 있어요. 완료 여부를 먼저 확인해 주세요. 새 설치를 시작하지 않았어요."));
   String message=view.state==0?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0392,"누도가 기존 파일을 검사 중이에요. 검사가 끝나면 설치 메뉴가 열립니다."):
    view.state==9?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0393,"Bluetooth 연결은 확인됐어요. 누도에서 O로 메뉴로 돌아가 Install CFW를 선택하고 O를 눌러 주세요."):
    view.phase==100?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0394,"누도에 이전 확인 화면이 남아 있어요. Back을 선택해 돌아간 뒤 Install CFW를 다시 선택해 주세요."):
    io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0395,"누도에서 Install CFW를 선택하고 O를 눌러 주세요. 이 연결을 유지한 채 다음 단계로 자동 진행해요. 키는 ON으로 유지해 주세요.");
   progress.stage("authorize",message,0,0,"");
   if((view.flags&1)!=0&&(view.state==2||view.state==3)&&view.phase<100){
    try{
     // Harmless read is an actual check of local intent, authentication and
     // completed startup audit. A connected radio alone is not permission.
     byte[] r=session.request(0x80,NdcpSession.words(0,2));
     if(r.length!=34||ByteCodec.u32le(r,8)!=0||ByteCodec.u32le(r,24)!=2)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0396,"설치 준비 확인 응답이 맞지 않아요."));
     progress.stage("audit",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0397,"설치 권한과 연결을 확인했어요. 복구 자료 보관과 파일 준비를 시작해요."),0,0,"");return;
    }catch(NdcpSession.DeviceRejected waiting){if(waiting.result!=2&&waiting.result!=3)throw waiting;}
   }
   clock.pause();
  }
  throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0398,"누도의 Install CFW 선택을 3분 동안 기다렸어요. 쓰기 작업은 시작하지 않았어요. 메뉴를 선택한 뒤 설치 계속을 눌러 주세요."));
 }
}
