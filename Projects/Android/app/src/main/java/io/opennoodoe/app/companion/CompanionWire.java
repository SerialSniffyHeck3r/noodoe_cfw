package io.opennoodoe.app.companion;
import io.opennoodoe.app.protocol.ByteCodec;
import io.opennoodoe.app.protocol.ndcp.NdcpClient;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
/** Product-only, one connection generation. Never sends cached settings on connect. */
public final class CompanionWire {
 public final NdcpClient client;public long epoch;public boolean ign,ignValid,speedValid,visuals,phonePanels,packedPanels,indicators,calls,marquee,packedMusic;public long speed,rideSession;public boolean fullSettings,callCards,largePhonePanels,compactPhonePanels;
 public CompanionWire(NdcpClient c){client=c;}
 public static byte[] words(long... v){byte[] b=new byte[v.length*4];for(int i=0;i<v.length;i++)ByteCodec.putU32le(b,i*4,v[i]);return b;}
 public static long u(byte[] b,int o){return ByteCodec.u32le(b,o);}
 public void connect()throws IOException{byte[] c=client.request(0x0d,new byte[0]);if(c.length!=16||u(c,4)!=2||u(c,8)!=2||(u(c,12)&251)!=251)throw new IOException("This firmware does not support companion protocol 1");callCards=(u(c,12)&131072)!=0;largePhonePanels=(u(c,12)&262144)!=0;compactPhonePanels=(u(c,12)&524288)!=0;fullSettings=(u(c,12)&32768)!=0;visuals=(u(c,12)&256)!=0;phonePanels=(u(c,12)&512)!=0;indicators=(u(c,12)&1024)!=0;calls=(u(c,12)&2048)!=0;marquee=(u(c,12)&4096)!=0;packedPanels=(u(c,12)&8192)!=0;packedMusic=(u(c,12)&16384)!=0;poll();}
 /** Readiness is application backpressure, not a broken RFCOMM socket.
  * Notification clear is idempotent and retried only after an explicit BUSY;
  * no uncertain command or persistent mutation is replayed here. */
 public void initialize()throws IOException {
  connect();long end=System.nanoTime()+5_000_000_000L;
  for(;;)try{send(0x75,words(0));return;}
  catch(NdcpClient.DeviceRejected busy){
   if(busy.result!=8||System.nanoTime()>=end)throw busy;
   try{Thread.sleep(100);}catch(InterruptedException e){Thread.currentThread().interrupt();throw new java.io.InterruptedIOException("Companion initialization cancelled");}
   poll();
  }
 }
 public void poll()throws IOException{byte[] r=client.request(0x70,new byte[0]);if(!((r.length==32&&u(r,4)==1)||(r.length==36&&u(r,4)==2))||u(r,8)==0)throw new IOException("Invalid companion status");long next=u(r,8);if(epoch!=0&&epoch!=next)throw new IOException("Connection generation changed");epoch=next;ignValid=u(r,12)==1;speedValid=u(r,20)==1;ign=u(r,12)==1&&u(r,16)==1;speed=u(r,24);rideSession=r.length==36?u(r,32):0;}
 public byte[] send(int op,byte[] data)throws IOException{byte[] b=new byte[4+data.length];ByteCodec.putU32le(b,0,epoch);System.arraycopy(data,0,b,4,data.length);return client.request(op,b);}
 public static byte[] text(String s,int max){if(s==null)s="";StringBuilder b=new StringBuilder();int n=0;for(int o=0;o<s.length();){int cp=s.codePointAt(o);o+=Character.charCount(cp);if(cp<32)cp=32;String c=new String(Character.toChars(cp));int bytes=c.getBytes(StandardCharsets.UTF_8).length;if(n+bytes>max)break;b.append(c);n+=bytes;}return b.toString().getBytes(StandardCharsets.UTF_8);}
 public void notification(long id,String app,String title,String body)throws IOException{byte[] a=text(app,23),t=text(title,47),b=text(body,95);byte[] out=new byte[7+a.length+t.length+b.length];ByteCodec.putU32le(out,0,id);out[4]=(byte)a.length;out[5]=(byte)t.length;out[6]=(byte)b.length;System.arraycopy(a,0,out,7,a.length);System.arraycopy(t,0,out,7+a.length,t.length);System.arraycopy(b,0,out,7+a.length+t.length,b.length);send(0x74,out);}
 public void notification(NotificationHistory.Entry e,long visual)throws IOException{
  byte[] a=text(e.app,23),t=text(e.title,47),b=text(e.body,95);byte[] out=new byte[19+a.length+t.length+b.length];
  ByteCodec.putU32le(out,0,e.id);out[4]=(byte)a.length;out[5]=(byte)t.length;out[6]=(byte)b.length;
  System.arraycopy(a,0,out,7,a.length);System.arraycopy(t,0,out,7+a.length,t.length);System.arraycopy(b,0,out,7+a.length+t.length,b.length);
  System.arraycopy(words(e.revision,visual,e.reply==null?0:1),0,out,7+a.length+t.length+b.length,12);send(0x74,out);
 }
 public void indicators(long source,boolean gps,List<NotificationHistory.Entry> entries,NotificationHistory history)throws IOException{
  if(!indicators)return;int count=Math.min(10,entries.size());byte[] data=new byte[12+count*16];System.arraycopy(words(source,gps?1:0,count),0,data,0,12);
  long now=android.os.SystemClock.elapsedRealtime();for(int i=0;i<count;i++){NotificationHistory.Entry e=entries.get(i);System.arraycopy(words(e.id,e.revision,Math.min(86400000,Math.max(0,now-e.received)),e.read?1:0),0,data,12+16*i,16);}
  byte[] r=send(0x7e,data);if(r.length!=12+12*count||u(r,4)!=1||u(r,8)!=count)throw new IOException("Indicator response differs");
  for(int i=0;i<count;i++){NotificationHistory.Entry e=entries.get(i);if(u(r,12+12*i)!=e.id||u(r,16+12*i)!=e.revision||u(r,20+12*i)>1)throw new IOException("Stale notification acknowledgement");if(u(r,20+12*i)==1){e.read=true;if(history!=null)history.markRead(e.id,e.revision);}}
 }
 public void music(boolean valid,boolean playing,long position,long duration,String title,String artist)throws IOException{music(valid,playing,position,duration,title,artist,0);}
 public void music(boolean valid,boolean playing,long position,long duration,String title,String artist,long key)throws IOException{byte[] t=text(title,63),a=text(artist,47);byte[] b=new byte[14+t.length+a.length+(visuals?4:0)];b[0]=(byte)(valid?1:0);b[1]=(byte)(playing?1:0);ByteCodec.putU32le(b,4,position);ByteCodec.putU32le(b,8,duration);b[12]=(byte)t.length;b[13]=(byte)a.length;System.arraycopy(t,0,b,14,t.length);System.arraycopy(a,0,b,14+t.length,a.length);if(visuals)ByteCodec.putU32le(b,14+t.length+a.length,key);send(0x76,b);}
 public List<long[]> settings()throws IOException{List<long[]> values=new ArrayList<>();for(int page=0;page<64;page+=8){byte[] r=client.request(0x71,words(page));if(r.length<12||u(r,4)!=1||u(r,8)>8||r.length!=12+24*u(r,8))throw new IOException("Settings schema differs");for(int i=0;i<u(r,8);i++){long[] row=new long[6];for(int j=0;j<6;j++)row[j]=u(r,12+24*i+4*j);values.add(row);}if(u(r,8)<8)return values;}throw new IOException("Setting catalog too large");}
 public long change(long field,int value)throws IOException{byte[] r=send(0x72,words(field,value));if(r.length!=8)throw new IOException("Setting response differs");return u(r,4);}
 public boolean saved(long id)throws IOException{byte[] r=send(0x73,words(id));if(r.length!=20||u(r,4)!=id)throw new IOException("Setting result differs");if(u(r,8)==0)return false;if(u(r,12)!=0)throw new IOException("Setting rejected: "+u(r,12));long result=u(r,16);if(result==1||result==8)return false;if(result!=0)throw new IOException("Setting not saved: "+result);return true;}
}
