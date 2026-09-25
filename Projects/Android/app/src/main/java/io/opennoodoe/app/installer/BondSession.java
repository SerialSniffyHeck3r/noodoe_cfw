package io.opennoodoe.app.installer;
import java.io.IOException;

/** Public Android bond lifecycle, independent of socket-connect deadlines.
 * Never removes an existing bond, approves a dialog, or retries a rejection. */
final class BondSession {
 static final int NONE=10,BONDING=11,BONDED=12;
 interface Peer {int state();boolean request();void waitChange(long ms)throws InterruptedException;}
 interface Clock {long now();}
 static final class Failure extends IOException {Failure(String message){super(message);}}
 static void ensure(Peer peer,Clock clock,StockUpdateSession.Progress progress)throws IOException {
  if(Thread.currentThread().isInterrupted())throw new Failure(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0350,"페어링 대기를 중단했어요."));
  int state=peer.state();if(state==BONDED)return;
  long deadline=clock.now()+60000;boolean bonding=state==BONDING;
  progress.connection("bond_wait",0,state);
  progress.update(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0351,"누도와 페어링하고 있어요. Android의 페어링 요청이 뜨면 승인해 주세요. Bootstrap에서는 Install CFW를 선택해 두세요."));
  if(!bonding){
   progress.connection("bond_request",0,state);
   if(!peer.request()&&peer.state()==NONE)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0352,"Android가 페어링을 시작하지 못했어요. 본체에서 Install CFW를 선택한 뒤 자동 연결을 다시 눌러 주세요."));
  }
  while(clock.now()<deadline){
   state=peer.state();
   if(state==BONDED){progress.connection("bond_ready",0,state);return;}
   if(state==BONDING)bonding=true;
   else if(state==NONE&&bonding)throw new Failure(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0353,"페어링이 완료되지 않았어요. Android 요청과 본체의 Install CFW 선택을 확인한 뒤 다시 연결하세요."));
   try{peer.waitChange(Math.min(250,Math.max(1,deadline-clock.now())));}
   catch(InterruptedException e){Thread.currentThread().interrupt();throw new Failure(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0350,"페어링 대기를 중단했어요."));}
  }
  throw new Failure(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0354,"페어링 승인을 60초 동안 기다렸어요. Android 요청을 확인한 뒤 자동 연결을 다시 눌러 주세요."));
 }
}
