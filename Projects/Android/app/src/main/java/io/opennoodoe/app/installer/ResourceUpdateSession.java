package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import java.io.IOException;
import java.util.Arrays;
/** Inactive resources only. Live SDRAM resources and the previous valid slot
 * remain pinned. RAM reception is not presented as a durable checkpoint. */
final class ResourceUpdateSession {
 private final NdcpSession wire;
 ResourceUpdateSession(NdcpSession s){wire=s;}
 private static long u(byte[] b,int n){return ByteCodec.u32le(b,n);}
 private byte[] status()throws IOException{return shape(wire.request(0x8d,new byte[0]));}
 private byte[] shape(byte[] r)throws IOException{
  if(r.length!=96||u(r,4)!=1||u(r,8)>6||u(r,16)>u(r,20)||u(r,20)>524288)throw new IOException("Resource status ABI mismatch");
  return r;
 }
 private byte[] checked(byte[] r)throws IOException{
  shape(r);
  if(u(r,8)==5)throw new IOException("Resource write failed: "+u(r,24));
  if(u(r,8)==6)throw new IOException("Resource transfer was cancelled on Noodoe");return r;
 }
 static byte[] slot(byte[] container,byte[] required)throws IOException{
  if(container.length!=1048576)throw new IOException("Expected fixed resource container");
  for(int off:new int[]{0,524288}){
   if(Arrays.equals(required,Arrays.copyOfRange(container,off+16,off+48))&&u(container,off+4092)==0x434d5431L){
    long size=u(container,off+8),count=u(container,off+12);
    if(size>520192||count>252||count==0)throw new IOException("Invalid resource bounds");
    try{java.security.MessageDigest sha=java.security.MessageDigest.getInstance("SHA-256");
     sha.update(container,off+48,(int)count*16);sha.update(container,off+4096,(int)size);
     if(!Arrays.equals(sha.digest(),required))throw new IOException("Resource content hash mismatch");
    }catch(java.security.NoSuchAlgorithmException e){throw new IOException(e);}
    return Arrays.copyOfRange(container,off,off+((4096+(int)size+4095)&~4095));
   }
  }throw new IOException("Package lacks the resources required by its APP");
 }
 void ensure(RecoveryBundle bundle,byte[] product,InstallJournal journal,StockUpdateSession.Progress progress,TransferCapabilities caps)throws Exception{
  if(!caps.resources)return; // Older Product still verifies compatibility at COMMIT.
  byte[] required=Arrays.copyOfRange(product,0x20c,0x22c),remote=status();
  if(Arrays.equals(required,Arrays.copyOfRange(remote,32,64)))return;
  byte[] data=slot(bundle.image("resources"),required);
  if(u(remote,8)==4&&Arrays.equals(required,Arrays.copyOfRange(remote,64,96)))return;
  long tx=u(remote,12);
  boolean same=Arrays.equals(required,Arrays.copyOfRange(remote,64,96))&&u(remote,20)==data.length;
  if(u(remote,8)>=1&&u(remote,8)<=3&&!same)throw new IOException("A different resource transfer is pending; check the original package");
  if(!same||tx==0)tx=(new java.security.SecureRandom().nextInt()&0x7fffffffL)+1;
  journal.values.setProperty("resource.transaction",Long.toString(tx));journal.values.setProperty("resource.sha256",BootstrapProvisioner.toHex(required));
  if(!(same&&u(remote,8)==3)){
   byte[] begin=Arrays.copyOf(NdcpSession.words(tx,data.length),40);System.arraycopy(required,0,begin,8,32);
   journal.save("RESOURCE_BEGIN_RESULT_UNKNOWN");remote=checked(wire.request(0x8e,begin));
   long deadline=System.nanoTime()+180_000_000_000L;
   while(u(remote,8)==1){if(System.nanoTime()>deadline)throw new IOException("Resource arena preparation timed out");Thread.sleep(20);remote=status();}
   int offset=(int)u(remote,16);
   while(offset<data.length){int n=caps.count(offset,data.length-offset);byte[] chunk=Arrays.copyOf(NdcpSession.words(tx,offset),8+n);System.arraycopy(data,offset,chunk,8,n);
    remote=checked(wire.request(0x8f,chunk));if(u(remote,12)!=tx||u(remote,16)!=offset+n)throw new IOException("Resource reception position differs");offset+=n;
    progress.stage("file-resources",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0573,"변경된 자산 전송 (RAM 수신, 아직 저장 전)"),offset,data.length,"B");
   }
   journal.save("RESOURCE_COMMIT_RESULT_UNKNOWN");byte[] commit=Arrays.copyOf(NdcpSession.words(tx),36);System.arraycopy(required,0,commit,4,32);remote=checked(wire.request(0x90,commit));
  }
  long deadline=System.nanoTime()+180_000_000_000L;
  while(u(remote,8)!=4){
   checked(remote);
   if(u(remote,12)!=tx||!Arrays.equals(required,Arrays.copyOfRange(remote,64,96)))throw new IOException("Resource transaction changed");
   if(System.nanoTime()>deadline)throw new IOException("Resource publication result unknown; query before retrying");
   progress.stage("file-resources",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0574,"비활성 자산 슬롯 기록·물리 검증"),u(remote,28),524288,"B");
   if(caps.progress)progress.installStatus(new InstallProgressSnapshot(wire.request(0x8c,new byte[0])));
   Thread.sleep(200);remote=status();
  }
  if(u(remote,12)!=tx||!Arrays.equals(required,Arrays.copyOfRange(remote,64,96)))throw new IOException("Published resource transaction differs");
  journal.save("RESOURCE_SAVED");
 }
}
