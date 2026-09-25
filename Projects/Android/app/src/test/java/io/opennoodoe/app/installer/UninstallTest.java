package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import org.junit.Test;
import static org.junit.Assert.*;
import java.io.*;
import java.util.*;
public final class UninstallTest {
 static byte[] image(){byte[] b=new byte[0x70000];Arrays.fill(b,(byte)255);
  long[] v={0x31424e55L,1,0x08010000,0x70000,0x08011000,0xf000,0x00100005,0};
  ByteCodec.putU32le(b,0,0x2002ff00);ByteCodec.putU32le(b,4,0x08020101);
  for(int i=0;i<v.length;i++)ByteCodec.putU32le(b,0x200+4*i,v[i]);return b;}
 static byte[] caps(byte[] b){byte[] r=Arrays.copyOf(NdcpSession.words(0,1,3,1,2,3,b.length,7),64);
  System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(b)),0,r,32,32);return r;}
 @Test public void releasePinAndUidRequired()throws Exception{
  byte[] b=image(),r=caps(b);UninstallSession.capability(r,b,"1,2,3");
  for(int off:new int[]{4,8,12,24,28,32}){byte[] bad=r.clone();bad[off]^=2;
   try{UninstallSession.capability(bad,b,"1,2,3");fail("offset "+off);}catch(IOException expected){}}
 }
 @Test public void journalMustStartErasedAndCodeCannotLiveInsideIt()throws Exception{
  byte[] b=image();RecoveryBundle.validateUninstall(b);
  b[0x1800]=0;try{RecoveryBundle.validateUninstall(b);fail();}catch(IOException expected){}
  b=image();ByteCodec.putU32le(b,4,0x08011101);try{RecoveryBundle.validateUninstall(b);fail();}catch(IOException expected){}
 }
 @Test public void retainedPolicyIsOnlyInInitialJournalAndCrcCoversIt(){
  byte[] app=new byte[0x60000];long[] uid={1,2,3};byte[] fresh=GateContainers.journal(app,uid),restored=GateContainers.journal(app,uid,true);
  assertEquals(0,ByteCodec.u32le(fresh,176));assertEquals(0x52535452L,ByteCodec.u32le(restored,176));
  assertEquals(0x52535452L,ByteCodec.u32le(restored,180));assertEquals(NdcpSession.crc(restored,4088),ByteCodec.u32le(restored,4088));
  for(int i=0;i<fresh.length;i++)if(i<176||i>=184&&i<4088||i>=4092)assertEquals("at "+i,fresh[i],restored[i]);
 }
 @Test public void uninstallJourneyNeverClaimsCompletionDuringOfflineCleanup(){
  InstallJourney j=new InstallJourney();j.begin("uninstall-stock");j.stage("stage");assertEquals(25,j.percent());
  j.stage("unconfirmed");assertEquals(50,j.percent());j.stage("stage");assertEquals(50,j.percent());
 }

 static RecoveryBundle bundle()throws Exception{
  RecoveryBundle base=new InstallerTest().valid();Properties m=new Properties();m.putAll(base.manifest);
  Map<String,byte[]> files=new LinkedHashMap<>();for(String role:new String[]{"bootstrap","cfw","stock","resources"})files.put(m.getProperty(role+".file"),base.image(role));
  byte[] b=image();files.put("uninstall.bin",b);m.setProperty("uninstall.file","uninstall.bin");m.setProperty("uninstall.sha256",RecoveryBundle.sha(b));m.setProperty("uninstall.version","7");
  StringBuilder manifest=new StringBuilder();for(String key:m.stringPropertyNames())manifest.append(key).append('=').append(m.getProperty(key)).append('\n');
  files.put("manifest.properties",manifest.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8));ByteArrayOutputStream out=new ByteArrayOutputStream();
  try(java.util.zip.ZipOutputStream z=new java.util.zip.ZipOutputStream(out)){for(Map.Entry<String,byte[]> e:files.entrySet()){z.putNextEntry(new java.util.zip.ZipEntry(e.getKey()));z.write(e.getValue());z.closeEntry();}}
  return RecoveryBundle.read(new ByteArrayInputStream(out.toByteArray()));
 }
 static final class Radio implements InstallerTransport {
  final RecoveryBundle bundle;final List<Integer> ops=new ArrayList<>();byte[] reply;long tx,state,offset;boolean loseCommit,unsupported;int resets,commits;
  Radio(RecoveryBundle b){bundle=b;}
  public void send(byte[] f)throws IOException{
   int op=f[5]&255,seq=(int)ByteCodec.u32le(f,8),flags=1;byte[] p=Arrays.copyOfRange(f,16,f.length-4),r;
   ops.add(op);try{switch(op){
    case 0x58:r=Arrays.copyOf(NdcpSession.words(0,1,2,1,2,3),88);break;
    case 0x64:r=Arrays.copyOf(NdcpSession.words(0,2),40);System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(Arrays.copyOf(bundle.paddedImage("cfw"),65536))),0,r,8,32);break;
    case 0x93:r=unsupported?NdcpSession.words(2):caps(image());if(unsupported)flags=3;break;
    case 0x45:r=Arrays.copyOf(NdcpSession.words(0,state,tx,offset,offset),84);System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(image())),0,r,52,32);break;
    case 0x5b:r=Arrays.copyOf(NdcpSession.words(0,2,1,4,0,0,0xffffffffL,0,0,0,0,0),80);break;
    case 0x62:r=NdcpSession.words(0);break;
    case 0x63:r=NdcpSession.words(0,3,0);break;
    case 0x5d:r=NdcpSession.words(2);flags=3;break;
    case 0x86:r=NdcpSession.words(0,2,512,4096,0);break;
    case 0x40:assertEquals(3,ByteCodec.u32le(p,52));assertEquals(7,ByteCodec.u32le(p,4));assertEquals(0x70000,ByteCodec.u32le(p,8));tx=ByteCodec.u32le(p,0);state=1;r=NdcpSession.words(0,state,tx,offset,0);break;
    case 0x41:assertEquals(offset,ByteCodec.u32le(p,4));assertArrayEquals(Arrays.copyOfRange(image(),(int)offset,(int)offset+p.length-8),Arrays.copyOfRange(p,8,p.length));offset+=p.length-8;r=NdcpSession.words(0,state,tx,offset,0);break;
    case 0x42:state=3;r=Arrays.copyOf(NdcpSession.words(0,state,tx,offset,offset),52);System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(image())),0,r,20,32);break;
    case 0x43:commits++;state=4;if(loseCommit)throw new IOException("lost commit");r=NdcpSession.words(0,state,tx,offset,offset);break;
    case 0x47:resets++;state=5;r=NdcpSession.words(0);break;
    default:throw new IOException("unexpected opcode "+op);
   }}catch(IOException e){throw e;}catch(Exception e){throw new IOException(e);}
   reply=NdcpSession.encode(op,seq,r,flags);
  }
  public byte[] receive(long t)throws IOException{if(reply==null)throw new IOException("lost reply");byte[] r=reply;reply=null;return r;}public void close(){}
 }
 @Test public void fullUploadThenLostCommitOnlyQueriesBeforeSingleReset()throws Exception{
  RecoveryBundle b=bundle();Radio r=new Radio(b);r.loseCommit=true;
  InstallJournal j=new InstallJournal(new File(java.nio.file.Files.createTempDirectory("uninstall").toFile(),"work.journal"));
  try{new UninstallSession(new NdcpSession(r)).run(b,j,m->{});fail();}catch(IOException expected){}
  assertEquals("UNINSTALL_COMMIT_RESULT_UNKNOWN",j.state());assertEquals(1,r.commits);assertEquals(0,r.resets);assertEquals(896,Collections.frequency(r.ops,0x41));
  r.ops.clear();new UninstallSession(new NdcpSession(r)).reconcile(b,j);
  assertEquals("UNINSTALL_COMMITTED",j.state());assertEquals(0,r.resets);assertFalse(r.ops.contains(0x43));assertFalse(r.ops.contains(0x47));
  r.ops.clear();r.loseCommit=false;new UninstallSession(new NdcpSession(r)).run(b,j,m->{});
  assertEquals(1,r.commits);assertEquals(1,r.resets);assertFalse(r.ops.contains(0x40));assertFalse(r.ops.contains(0x43));assertEquals("UNINSTALL_DEVICE_RUNNING",j.state());
  new UninstallSession(new NdcpSession(r)).run(b,j,m->{});assertEquals(1,r.resets);
 }
 @Test public void oldDeviceCannotSilentlyFallBackToKeep()throws Exception{
  RecoveryBundle b=bundle();Radio r=new Radio(b);r.unsupported=true;
  InstallJournal j=new InstallJournal(new File(java.nio.file.Files.createTempDirectory("uninstall-old").toFile(),"work.journal"));
  try{new UninstallSession(new NdcpSession(r)).run(b,j,m->{});fail();}catch(IOException expected){}
  assertFalse(r.ops.contains(0x40));assertFalse(r.ops.contains(0x48));
 }
}
