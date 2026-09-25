package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import java.io.*;
import java.nio.file.Files;
import java.util.*;
import org.junit.Test;
import static org.junit.Assert.*;

/** Transport mock checks host bytes/evidence; real ARM allocator is tested separately. */
public final class ScopedBackupTest {
 static final int[] SIZES={1048576,131072,262144,1048576,524288,524288,524288,65536,262144};
 static final class Radio implements InstallerTransport {
  byte[] logical=new byte[0x9000],pending,plan;long readBytes;int commands,busy=1,inspections;boolean wrongOffset;
  final Map<Long,byte[]> extents=new HashMap<>();
  Radio(){ByteCodec.putU16le(logical,11,4096);logical[13]=8;ByteCodec.putU16le(logical,14,1);logical[16]=2;
   ByteCodec.putU16le(logical,17,512);logical[21]=(byte)248;ByteCodec.putU16le(logical,22,2);ByteCodec.putU32le(logical,32,0x7f80);
   logical[510]=0x55;logical[511]=(byte)0xaa;for(int off:new int[]{0x1000,0x3000}){logical[off]=(byte)248;logical[off+1]=(byte)255;logical[off+2]=(byte)255;}}
  public void send(byte[] f)throws IOException {
   int op=f[5]&255;byte[] r;commands++;
   if(op==0x80){int off=(int)ByteCodec.u32le(f,16),n=(int)ByteCodec.u32le(f,20);assertTrue(n>0&&n<=960);
    if(busy-->0){pending=NdcpSession.encode(op,(int)ByteCodec.u32le(f,8),NdcpSession.words(3),3);return;}
    r=Arrays.copyOf(NdcpSession.words(0,8,0,0,0,0,off+n+(wrongOffset?1:0),1),32+n);byte[] raw=BootstrapFatPlan.swap(logical);
    for(int i=0;i<n;i++){long address=off+i;r[32+i]=address<raw.length?raw[(int)address]:(byte)255;
     for(Map.Entry<Long,byte[]> e:extents.entrySet())if(address>=e.getKey()&&address-e.getKey()<e.getValue().length)r[32+i]=e.getValue()[(int)(address-e.getKey())];
    }readBytes+=n;
   }else if(op==0x81){plan=Arrays.copyOfRange(f,16,f.length-4);r=NdcpSession.words(0,12,0,0,0,0,0,0);}
   else if(op==0x56||op==0x82||op==0x59){if(op==0x59)inspections++;r=Arrays.copyOf(NdcpSession.words(0,8,0,0,0,0,0,1),64);Arrays.fill(r,32,64,(byte)42);}
   else throw new IOException("Unexpected write/command "+op);
   pending=NdcpSession.encode(op,(int)ByteCodec.u32le(f,8),r,1);
  }
  public byte[] receive(long timeout){byte[] r=pending;pending=null;return r;}public void close(){}
 }
 @Test public void sparseEvidenceReadsOnlyChangedExtentsAndMetadata()throws Exception {
  Radio radio=new Radio();File folder=Files.createTempDirectory("scoped-install").toFile();File file=new File(folder,"expected.bin");
  ScopedBackup s=new ScopedBackup(new NdcpSession(radio),v->{},file);s.snapshot();
  assertEquals(0x9000+65536,radio.readBytes);assertEquals(0x8000000L,file.length());
  byte[][] images=new byte[9][];for(int k=0;k<9;k++)images[k]=new byte[SIZES[k]];
  s.begin(images);assertEquals(68,radio.plan.length);
  assertArrayEquals(NdcpSession.hex(RecoveryBundle.sha(radio.logical)),Arrays.copyOf(radio.plan,32));
  byte[] after;try(BootstrapFatPlan plan=new BootstrapFatPlan(file)){
   for(int k:ScopedBackup.ORDER)assertEquals(plan.allocate(k,SIZES[k]),ByteCodec.u32le(radio.plan,32+4*k));after=plan.metadata.clone();
  }
  for(int k=0;k<9;k++)assertEquals(SIZES[k],new File(folder,"extent-"+k+"-original.bin").length());
  radio.logical=after;try(RandomAccessFile f=new RandomAccessFile(file,"rw")){f.write(BootstrapFatPlan.swap(after));}
  s.finish();assertTrue(radio.readBytes<5*1024*1024);assertFalse(new File(folder,"A.bin").exists());
  assertArrayEquals(BootstrapFatPlan.swap(after),Files.readAllBytes(new File(folder,"metadata-final.bin").toPath()));
 }
 @Test public void offsetMismatchNeverCreatesWriteEvidence()throws Exception {
  Radio radio=new Radio();radio.wrongOffset=true;File dir=Files.createTempDirectory("scoped-bad").toFile();
  try{new ScopedBackup(new NdcpSession(radio),v->{},new File(dir,"expected.bin")).snapshot();fail();}catch(IOException expected){}
  assertNull(radio.plan);assertFalse(new File(dir,"scope-plan.bin").exists());
 }
 @Test public void freshPhotoSlotsAreEmptyRegardlessOfStockPictures(){
  byte[] image=BootstrapProvisioner.freshPhotos(new long[]{1,2,3});assertEquals(1048576,image.length);
  for(int i=4096;i<image.length;i++)assertEquals((byte)255,image[i]);
 }
 @Test public void failedPhotoInspectionReportsFileAndFinalDeviceOffset()throws Exception {
  List<Integer> commands=new ArrayList<>();
  InstallerTransport radio=new InstallerTransport(){byte[] pending;
   public void send(byte[] f)throws IOException{
    int op=f[5]&255;commands.add(op);byte[] r;
    if(op==0x56)r=NdcpSession.words(0,9,6,1048576,0,1048576,0,1);
    else if(op==0x84)r=NdcpSession.words(0,2,8,9,2,3,1048576,1048576,0x50006,42,10000,0,9,9,0,1);
    else throw new IOException("Unexpected command "+op);
    pending=NdcpSession.encode(op,(int)ByteCodec.u32le(f,8),r,1);
   }
   public byte[] receive(long t){return pending;}public void close(){}
  };
  InstallerPresentation p=new InstallerPresentation(()->1000);p.begin("guided-bootstrap");p.stage("file-3","reading",753664,1048576,"B");
  ScopedBackup s=new ScopedBackup(new NdcpSession(radio),p,new File(Files.createTempDirectory("photo-format").toFile(),"plan"));
  try{s.request(0x56,new byte[0]);fail();}catch(IOException expected){
   assertTrue(expected.getMessage().contains("00050006"));assertTrue(expected.getMessage().contains("CFWPIC.DAT"));
   assertTrue(expected.getMessage().contains("1048576/1048576"));p.finish(expected.getMessage(),true);
  }
  assertTrue(p.snapshot().counts.contains("1,048,576 / 1,048,576"));assertTrue(p.snapshot().error);assertEquals(-1,p.snapshot().percent);
  assertEquals(Arrays.asList(0x56,0x84),commands);
 }
 static byte[][] provisionFixture(Radio r,File file,int count)throws Exception {
  try(RandomAccessFile f=new RandomAccessFile(file,"rw")){f.setLength(0x8000000L);f.write(BootstrapFatPlan.swap(r.logical));}
  byte[][] images=new byte[9][];for(int k=0;k<9;k++){images[k]=new byte[SIZES[k]];Arrays.fill(images[k],(byte)(k+10));}
  try(BootstrapFatPlan fs=new BootstrapFatPlan(file)){
   for(int i=0;i<count;i++){int k=ScopedBackup.ORDER[i];long at=fs.allocate(k,SIZES[k]);r.extents.put(at,BootstrapFatPlan.swap(images[k]));}r.logical=fs.metadata.clone();
  }
  return images;
 }
 @Test public void fullyPreparedFilesAreAdoptedWithoutWriteCommands()throws Exception {
  Radio r=new Radio();File dir=Files.createTempDirectory("reuse-valid").toFile(),file=new File(dir,"expected.bin");
  byte[][] images=provisionFixture(r,file,9);ScopedBackup s=new ScopedBackup(new NdcpSession(r),v->{},file);
  s.snapshot();s.reuse(images);s.finish();assertEquals(9,r.inspections);
  assertEquals(72,r.plan.length);assertEquals(0x45535552L,ByteCodec.u32le(r.plan,68));
  assertTrue(new File(dir,"REUSE.txt").exists());assertEquals(0x9000,new File(dir,"metadata-final.bin").length());
 }
 @Test public void differentProductCannotBeAdoptedOrOverwritten()throws Exception {
  Radio r=new Radio();File dir=Files.createTempDirectory("reuse-different").toFile(),file=new File(dir,"expected.bin");
  byte[][] images=provisionFixture(r,file,9);images[5][512]^=1;
  ScopedBackup s=new ScopedBackup(new NdcpSession(r),v->{},file);s.snapshot();
  try{s.reuse(images);fail();}catch(IOException expected){assertTrue(expected.getMessage().contains("CFWA.DAT"));}
  assertFalse(new File(dir,"metadata-final.bin").exists());assertTrue(r.inspections<9);
 }
 @Test public void partialPublicationStopsBeforeAnyScopeOrWrite()throws Exception {
  Radio r=new Radio();File dir=Files.createTempDirectory("reuse-partial").toFile(),file=new File(dir,"expected.bin");
  byte[][] images=provisionFixture(r,file,4);ScopedBackup s=new ScopedBackup(new NdcpSession(r),v->{},file);s.snapshot();
  try{s.reuse(images);fail();}catch(IOException expected){assertTrue(expected.getMessage().contains("일부"));}
  assertNull(r.plan);assertEquals(0,r.inspections);
 }
}
