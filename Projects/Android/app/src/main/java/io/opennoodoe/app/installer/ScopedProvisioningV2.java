package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import java.io.*;
import java.util.*;

/** FAT-owned installation. Stock bytes never cross SPP; Product
 * crosses once. Every file retains independent expected SHA and physical device
 * verification, plus original/prepared/final FAT evidence. No whole-NOR claim. */
final class ScopedProvisioningV2 {
 private final NdcpSession session;private final InstallJournal journal;private final StockUpdateSession.Progress progress;
 private final BootstrapProgress monitor;private final ScopedBackup reader;
 private final File folder;private final CompactFatSnapshot snapshot;
 private final long[] addresses=new long[9];private int existing;
 ScopedProvisioningV2(NdcpSession s,InstallJournal j,StockUpdateSession.Progress p,File out)throws IOException{
  session=s;journal=j;progress=p;folder=out;snapshot=new CompactFatSnapshot(new File(out,"fat"));reader=new ScopedBackup(s,p,new File(out,"unused"));monitor=new BootstrapProgress(s,p);
 }
 private byte[] request(int op,byte[] b)throws IOException{return reader.request(op,b);}
 private byte[] waitState(int expected,String label)throws Exception{
  VerificationWatch watch=new VerificationWatch(System.nanoTime()/1000000L);
  for(;;){byte[] status=request(0x56,new byte[0]);if(ByteCodec.u32le(status,4)==expected)return status;
   long now=System.nanoTime()/1000000L;BootstrapProgress.View v=monitor.poll(label,now);if(v!=null)watch.observe(now,v);else watch.check(now);Thread.sleep(40);
  }
 }
 private void directories(byte[] meta,byte[] dir,boolean[] seen,int depth)throws Exception{
  if(depth>16)throw new IOException("Directory depth exceeded");
  for(int at=0;at<dir.length;at+=32){int lead=dir[at]&255,attr=dir[at+11]&255;
   if(lead==0)break;if(lead==229||lead==46||attr==15||(attr&8)!=0||(attr&16)==0)continue;
   int c=ByteCodec.u16le(dir,at+26),count=0;ByteArrayOutputStream content=new ByteArrayOutputStream();
   while(c<0xff8){if(c<2||c>=4080||seen[c]||++count>32)throw new IOException("Invalid directory chain");seen[c]=true;
    long address=BootstrapFatPlan.address(c);byte[] raw=reader.read(address,32768);snapshot.put(address,raw);content.write(BootstrapFatPlan.swap(raw));
    int value=ByteCodec.u16le(meta,0x1000+c+c/2);c=(c&1)!=0?value>>>4:value&4095;
   }directories(meta,content.toByteArray(),seen,depth+1);
  }
 }
 private byte[] audit(byte[][] images)throws Exception{
  byte[] original=reader.read(0,0x9000);snapshot.put(0,original);ScopedBackup.save(new File(folder,"metadata-original.bin"),original);
  byte[] meta=BootstrapFatPlan.swap(original);directories(meta,Arrays.copyOfRange(meta,0x5000,0x9000),new boolean[4080],0);
  snapshot.put(0x7f70000,reader.read(0x7f70000,65536));
  byte[] plan=new byte[76];System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(meta)),0,plan,0,32);
  try(BootstrapFatPlan fs=new BootstrapFatPlan(snapshot)){
   fs.rejectLegacy();for(int kind:ScopedBackup.ORDER){
    if(fs.hasContainer(kind)){existing|=1<<kind;addresses[kind]=fs.existingAddress(kind,images[kind].length);}
    else addresses[kind]=fs.allocate(kind,images[kind].length);
    ByteCodec.putU32le(plan,32+4*kind,addresses[kind]);
   }
  }
  ByteCodec.putU32le(plan,68,0x32504353);ByteCodec.putU32le(plan,72,existing==511?1:0);
  ScopedBackup.save(new File(folder,"scope-v2.bin"),plan);
  progress.stage("audit",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0601,"FAT 전체 할당·기존 파일·새 쓰기 범위를 대조하고 있어요."),0,0,"");
  request(0x81,plan);return Arrays.copyOfRange(waitState(8,"audit"),32,64);
 }
 void run(RecoveryBundle bundle,long[] uid,byte[][] images)throws Exception{
  ScopedBackup.save(new File(folder,"EVIDENCE.txt"),("Scoped-v2 FAT allocation evidence. No full NOR backup/hash is claimed.\nUnallocated file data is not backed up. Stock files and reserved ranges have no write grant.\nExisting owned CFW A/B/BOOT may be explicitly reseeded with physical preimages retained. A resource migration preserves one verified old slot and replaces only the other, retaining the full container preimage. Other existing files stay read-only.\nOriginal/prepared/final metadata and file SHA evidence are retained.\n").getBytes(java.nio.charset.StandardCharsets.UTF_8));
  TransferCapabilities caps=TransferCapabilities.query(session);
  byte[] proof=audit(images);
  if("restore".equals(journal.values.getProperty("reinstall.policy"))&&(existing&14)!=14)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0602,"복원할 설정·주행·사진 파일이 모두 있지 않아요. 새로 시작을 선택할 수 있어요. 아직 기록하지 않았어요."));
  journal.values.setProperty("backup.mode","scoped-v2");journal.save("BACKUP_IN_PROGRESS");
  for(int kind:ScopedBackup.ORDER){
   if((existing&(1<<kind))!=0){
    if(kind==0){
     // Old valid resources are not corruption. A new Bootstrap may need a
     // newer table; preserve one verified old slot and replace only the other.
     byte[] required=Arrays.copyOfRange(images[0],16,48);boolean matching=false;
     for(int slot=0;slot<2;slot++){
      byte[] header=BootstrapFatPlan.swap(reader.read(addresses[0]+slot*524288L,4096));
      if(ByteCodec.u32le(header,4092)==0x434d5431L&&Arrays.equals(required,Arrays.copyOfRange(header,16,48)))matching=true;
     }
     if(!matching){
      if(!caps.bootstrapResources)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0603,"기기에 이전 자산이 남아 있어요. 자산 이관을 지원하는 새 Bootstrap이 필요합니다. 순정 복귀 후 이번 ZIP으로 설치 도구를 다시 올려 주세요."));
      reader.readKey="file-resource-preimage";reader.readText=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0604,"기존 자산 보존 자료를 저장하고 있어요.");
      byte[] raw=reader.read(addresses[0],1048576);
      byte[] compact=ResourceUpdateSession.slot(images[0],required),slot=Arrays.copyOf(compact,524288);
      Arrays.fill(slot,compact.length,slot.length,(byte)255);
      install(0,slot,proof,raw);continue;
     }
    }
    if(kind>=5&&kind<=7){
     // A previous CFW/stock round trip retains owned A/B/journal files. Keep
     // their exact physical preimages; explicitly reseed only these extents.
     // This is fresh installation, not a routine update or compatibility bypass.
     byte[] raw=reader.read(addresses[kind],images[kind].length);
     if(!Arrays.equals(BootstrapFatPlan.swap(raw),images[kind])){install(kind,images[kind],proof,raw);continue;}
    }
    progress.stage("file-reuse-"+kind,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0605,"기존 파일 검증 · ")+BootstrapFatPlan.NAMES[kind],0,images[kind].length,"B");
    request(0x59,NdcpSession.words(kind));waitState(8,"file-reuse-"+kind);
    byte[] hashReply=request(0x89,new byte[0]);if(hashReply.length!=64)throw new IOException("Truncated existing-file SHA");
    byte[] digest=Arrays.copyOfRange(hashReply,32,64);
    ScopedBackup.save(new File(folder,"existing-"+kind+".sha256"),digest);
    // Resource validation uses the device's required slot hash, allowing an
    // unrelated older inactive slot to remain intact. Recovery stays exact.
    if((kind==4||kind==5||kind==6||kind==7)&&!Arrays.equals(digest,NdcpSession.hex(RecoveryBundle.sha(images[kind]))))throw new IOException("Existing "+BootstrapFatPlan.NAMES[kind]+" differs; nothing was overwritten");
   }else install(kind,images[kind],proof,null);
  }
  journal.save("POSTIMAGE_VERIFY_IN_PROGRESS");request(0x82,new byte[0]);waitState(8,"preserve");
  byte[] expected=snapshot.read(0,0x9000),actual=reader.read(0,0x9000);
  if(!Arrays.equals(expected,actual))throw new IOException("Final FAT differs from independent plan");
  ScopedBackup.save(new File(folder,"metadata-final.bin"),actual);
  journal.values.setProperty("planned.sha256",RecoveryBundle.sha(actual));journal.values.setProperty("backup.sha256",RecoveryBundle.sha(ScopedBackup.readFile(new File(folder,"scope-v2.bin"),76)));
  journal.save("PROVISIONED");session.request(0x1f,NdcpSession.words(uid[0],uid[1],uid[2],0x42414b32));
 }
 private void install(int kind,byte[] image,byte[] proof,byte[] preimage)throws Exception{
  boolean resource=kind==0&&preimage!=null;
  File out=new File(folder,BootstrapFatPlan.NAMES[kind]);if(!out.mkdirs())throw new IOException("Fresh file evidence required");
  byte[] before,after;
  try(BootstrapFatPlan fs=new BootstrapFatPlan(snapshot)){before=fs.metadata.clone();
   if((preimage==null?fs.allocate(kind,image.length):fs.existingAddress(kind,resource?1048576:image.length))!=addresses[kind])throw new IOException("Allocation changed");after=fs.metadata.clone();}
  ScopedBackup.save(new File(out,"metadata-before.bin"),BootstrapFatPlan.swap(before));ScopedBackup.save(new File(out,"metadata-after.bin"),BootstrapFatPlan.swap(after));
  byte[] digest=NdcpSession.hex(RecoveryBundle.sha(image));ScopedBackup.save(new File(out,"expected.sha256"),digest);
  byte[] begin=new byte[preimage==null?72:104];ByteCodec.putU32le(begin,0,kind);ByteCodec.putU32le(begin,4,image.length);System.arraycopy(digest,0,begin,8,32);System.arraycopy(proof,0,begin,40,32);
  if(preimage!=null){ScopedBackup.save(new File(out,"physical-before.bin"),preimage);System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(BootstrapFatPlan.swap(preimage))),0,begin,72,32);}
  journal.values.setProperty("create.kind",Integer.toString(kind));journal.values.setProperty("create.address",Long.toString(addresses[kind]));journal.save("CREATE_BEGIN_RESULT_UNKNOWN");request(preimage==null?0x52:0x88,begin);
  if(preimage!=null)waitState(3,"file-preimage-"+kind);
  if(kind==1||kind==2||kind==3||(kind==7&&!"restore".equals(journal.values.getProperty("reinstall.policy")))||kind==8){request(0x8a,new byte[0]);waitState(3,"file-generate-"+kind);}
  else for(int offset=0;offset<image.length;){
   progress.stage("file-upload-"+kind,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0424,"파일 준비 · ")+BootstrapFatPlan.NAMES[kind],offset,image.length,"B");
   if(offset==4096&&(kind==4||kind==6)){
    int bytes=kind==4?0x70000:0x60000;request(0x87,NdcpSession.words(offset,kind==4?0:1,0,bytes));waitState(3,"file-local-"+kind);offset+=bytes;continue;
   }
   int blank=0;while(blank<65536&&offset+blank<image.length&&image[offset+blank]==(byte)255)blank++;
   if(blank>=64){if(offset<4096)blank=Math.min(blank,4096-offset);request(0x83,NdcpSession.words(offset,blank));offset+=blank;continue;}
   int n=Math.min(960,Math.min(image.length-offset,4096-(offset&4095)));byte[] data=new byte[4+n];ByteCodec.putU32le(data,0,offset);System.arraycopy(image,offset,data,4,n);request(0x53,data);offset+=n;
  }
  request(0x54,new byte[0]);byte[] ready=waitState(6,"file-verify-"+kind);
  long destination=ByteCodec.u32le(ready,16);
  if((destination!=addresses[kind]&&(!resource||destination!=addresses[kind]+524288))||ByteCodec.u32le(ready,20)!=image.length)throw new IOException("Device allocation differs");
  ByteArrayOutputStream metadata=new ByteArrayOutputStream();for(int offset=0;offset<0x9000;offset+=480){byte[] r=request(0x56,NdcpSession.words(offset));metadata.write(r,32,r.length-32);}
  if(!Arrays.equals(after,metadata.toByteArray()))throw new IOException("Prepared FAT differs");
  ScopedBackup.save(new File(out,"device-prepared.bin"),metadata.toByteArray());journal.save("CREATE_COMMIT_RESULT_UNKNOWN");request(0x55,digest);waitState(8,"file-write-"+kind);
  snapshot.put(0,BootstrapFatPlan.swap(after));journal.save("FILE_CREATED");
 }
}
