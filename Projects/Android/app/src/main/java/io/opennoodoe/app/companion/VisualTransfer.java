package io.opennoodoe.app.companion;
import java.io.*;
import java.util.*;
import java.util.zip.CRC32;
/** One bounded transfer, owner-loop pumping leaves time for control/GPS.
 * A disconnect discards this RAM transaction; persistent photo completion
 * must still be queried, not inferred from a successful FINISH response. */
public final class VisualTransfer {
 private final CompanionWire wire;private final byte[] bytes;private final long key,kind;
 private int offset,state;private long started;
 public VisualTransfer(CompanionWire wire,int kind,long key,byte[] bytes){if(kind<1||kind>9||key==0||bytes.length==0||bytes.length>131072||(kind==7&&bytes.length!=11184))throw new IllegalArgumentException();this.wire=wire;this.kind=kind;this.key=key;this.bytes=bytes.clone();}
 public boolean pump(long now)throws IOException{
  if(started==0)started=now;if(now-started>120000)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0704,"이미지 검증 시간이 지났어요. 완료 여부를 확인하지 못했어요."));
  if(state==0){CRC32 crc=new CRC32();crc.update(bytes);try{wire.send(0x79,CompanionWire.words(kind,key,bytes.length,crc.getValue()));}
   catch(io.opennoodoe.app.protocol.ndcp.NdcpClient.DeviceRejected e){if(e.result==8)return false;throw e;}state=1;}
  for(int i=0;state==1&&offset<bytes.length&&i<8;i++){
   int n=Math.min(960,bytes.length-offset);byte[] data=new byte[n+8];System.arraycopy(CompanionWire.words(key,offset),0,data,0,8);System.arraycopy(bytes,offset,data,8,n);
   byte[] reply=wire.send(0x7a,data);if(reply.length!=8||CompanionWire.u(reply,4)!=offset+n)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0705,"이미지 수신 위치가 맞지 않아요."));offset+=n;
  }
  if(state==1&&offset==bytes.length){wire.send(0x7b,CompanionWire.words(key));state=2;}
  if(state==2){byte[] r=wire.send(0x7c,new byte[0]);if(r.length!=24||CompanionWire.u(r,4)!=1||CompanionWire.u(r,8)!=key)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0706,"이미지 작업이 다른 요청으로 바뀌었어요."));
   // A page/track generation may end while a RAM tile is in flight. It is
   // discarded, never a reason to drop the otherwise healthy connection.
   if(isTile()&&CompanionWire.u(r,12)==4&&CompanionWire.u(r,16)==9){state=3;return true;}
   if(CompanionWire.u(r,12)==4||CompanionWire.u(r,16)!=0)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0707,"이미지 검증/저장 실패: ")+CompanionWire.u(r,16));
   long phase=CompanionWire.u(r,12);
   if((phase!=2&&phase!=3)||CompanionWire.u(r,20)!=bytes.length)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0708,"이미지 완료 상태 또는 길이가 맞지 않아요."));
   if(phase==3){state=3;return true;}}
  return state==3;
 }
 /** BEGIN/DATA can be safely restarted; decoding/physical writes must drain. */
 public boolean pauseForMusic(){if((kind!=6&&kind!=8)||state!=0)return false;return true;}
 public boolean isArt(){return kind==2;}
 public boolean isMusic(){return kind==1||kind==2||isTile();}
 public boolean isTile(){return kind==7||kind==9;}
 public boolean tileMatches(long track,long view,long index){return isTile()&&CompanionWire.u(bytes,0)==track&&CompanionWire.u(bytes,4)==view&&(index<0||CompanionWire.u(bytes,8)==index);}
 // One receiver owns BEGIN/DATA. Restarting an in-flight JPEG for each
 // scrolling tile starved artwork indefinitely. Prefetch may reorder work
 // before BEGIN only; an accepted image drains while commands stay serviced.
 public boolean pauseForTile(){return kind==2&&state==0;}
 public int received(){return offset;}public int total(){return bytes.length;}
}
