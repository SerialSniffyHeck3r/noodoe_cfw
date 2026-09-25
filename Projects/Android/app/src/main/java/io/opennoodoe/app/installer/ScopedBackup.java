package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import java.io.*;
import java.util.*;

/** Fresh-install evidence: FAT/directories + exact changed extents, never a
 * claimed full NOR backup. Unchanged physical data is hashed on the device.
 * The sparse planning file is local only and MUST NOT be offered as a dump. */
final class ScopedBackup {
 static final int CHUNK=960;
 static final int[] ORDER={4,0,1,2,3,8,5,6,7};
 final NdcpSession session;final StockUpdateSession.Progress progress;
 final File file,folder;final long[] first=new long[9];
 byte[] beforeHash;long transferred;String readKey="audit",readText=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0588,"파일 배치와 디렉터리를 읽고 있어요.");
 final BootstrapProgress monitor;
 ScopedBackup(NdcpSession s,StockUpdateSession.Progress p,File f){session=s;progress=p;file=f;folder=f.getParentFile();monitor=new BootstrapProgress(s,p);}
 byte[] request(int op,byte[] data)throws IOException {
  byte[] r;long deadline=System.nanoTime()+60_000_000_000L;
  // Only a rejected read is retried. Unknown writes are never replayed.
  while(true){try{r=session.request(op,data);break;}catch(NdcpSession.DeviceRejected e){
   if(op!=0x80||e.result!=3||System.nanoTime()>=deadline)throw e;
   try{Thread.sleep(100);}catch(InterruptedException interrupted){Thread.currentThread().interrupt();throw new IOException("Interrupted while waiting for audit",interrupted);}
  }}
  if(r.length<32)throw new IOException("Truncated storage reply");
  if(ByteCodec.u32le(r,8)!=0||ByteCodec.u32le(r,4)==9){
   String detail=String.format(java.util.Locale.ROOT,"Storage error %08X (command %02X, state %d)",0x50000L|ByteCodec.u32le(r,8),op,ByteCodec.u32le(r,4));
   try{BootstrapProgress.View v=new BootstrapProgress.View(session.request(0x84,new byte[0]));progress.deviceStatus(v);
    String name=v.kind<BootstrapFatPlan.NAMES.length?BootstrapFatPlan.NAMES[(int)v.kind]:"unknown";
    detail+=String.format(java.util.Locale.ROOT,"; file %d (%s), phase %d.%d, %d/%d B",v.kind,name,v.phase,v.subphase,v.position,v.total);
    // The failed validator may have finished a whole-file read since the last
    // throttled progress sample. Show its actual offset without claiming success.
    progress.stage("file-"+v.kind,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0589,"기기에서 ")+name+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0590," 검사 오류를 보고했어요."),v.position,v.total,"B");
   }catch(IOException unavailable){/* Preserve the first operation error. */}
   throw new IOException(detail);
  }return r;
 }
 byte[] read(long off,int n)throws IOException {
  byte[] out=new byte[n];
  for(int at=0;at<n;){int k=Math.min(CHUNK,n-at);byte[] r=request(0x80,NdcpSession.words(off+at,k));
   if(r.length!=32+k||ByteCodec.u32le(r,24)!=off+at+k)throw new IOException("Scoped read offset mismatch");
   System.arraycopy(r,32,out,at,k);at+=k;transferred+=k;progress.stage(readKey,readText,at,n,"B");
  }return out;
 }
 void snapshot()throws Exception {
  save(new File(folder,"EVIDENCE.txt"),("Scoped installation evidence, not a complete NOR dump.\n"
    +"expected.bin is a sparse planning image: unread original file bodies are absent. Never flash it to a device.\n"
    +"metadata-original.bin and extent-*-original.bin contain physically read preimages.\n"
    +"Unchanged data is verified locally on-device with unchanged-sha256.bin.\n").getBytes(java.nio.charset.StandardCharsets.UTF_8));
  try(RandomAccessFile out=new RandomAccessFile(file,"rw")){
   out.setLength(0x8000000L);byte[] raw=read(0,0x9000);out.write(raw);
   save(new File(folder,"metadata-original.bin"),raw);
   byte[] meta=BootstrapFatPlan.swap(raw);boolean[] visited=new boolean[4080];
   directories(out,meta,Arrays.copyOfRange(meta,0x5000,0x9000),visited,0);
   out.seek(0x7f70000);out.write(read(0x7f70000,65536));out.getFD().sync();
  }
  try(BootstrapFatPlan audit=new BootstrapFatPlan(file)){audit.rejectLegacy();}
 }
 private void directories(RandomAccessFile out,byte[] meta,byte[] dir,boolean[] seen,int depth)throws Exception {
  if(depth>16)throw new IOException("Directory depth exceeded");
  for(int at=0;at<dir.length;at+=32){int lead=dir[at]&255,attr=dir[at+11]&255;
   if(lead==0)break;if(lead==229||lead==46||attr==15||(attr&8)!=0||(attr&16)==0)continue;
   int c=ByteCodec.u16le(dir,at+26),count=0;ByteArrayOutputStream content=new ByteArrayOutputStream();
   while(c<0xff8){if(c<2||c>=4080||seen[c]||++count>32)throw new IOException("Invalid directory chain");seen[c]=true;
    long address=BootstrapFatPlan.address(c);byte[] raw=read(address,32768);out.seek(address);out.write(raw);content.write(BootstrapFatPlan.swap(raw));
    int value=ByteCodec.u16le(meta,0x1000+c+c/2);c=(c&1)!=0?value>>>4:value&4095;
   }directories(out,meta,content.toByteArray(),seen,depth+1);
  }
 }
 byte[] begin(byte[][] images)throws Exception {
  byte[] metadata;
  try(BootstrapFatPlan plan=new BootstrapFatPlan(file)){
   metadata=plan.metadata.clone();for(int kind:ORDER)first[kind]=plan.allocate(kind,images[kind].length);
  }
  byte[] begin=new byte[68];System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(metadata)),0,begin,0,32);
  for(int k=0;k<9;k++)ByteCodec.putU32le(begin,32+4*k,first[k]);
  save(new File(folder,"scope-plan.bin"),begin);
  progress.update(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0591,"기존 파일을 기기 안에서 검사하고 있어요. 전체 저장소를 폰으로 전송하지 않아요."));
  progress.stage("baseline",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0592,"변경 전 원본 해시를 계산해요. 누도 내부 검사이며 사진을 전송하거나 덮어쓰지 않아요."),0,0,"");request(0x81,begin);byte[] status=poll("baseline");beforeHash=Arrays.copyOfRange(status,32,64);
  save(new File(folder,"unchanged-sha256.bin"),beforeHash);
  try(RandomAccessFile out=new RandomAccessFile(file,"rw")){
   for(int kind:ORDER){readKey="backup-"+kind;readText=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0593,"변경 전 복구 자료 보관 · ")+BootstrapFatPlan.NAMES[kind];progress.stage(readKey,readText,0,images[kind].length,"B");
    byte[] raw=read(first[kind],images[kind].length);save(new File(folder,"extent-"+kind+"-original.bin"),raw);
    out.seek(first[kind]);out.write(raw);
   }out.getFD().sync();
  }
  return beforeHash;
 }
 interface Wait {long now();void pause()throws InterruptedException;}
 /** Re-enter after stock restoration without re-creating or replacing files.
  * The old evidence remains untouched. Mandatory install bytes must match this
  * bundle; mutable settings/ride/photo/log records are validated by the device.
  * A new baseline covers this read-only attempt, not the historical install. */
 byte[] reuse(byte[][] images)throws Exception {
  byte[] start=new byte[72];
  try(BootstrapFatPlan plan=new BootstrapFatPlan(file)){
   if(plan.existingContainers()!=9)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0594,"CFW 파일이 일부만 있어요. 기존 파일은 변경하지 않았어요. 진단 자료를 보관해 주세요."));
   System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(plan.metadata)),0,start,0,32);
   for(int k=0;k<9;k++){first[k]=plan.existingAddress(k,images[k].length);ByteCodec.putU32le(start,32+4*k,first[k]);}
  }
  ByteCodec.putU32le(start,68,0x45535552L);
  // Keep the common 68-byte extent plan for subsequent same-attempt resume.
  save(new File(folder,"scope-plan.bin"),Arrays.copyOf(start,68));
  save(new File(folder,"REUSE.txt"),"Read-only adoption after stock round trip. New baseline; existing files are not replaced.\n".getBytes(java.nio.charset.StandardCharsets.UTF_8));
  progress.stage("baseline",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0595,"이미 준비된 파일을 재사용해요. 현재 상태를 새로 검사하며 기존 파일은 덮어쓰지 않아요."),0,0,"");
  request(0x81,start);beforeHash=Arrays.copyOfRange(poll("baseline"),32,64);
  save(new File(folder,"unchanged-sha256.bin"),beforeHash);
  try(RandomAccessFile out=new RandomAccessFile(file,"rw")){
   for(int kind:ORDER){
    readKey="file-resume-"+kind;readText=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0596,"기존 설치 파일 확인 · ")+BootstrapFatPlan.NAMES[kind];
    byte[] raw=read(first[kind],images[kind].length);
    save(new File(folder,"extent-"+kind+"-original.bin"),raw);
    out.seek(first[kind]);out.write(raw);
    if((kind==0||kind==4||kind==5||kind==6||kind==7)&&!Arrays.equals(BootstrapFatPlan.swap(raw),images[kind]))
     throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0597,"기존 ")+BootstrapFatPlan.NAMES[kind]+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0598," 내용이 이 설치 ZIP과 달라요. 덮어쓰지 않았어요. 진단 자료를 보관해 주세요."));
    request(0x59,NdcpSession.words(kind));poll(readKey);
   }out.getFD().sync();
  }
  return beforeHash;
 }
 byte[] poll(String context)throws Exception {
  return poll(context,new Wait(){public long now(){return System.nanoTime()/1_000_000L;}public void pause()throws InterruptedException{Thread.sleep(100);}});
 }
 byte[] poll(String context,Wait clock)throws Exception {
  VerificationWatch watch=new VerificationWatch(clock.now());
  while(true){byte[] r=request(0x56,new byte[0]);long state=ByteCodec.u32le(r,4);
   if(state==8&&r.length==64)return r;
   BootstrapProgress.View v=monitor.poll(context,clock.now());
   if(v!=null)watch.observe(clock.now(),v);else watch.check(clock.now());
   clock.pause();
  }
 }
 void finish()throws Exception {
  progress.update(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0599,"기존 파일 보존과 새 파일 기록을 확인하고 있어요."));request(0x82,new byte[0]);
  if(!Arrays.equals(beforeHash,Arrays.copyOfRange(poll("preserve"),32,64)))throw new IOException("Unchanged NOR hash mismatch");
  byte[] expected=new byte[0x9000];try(RandomAccessFile f=new RandomAccessFile(file,"r")){f.readFully(expected);}
  readKey="preserve";readText=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0600,"최종 파일 배치를 독립 설치 계획과 비교하고 있어요.");
  if(!Arrays.equals(expected,read(0,expected.length)))throw new IOException("Final FAT differs from independent plan");
  save(new File(folder,"metadata-final.bin"),expected);
 }
 void resume()throws Exception {
  byte[] plan=readFile(new File(folder,"scope-plan.bin"),68),metadata=readFile(new File(folder,"metadata-final.bin"),0x9000);
  beforeHash=readFile(new File(folder,"unchanged-sha256.bin"),32);
  if(!Arrays.equals(metadata,read(0,metadata.length)))throw new IOException("Published FAT changed; do not overwrite");
  byte[] start=Arrays.copyOf(plan,100);System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(BootstrapFatPlan.swap(metadata))),0,start,0,32);
  System.arraycopy(beforeHash,0,start,68,32);request(0x81,start);poll("baseline");
  for(int kind:ORDER){request(0x59,NdcpSession.words(kind));poll("file-resume-"+kind);}
  finish();
 }
 static byte[] readFile(File f,int n)throws IOException {try(InputStream in=new FileInputStream(f)){byte[] b=RecoveryBundle.bounded(in,n);if(b.length!=n)throw new IOException("Missing scoped evidence");return b;}}
 static void save(File f,byte[] data)throws IOException {try(FileOutputStream out=new FileOutputStream(f)){out.write(data);out.getFD().sync();}io.opennoodoe.app.diagnostics.DurableFiles.directory(f.getParentFile());}
}
