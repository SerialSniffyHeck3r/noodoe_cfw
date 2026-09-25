package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import java.io.IOException;

/** Stock -> Bootstrap has its own read-only reconnect stage. Never repeats the
 * stock transfer, sends COMMIT, or interprets an absent radio as installed CFW. */
final class BootstrapConnectSession {
 interface Clock {long now();void sleep(long ms)throws InterruptedException;}
 static final class Unavailable extends IOException {Unavailable(Throwable cause){super(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0380,"Bootstrap 연결을 아직 확인하지 못했어요. 본체에 Bootstrap이 뜨면 Install CFW를 선택하고 ‘Bootstrap 자동 연결’을 눌러 주세요. 설치 파일을 다시 보낼 필요는 없어요."),cause);}}
 private final Clock clock;
 BootstrapConnectSession(){this(new Clock(){public long now(){return System.nanoTime()/1000000;}public void sleep(long ms)throws InterruptedException{Thread.sleep(ms);}});}
 BootstrapConnectSession(Clock clock){this.clock=clock;}
 String run(InstallerController.Connections connections,RecoveryBundle bundle,InstallJournal journal,StockUpdateSession.Progress progress)throws Exception {
  journal.require("IMPORTED","STOCK_IDENTIFIED","STOCK_BEGIN_RESULT_UNKNOWN","STOCK_START_RESULT_UNKNOWN","STOCK_DATA_RESULT_UNKNOWN",
   "STOCK_TERMINATE_RESULT_UNKNOWN","STOCK_DONE_RESULT_UNKNOWN","STOCK_ACCEPTED_WAIT_IGN_OFF","BOOTSTRAP_IDENTIFIED");
  progress.role("unknown");
  String ignition=journal.state().equals("STOCK_ACCEPTED_WAIT_IGN_OFF")?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0381,"상시 전원은 유지하고 순정 화면의 안내대로 IGN만 OFF → Bootstrap 화면 확인 후 ON으로 바꿔 주세요.\n"):"";
  progress.stage("bootstrap-connect",ignition+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0382,"Bootstrap에서 Install CFW를 선택해 주세요. 앱이 자동으로 페어링·연결을 이어가요. Android 요청이 뜨면 승인해 주세요."),0,0,"");
  long deadline=clock.now()+240000;Exception last=null;
  // A reboot/key cycle is readiness, not five failed connection attempts.
  // Keep the advertised four-minute window even when connect fails instantly.
  for(int attempt=1;clock.now()<deadline;attempt++){
   clock.sleep(attempt==1?1000:5000);
   if(Thread.currentThread().isInterrupted())throw new InterruptedException();
   if(clock.now()>=deadline)break;
   progress.connection("bootstrap_connect_attempt",attempt,-1);
   InstallerTransport opened;
   try{opened=connections.openBoot(progress);}
   catch(BondSession.Failure pairing){throw pairing;}
   catch(InterruptedException interrupted){throw interrupted;}
   catch(IOException absent){last=absent;continue;}
   try(InstallerTransport stream=opened){
    NdcpSession ndcp=new NdcpSession(stream);byte[] identity;
    try{identity=ndcp.runningIdentity(0);}
    catch(InterruptedException interrupted){throw interrupted;}
    catch(IOException absent){last=absent;continue;}
    // Once NDCP identifies a peer, a role/hash/UID/binding mismatch is terminal.
    // Do not hide it among radio retries or clear an unresolved journal.
    if(ByteCodec.u32le(identity,8)!=1)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0383,"연결된 기기가 Bootstrap이 아니에요. 현재 기기 확인으로 실제 실행 상태를 확인해 주세요."));
    ndcp.identity(bundle,journal,1);
    BootstrapInstallFlow.identified(journal);progress.reply();progress.role("bootstrap");
    progress.connection("bootstrap_connected",attempt,-1);
    return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0384,"Bootstrap에 연결됐어요. 본체에서 Install CFW를 선택하고 앱의 ‘설치 계속’을 누르세요. 설치 도구는 다시 보내지 않아요.");
   }
  }
  throw new Unavailable(last);
 }
}
