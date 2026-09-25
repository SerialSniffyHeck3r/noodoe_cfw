package io.opennoodoe.app.companion;

/** Integrates only contiguous fresh UART intervals. Distance is explicitly an
 * estimate; gaps and stale data accrue unknown time instead of invented speed. */
public final class RideAccumulator {
 public long movingMs,stoppedMs,unknownMs,maxKph,samples;public double distanceKm;
 private long previous=-1,lastSpeed;private boolean valid;
 public void add(long elapsed,RideTelemetry t,int stopThreshold){
  if(previous>=0){long dt=Math.max(0,elapsed-previous);
   if(dt<=3000&&valid&&t.fresh){
    if(lastSpeed<stopThreshold)stoppedMs+=dt;else movingMs+=dt;
    distanceKm+=(lastSpeed+t.speed)*0.5*dt/3600000.0;
   }else unknownMs+=dt;
  }
  previous=elapsed;valid=t.fresh;lastSpeed=t.speed;samples++;
  if(t.fresh)maxKph=Math.max(maxKph,t.speed);
 }
}
