package io.opennoodoe.app.companion;

import java.io.IOException;
import io.opennoodoe.app.protocol.ByteCodec;

/** Validated UART snapshot (NDCP 0x0a). Phone GPS never substitutes vehicle speed. */
public final class RideTelemetry {
 public final boolean fresh,odometerValid;public final long speed,odometer;
 public RideTelemetry(byte[] r)throws IOException {
  if(r.length<80||ByteCodec.u32le(r,0)!=0||ByteCodec.u32le(r,76)>259||r.length!=80+ByteCodec.u32le(r,76))throw new IOException("Invalid vehicle snapshot");
  long flags=ByteCodec.u32le(r,8),age=ByteCodec.u32le(r,20);
  speed=ByteCodec.u32le(r,24);odometer=ByteCodec.u32le(r,28);
  fresh=(flags&1)!=0&&ByteCodec.u32le(r,12)==0&&age<=3000&&speed<=400;
  odometerValid=(flags&2)!=0&&ByteCodec.u32le(r,12)==0&&age<=3000;
 }
}
