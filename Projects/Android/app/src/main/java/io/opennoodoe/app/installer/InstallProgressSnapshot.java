package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import java.io.IOException;
import java.util.Locale;
/** Device-owned progress, independent of Activity and transport reconnections. */
public final class InstallProgressSnapshot {
 public final long session,sequence,state,stage,stages,phase,file,files,position,total,verified,sector,sectors,elapsed,idle,error;
 public final boolean cancellable,connected;
 public InstallProgressSnapshot(byte[] b)throws IOException {
  if(b.length!=80||u(b,0)!=0||u(b,1)!=1||u(b,4)>13||u(b,6)!=8||u(b,5)>8||u(b,17)>1||u(b,19)>1)
   throw new IOException("Install progress ABI mismatch");
  session=u(b,2);sequence=u(b,3);state=u(b,4);stage=u(b,5);stages=u(b,6);phase=u(b,7);file=u(b,8);files=u(b,9);
  position=u(b,10);total=u(b,11);verified=u(b,12);sector=u(b,13);sectors=u(b,14);elapsed=u(b,15);idle=u(b,16);cancellable=u(b,17)!=0;error=u(b,18);connected=u(b,19)!=0;
  if(position>total&&total!=0||sector>sectors)throw new IOException("Invalid device progress counters");
 }
 private static long u(byte[] b,int i){return ByteCodec.u32le(b,i*4);}
 public int overallPercent(){return state==8?100:(int)Math.min(99,stage*100/stages);}
 public String detail(){return String.format(Locale.ROOT,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0547,"기기 기준 · 단계 %d/%d · 파일 %d/%d\n현재 섹터 %d/%d (4KiB) · 검증 %,d B\n%s"),stage,stages,files>0?Math.min(files,file+1):0,files,sector,sectors,verified,
  state==13?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0548,"결과 미확인 — 상태 조회 필요"):state==10?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0549,"현재 기록 종료 후 취소"):state==11?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0550,"안전 취소 완료"):!connected?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0551,"마지막 확인 상태 · 재연결 대기"):cancellable?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0552,"O 새로 3초 유지: 기기 작업 취소"):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0553,"확정 이후 — 상시 전원을 유지하세요"));}
}
