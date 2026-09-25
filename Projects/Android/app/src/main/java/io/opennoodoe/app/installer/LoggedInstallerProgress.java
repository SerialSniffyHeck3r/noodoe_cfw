package io.opennoodoe.app.installer;
import io.opennoodoe.app.diagnostics.SessionLog;
import java.io.IOException;

/** Bounded numeric evidence, never album art, names or free-form messages.
 * Only the single installer worker calls this wrapper. Log failure stops work. */
final class LoggedInstallerProgress implements StockUpdateSession.Progress {
 private final StockUpdateSession.Progress target;private final SessionLog log;
 private String stage="";private long lastStage,lastDevice,lastError=-1,lastPhase=-1;
 private long installAt,installState=-1,installError=-1,installId=-1;
 private final long started=System.nanoTime();private long phaseAt=started,userWait;private String timingStage="";
 public void finishMetrics()throws IOException {long now=System.nanoTime();account(now);
  log.append("install_timing",SessionLog.fields("elapsed_ms",Long.toString((now-started)/1000000),"user_wait_ms",Long.toString(userWait/1000000),"active_ms",Long.toString((now-started-userWait)/1000000)));}
 private void account(long now){if(timingStage.equals("authorize")||timingStage.equals("confirm")||timingStage.equals("screen-confirm")||timingStage.equals("stock-reboot")||timingStage.equals("bootstrap-connect"))userWait+=now-phaseAt;phaseAt=now;}
 LoggedInstallerProgress(StockUpdateSession.Progress target,SessionLog log){this.target=target;this.log=log;}
 public void update(String text){target.update(text);}
 public void reply(){target.reply();}
 public void screenPrompt(){target.screenPrompt();}
 public void awaitScreen(String candidate,long deadline)throws Exception {awaitScreen(candidate,deadline,()->{});}
 public void awaitScreen(String candidate,long deadline,StockUpdateSession.ScreenCheck check)throws Exception {
  log.append("visual_confirmation_requested",SessionLog.fields("state","WAIT_USER_SCREEN"));
  try{target.awaitScreen(candidate,deadline,check);}
  catch(Exception e){log.append("visual_confirmation_interrupted",SessionLog.fields("state",e instanceof StockUpdateSession.ScreenConfirmationTimeout?"USER_TIMEOUT":"QUERY_OR_SESSION_FAILED"));throw e;}
  log.append("visual_confirmation_received",SessionLog.fields("state","USER_SAW_CFW"));
 }
 public void role(String role){target.role(role);}
 public void connection(String phase,int attempt,int bond)throws IOException {
  log.append("spp_connection",SessionLog.fields("state",phase,"sequence",Integer.toString(attempt),"result",Integer.toString(bond)));
  target.connection(phase,attempt,bond);
 }
 public void stage(String key,String text,long done,long total,String unit){
  long now=System.nanoTime();
  if(!key.equals(timingStage)){account(now);timingStage=key;}
  if(!stage.equals(key)||now-lastStage>5_000_000_000L){
   try{log.append("installer_stage",SessionLog.fields("state",key,"offset",Long.toString(done),"length",Long.toString(total)));}
   catch(IOException e){throw new IllegalStateException("Cannot save installer progress evidence",e);}
   stage=key;lastStage=now;
  }
  target.stage(key,text,done,total,unit);
 }
 public void installStatus(InstallProgressSnapshot s)throws IOException {
  long now=System.nanoTime();
  if(s.session!=installId||s.state!=installState||s.error!=installError||now-installAt>=5_000_000_000L){
   log.append("install_progress",SessionLog.fields("transaction",Long.toString(s.session),"sequence",Long.toString(s.sequence),"state",Long.toString(s.state),"offset",Long.toString(s.position),"length",Long.toString(s.total),"result",Long.toString(s.error)));
   installAt=now;installId=s.session;installState=s.state;installError=s.error;
  }
  target.installStatus(s);
 }
 public void deviceStatus(BootstrapProgress.View v)throws IOException {
  long now=System.nanoTime();
  if(v.error!=lastError||v.phase!=lastPhase||now-lastDevice>5_000_000_000L){
   log.append("bootstrap_status",SessionLog.fields("state",v.phase+"."+v.subphase,"action",Long.toString(v.kind),
    "offset",Long.toString(v.position),"length",Long.toString(v.total),"result",Long.toString(v.error),"reason",Long.toString(v.idle),"epoch",Long.toString(v.epoch)));
   lastDevice=now;lastError=v.error;lastPhase=v.phase;
  }
  target.deviceStatus(v);
 }
}
