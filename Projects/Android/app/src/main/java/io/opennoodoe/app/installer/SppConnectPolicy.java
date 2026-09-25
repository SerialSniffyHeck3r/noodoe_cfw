package io.opennoodoe.app.installer;
import java.io.IOException;

/** Retry only creation/connection of fresh secure sockets before any protocol
 * bytes are sent. A failed protocol/write is never replayed by this policy. */
final class SppConnectPolicy {
 interface Attempt<T> {T open(int attempt)throws IOException;void prepareRetry(int attempt)throws IOException;}
 static <T> T connect(Attempt<T> attempt)throws IOException {
  return connect(attempt,3);
 }
 static <T> T connect(Attempt<T> attempt,int attempts)throws IOException {
  if(attempts<1||attempts>3)throw new IllegalArgumentException("SPP attempts");
  IOException previous=null;
  for(int i=1;i<=attempts;i++){
   if(Thread.currentThread().isInterrupted())throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0606,"SPP 연결을 중단했어요."));
   if(i>1)attempt.prepareRetry(i);
   try{return attempt.open(i);}catch(IOException e){if(previous!=null)e.addSuppressed(previous);previous=e;}
  }
  throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0607,"SPP 연결에 ")+attempts+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0608,"회 실패했어요. 페어링은 삭제하지 않았어요. 순정 앱이 연결 중인지 확인하고 진단 자료를 공유해 주세요."),previous);
 }
}
