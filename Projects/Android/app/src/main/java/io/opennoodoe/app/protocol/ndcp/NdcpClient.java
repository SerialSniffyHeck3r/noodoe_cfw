package io.opennoodoe.app.protocol.ndcp;
import io.opennoodoe.app.protocol.ByteCodec;
import io.opennoodoe.app.transport.SppTransport;
import java.io.*;
import java.util.*;
import java.util.zip.CRC32;
/** Same NDCP v1 bytes as firmware NDCP.c. Reuse for Product later; no UI policy. */
public final class NdcpClient {
 private final SppTransport transport;
 private final long timeoutNanos;
 private int sequence=1;
 private int lastOpcode=-1;
 public int lastOpcode(){return lastOpcode;}
 public int lastSequence(){return sequence;}
 private byte[] pending=new byte[0];
 public interface RequestHandler { long handle(int opcode,byte[] payload); }
 private RequestHandler handler;
 private long lastEventId=-1,lastEventResult;
 public void setRequestHandler(RequestHandler value){handler=value;}
 public static class DeviceRejected extends IOException {
  public final int opcode; public final long result;
  public DeviceRejected(int opcode,long result){super("Device rejected opcode "+opcode+" result="+result);this.opcode=opcode;this.result=result;}
 }
 public NdcpClient(SppTransport transport){this(transport,60000);}
 public NdcpClient(SppTransport transport,long timeoutMs){if(timeoutMs<100||timeoutMs>60000)throw new IllegalArgumentException("Request deadline");this.transport=transport;timeoutNanos=timeoutMs*1000000L;}
    public static byte[] encode(int opcode,int sequence,byte[] payload,int flags) {
        if(payload.length>1024 || (flags&~3)!=0) throw new IllegalArgumentException("NDCP bounds");
        byte[] wire=new byte[20+payload.length]; wire[0]='N';wire[1]='D';wire[2]='C';wire[3]='P';wire[4]=1;wire[5]=(byte)opcode;
        ByteCodec.putU16le(wire,6,flags);ByteCodec.putU32le(wire,8,sequence&0xffffffffL);ByteCodec.putU16le(wire,12,payload.length);
        System.arraycopy(payload,0,wire,16,payload.length);ByteCodec.putU32le(wire,wire.length-4,crc(wire,wire.length-4));return wire;
    }
    public synchronized byte[] request(int opcode,byte[] payload) throws IOException {
        int id=++sequence;lastOpcode=opcode;
        if(transport instanceof CommandAudit)((CommandAudit)transport).beforeCommand(opcode,id,payload.length);
        transport.send(encode(opcode,id,payload,0));
        long deadline=System.nanoTime()+timeoutNanos;
        while(System.nanoTime()<deadline) {
            byte[] in;
            try{in=transport.receive(Math.max(1,(deadline-System.nanoTime())/1_000_000));}
            catch(IOException failure){throw new IOException(String.format(java.util.Locale.ROOT,"NDCP response failed (opcode 0x%02X, sequence %d): %s",opcode,id,failure.getMessage()),failure);}
            if(pending.length+in.length>8192) throw new IOException("NDCP receive bound exceeded");
            byte[] all=Arrays.copyOf(pending,pending.length+in.length);System.arraycopy(in,0,all,pending.length,in.length);pending=all;
            while(pending.length>=16) {
                int length=ByteCodec.u16le(pending,12),flags=ByteCodec.u16le(pending,6);
                if(pending[0]!='N'||pending[1]!='D'||pending[2]!='C'||pending[3]!='P'||pending[4]!=1
                        ||length>1024||(flags&~3)!=0||ByteCodec.u16le(pending,14)!=0) {pending=Arrays.copyOfRange(pending,1,pending.length);continue;}
                if(pending.length<20+length) break;
                byte[] frame=Arrays.copyOf(pending,20+length);pending=Arrays.copyOfRange(pending,frame.length,pending.length);
                if(crc(frame,frame.length-4)!=ByteCodec.u32le(frame,frame.length-4)) throw new IOException("NDCP CRC mismatch; result unknown");
                if(flags==0){
                    long event=ByteCodec.u32le(frame,8);int op=frame[5]&255;
                    if(((op==0x10&&length==4)||((op==0x11||op==0x12)&&length==20))&&(event&0x80000000L)!=0){
                        // One result per command ID in this connection. Never
                        // toggle playback twice because the ACK was lost.
                        if(event!=lastEventId){
                            if(lastEventId!=-1&&((event-lastEventId)&0xffffffffL)>=0x80000000L)continue;
                            lastEventResult=handler==null?7:handler.handle(op,Arrays.copyOfRange(frame,16,16+length));lastEventId=event;
                        }
                        byte[] reply=new byte[4];ByteCodec.putU32le(reply,0,lastEventResult);
                        transport.send(encode(op,(int)event,reply,lastEventResult==0?1:3));
                    }
                    continue;
                }
                if(ByteCodec.u32le(frame,8)!=(id&0xffffffffL)||(frame[5]&255)!=opcode) continue;
                if((flags&1)==0||length<4) throw new IOException("Invalid NDCP reply");
                byte[] answer=Arrays.copyOfRange(frame,16,16+length);long result=ByteCodec.u32le(answer,0);
                if(((flags&2)!=0)!=(result!=0)) throw new IOException("NDCP result/flags disagree");
                if(transport instanceof CommandAudit)((CommandAudit)transport).commandResult(opcode,id,result);
                if(result!=0) throw new DeviceRejected(opcode,result);
                return answer;
            }
        }
        throw new IOException("NDCP timeout; result unknown");
    }
 public static long crc(byte[] b,int n){CRC32 c=new CRC32();c.update(b,0,n);return c.getValue();}
}
