package io.opennoodoe.app.companion;

import android.content.Context;
import android.os.SystemClock;
import java.io.IOException;

/** Opt-in phone recording; runs on the companion's single socket owner.
 * Local storage failures are visible but never stop music/location transport. */
public final class RideSync implements AutoCloseable {
 private final RideHistory history;private final CompanionWire wire;private final String device;
 private RideAccumulator totals;private long record,session=-1,next;private int threshold;private boolean enabled;
 public String status=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0697,"폰 주행 기록 꺼짐");
 public RideSync(Context context,CompanionWire wire,String address,int stopThreshold){history=new RideHistory(context);this.wire=wire;device=address;threshold=Math.max(0,Math.min(10,stopThreshold));}
 public void threshold(int value){threshold=Math.max(0,Math.min(10,value));}
 /** Called by the socket owner only. A new opt-in starts a new interval;
  * disabled recording never requests telemetry or stops other companion data. */
 public void enabled(boolean value){if(enabled==value)return;enabled=value;next=0;
  if(!value){finish("ended");status=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0697,"폰 주행 기록 꺼짐");}else status=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0698,"주행 기록 · 시동 대기");
 }
 public void tick()throws IOException {
  if(!enabled)return;
  long now=SystemClock.elapsedRealtime();if(now<next)return;next=now+1000;
  if(!wire.ignValid){status=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0699,"주행 기록 · 시동 상태 확인 중");return;}
  if(!wire.ign){finish("ended");status=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0698,"주행 기록 · 시동 대기");return;}
  RideTelemetry sample=new RideTelemetry(wire.client.request(0x0a,new byte[0]));
  try {
   if(record==0||session!=wire.rideSession){finish("interrupted");totals=new RideAccumulator();record=history.begin(device,wire.rideSession,System.currentTimeMillis());session=wire.rideSession;}
   totals.add(now,sample,threshold);history.save(record,totals,sample,System.currentTimeMillis());
   status=sample.fresh?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0700,"주행 기록 저장 중"):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0701,"주행 기록 · UART 수신 대기 (미확인 구간)");
  }catch(RuntimeException e){status=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0702,"주행 기록 저장 실패 · 폰 저장 공간을 확인해 주세요");next=now+5000;record=0;}
 }
 private void finish(String state){if(record==0)return;try{history.finish(record,state);}catch(RuntimeException e){status=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0703,"주행 기록 마감 실패 · 미종료 기록으로 보존");}record=0;}
 @Override public void close(){finish("interrupted");history.close();}
}
