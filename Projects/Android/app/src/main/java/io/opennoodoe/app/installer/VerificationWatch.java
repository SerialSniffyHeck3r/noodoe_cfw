package io.opennoodoe.app.installer;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

/** Progress, not successful status replies, renews the inactivity allowance.
 * A bounded wall limit still catches cyclic/broken device progress. No writes
 * or reconnects are retried here; the caller keeps its durable journal. */
final class VerificationWatch {
 static final long STALL_MS=90_000, LIMIT_MS=30*60_000;
 private final long started;
 private long advanced;
 private final Map<String,Long> highWater=new HashMap<>();
 VerificationWatch(long now){started=advanced=now;}
 void observe(long now,BootstrapProgress.View v)throws IOException {
  String part=v.phase+":"+v.subphase+":"+v.kind;
  Long previous=highWater.get(part);
  if(previous==null||v.position>previous){highWater.put(part,v.position);advanced=now;}
  check(now);
 }
 void check(long now)throws IOException {
  if(now-started>=LIMIT_MS)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0631,"저장소 검사가 30분 한도를 넘었어요. 작업 기록을 보존했으며 쓰기 명령을 재전송하지 않았어요."));
  if(now-advanced>=STALL_MS)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0632,"기기 응답과 별개로 저장소 검사 진행량이 90초 동안 늘지 않았어요. 진단 자료를 공유해 주세요. 쓰기 명령을 재전송하지 않았어요."));
 }
}
