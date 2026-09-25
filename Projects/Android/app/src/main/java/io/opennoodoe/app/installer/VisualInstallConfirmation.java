package io.opennoodoe.app.installer;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
/** Durable user evidence is bound to one device, package, attempt and exact
 * candidate journal generation. It never substitutes for radio/health checks. */
final class VisualInstallConfirmation {
 static String binding(InstallJournal journal,long sequence,byte[] sha)throws IOException {
  String uid=journal.values.getProperty("uid");
  if(uid==null||uid.isEmpty())throw new IOException("Missing target identity");
  String s=journal.values.getProperty("address","")+"|"+uid+"|"+journal.values.getProperty("bundle","")+"|"+
   journal.values.getProperty("attempt.id","")+"|"+journal.values.getProperty("transaction","")+"|"+sequence+"|"+BootstrapProvisioner.toHex(sha);
  return RecoveryBundle.sha(s.getBytes(StandardCharsets.UTF_8));
 }
 static void require(InstallJournal journal,StockUpdateSession.Progress progress,String binding,long deadline)throws Exception {
  require(journal,progress,binding,deadline,()->{});
 }
 static void require(InstallJournal journal,StockUpdateSession.Progress progress,String binding,long deadline,StockUpdateSession.ScreenCheck check)throws Exception {
  if(binding.equals(journal.values.getProperty("visual.confirmed")))return;
  journal.values.setProperty("boot.waiting.for","screen_confirmation");journal.save(journal.state());
  progress.stage("screen-confirm",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.boot_screen_waiting,"Bluetooth 연결 완료. 화면 위의 확인 버튼을 눌러야 검사가 계속돼요."),0,0,"");
  progress.awaitScreen(binding,deadline,check);
  journal.values.setProperty("visual.confirmed",binding);
  journal.values.setProperty("boot.waiting.for","health_and_storage");
  // Retain the transaction state; this is evidence, not a remote completion.
  journal.save(journal.state());
 }
}
