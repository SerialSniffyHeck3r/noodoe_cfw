package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import java.io.IOException;
import java.util.Arrays;

/** Read-only role discovery across the radio-less stock installer/Gate.
 * A live Bootstrap is not a failed Product connection or a healthy Product.
 * COMMIT/RESET and firmware bytes are never sent by this handoff. */
final class BootHandoffSession {
 interface Clock {long now();void sleep(long ms)throws InterruptedException;}
 private final Clock clock;
 BootHandoffSession(){this(new Clock(){public long now(){return System.nanoTime();}public void sleep(long ms)throws InterruptedException{Thread.sleep(ms);}});}
 BootHandoffSession(Clock clock){this.clock=clock;}
 String run(InstallerController.Connections connections,RecoveryBundle bundle,InstallJournal journal,StockUpdateSession.Progress progress)throws Exception {
  progress.screenPrompt();
  progress.stage("reboot",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0355,"누도가 설치를 마치고 다시 시작하고 있어요. 앱은 자동으로 연결해요."),0,0,"");progress.role("unknown");
  long deadline=clock.now()+240_000_000_000L;boolean productSeen=false;Exception last=null;int failures=0;
  while(clock.now()<deadline){
   int attempt=failures+1;clock.sleep(BootReconnectPolicy.handoffDelayMs(attempt));if(clock.now()>=deadline)break;
   progress.update(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0356,"누도에 다시 연결하고 있어요 · ")+attempt);
   try(InstallerTransport radio=connections.openBoot(progress)){
    NdcpSession ndcp=new NdcpSession(radio,BootReconnectPolicy.REPLY_TIMEOUT_MS);
    while(clock.now()<deadline){
     byte[] identity=ndcp.runningIdentity(0);long role=ByteCodec.u32le(identity,8);
     String uid=ByteCodec.u32le(identity,12)+","+ByteCodec.u32le(identity,16)+","+ByteCodec.u32le(identity,20);
     if(!uid.equals(journal.values.getProperty("uid")))throw new ProductBootSession.TerminalFailure(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0357,"연결된 누도의 UID가 설치 대상과 달라요."));
     if(role==2){
      if(!productSeen){productSeen=true;deadline=clock.now()+175_000_000_000L;progress.connection("product_boot_seen",attempt,0);}
      new ProductBootSession(ndcp).confirm(bundle,journal,progress,deadline);
      return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0358,"업데이트가 끝났어요. 이제 사용할 수 있어요.");
     }
     if(role!=1||productSeen||!Arrays.equals(Arrays.copyOfRange(identity,24,56),NdcpSession.hex(RecoveryBundle.sha(bundle.paddedImage("bootstrap")))))
      throw new ProductBootSession.TerminalFailure(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0359,"예상한 설치 프로그램이 아니에요. 본체 화면과 작업 기록을 확인해 주세요."));
     byte[] status=ndcp.status();long state=ByteCodec.u32le(status,4);
     if(state==6)throw new ProductBootSession.TerminalFailure(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0360,"본체의 설치가 중단됐어요. 본체 오류와 작업 기록을 확인해 주세요."));
     failures=0;progress.connection("bootstrap_still_installing",attempt,0);
     progress.screenPrompt();
  progress.stage("reboot",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0361,"파일은 전달됐어요. 누도가 설치 중이에요. 새 CFW가 켜지면 자동으로 연결해요."),0,0,"");
     clock.sleep(1000);
    }
   }catch(ProductBootSession.TerminalFailure terminal){throw terminal;}
   catch(StockUpdateSession.ScreenConfirmationTimeout expired){throw expired;}
   catch(BondSession.Failure pairing){throw pairing;}
   catch(InterruptedException interrupted){throw interrupted;}
   catch(Exception failure){
    last=failure;failures++;progress.connection("boot_reconnect_failed",failures,0);
    // Preserve the command/phase that actually failed, not just a final generic
    // reconnect error. No payloads, names, coordinates or pairing keys here.
    journal.values.setProperty("boot.last.failure",failure.getClass().getSimpleName()+": "+failure.getMessage());
    journal.values.setProperty("boot.reconnect.failures",Integer.toString(failures));
    journal.save(journal.state());
   }
  }
  throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0362,"새 CFW와 연결을 확인하지 못했어요. 페어링을 지웠다면 Android의 페어링 요청을 승인해 주세요. 본체가 순정으로 돌아왔다면 ‘본체에서 순정으로 돌아왔어요 · 확인’을 누르세요.")+(last==null?"":"\n"+last.getMessage()),last);
 }
}
