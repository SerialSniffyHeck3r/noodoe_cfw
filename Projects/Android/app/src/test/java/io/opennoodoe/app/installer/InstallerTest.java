package io.opennoodoe.app.installer;

import io.opennoodoe.app.protocol.*;
import org.junit.Test;
import static org.junit.Assert.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;

public final class InstallerTest {
    /** Models only the read-only export. No real BL or radio is emulated. */
    private static final class CaptureRadio implements InstallerTransport {
        final byte[] lower=new byte[65536];byte[] pending;int reads,busyReplies,reject;boolean inconsistent;
        CaptureRadio(){ByteCodec.putU32le(lower,0x8000,0xf0000);}
        public void send(byte[] frame)throws IOException {
            if((frame[5]&255)!=0x60)throw new IOException("Export test attempted a write");
            if(busyReplies>0||reject!=0){int error=reject!=0?reject:3;if(busyReplies>0)busyReplies--;
                pending=NdcpSession.encode(0x60,(int)ByteCodec.u32le(frame,8),NdcpSession.words(error),3);return;}
            int off=(int)ByteCodec.u32le(frame,16),n=(int)ByteCodec.u32le(frame,20);
            byte[] r=Arrays.copyOf(NdcpSession.words(0,1,off,n,0xf0000,4,6,off==0?1:0),32+n);
            System.arraycopy(lower,off,r,32,n);reads++;
            if(inconsistent&&reads>137&&off==0)r[32]^=1;
            pending=NdcpSession.encode(0x60,(int)ByteCodec.u32le(frame,8),r,1);
        }
        public byte[] receive(long t){byte[] r=pending;pending=null;return r;}public void close(){}
        byte[] identity(){byte[] r=Arrays.copyOf(NdcpSession.words(0,1,1,1,2,3),88);
            System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(Arrays.copyOf(lower,32768))),0,r,56,32);return r;}
    }
    private RecoveryBundle captureBundle()throws Exception {
        RecoveryBundle b=valid();b.manifest.setProperty("target.boot.binding","device-capture");
        b.manifest.setProperty("target.boot.major","0");b.manifest.setProperty("target.boot.minor","15");
        b.manifest.setProperty("target.boot.sha256","capture-on-bootstrap");return b;
    }
    @Test public void captureTwiceThenReuseAcrossPackagesWithoutWrites()throws Exception {
        CaptureRadio radio=new CaptureRadio();RecoveryBundle b=captureBundle();
        File dir=Files.createTempDirectory("target-binding").toFile();InstallJournal j=new InstallJournal(new File(dir,"package-a.journal"));
        TargetBinding.verify(new NdcpSession(radio),b,j,radio.identity(),1);assertEquals(274,radio.reads);
        assertEquals(0xf0000,TargetBinding.version(j));assertEquals(65536,new File(j.values.getProperty("resident.backup")).length());
        InstallJournal next=new InstallJournal(new File(dir,"package-b.journal"));
        TargetBinding.verify(new NdcpSession(radio),b,next,radio.identity(),2);assertEquals(274,radio.reads);
        byte[] changed=radio.identity();changed[56]^=1;
        try{TargetBinding.verify(new NdcpSession(radio),b,next,changed,2);fail();}catch(IOException expected){}
        changed=radio.identity();ByteCodec.putU32le(changed,12,9);
        try{TargetBinding.verify(new NdcpSession(radio),b,next,changed,1);fail();}catch(IOException expected){}
    }
    @Test public void inconsistentOriginalAndMissingProductBackupFailClosed()throws Exception {
        CaptureRadio radio=new CaptureRadio();radio.inconsistent=true;RecoveryBundle b=captureBundle();InstallJournal j=journal();
        try{TargetBinding.verify(new NdcpSession(radio),b,j,radio.identity(),1);fail();}catch(IOException expected){}
        assertFalse(j.values.containsKey("resident.backup"));
        radio.reads=0;
        try{TargetBinding.verify(new NdcpSession(radio),b,j,radio.identity(),2);fail();}catch(IOException expected){}
        assertEquals(0,radio.reads);
    }
    @Test public void originalCaptureWaitsForExplicitStartupBusyOnly()throws Exception {
        CaptureRadio radio=new CaptureRadio();radio.busyReplies=2;InstallJournal j=journal();
        TargetBinding.verify(new NdcpSession(radio),captureBundle(),j,radio.identity(),1);
        assertEquals(274,radio.reads);assertEquals(0,radio.busyReplies);
        CaptureRadio denied=new CaptureRadio();denied.reject=2;InstallJournal k=journal();
        try{TargetBinding.verify(new NdcpSession(denied),captureBundle(),k,denied.identity(),1);fail();}
        catch(NdcpSession.DeviceRejected expected){assertEquals(2,expected.result);}
        assertEquals(0,denied.reads);assertFalse(k.values.containsKey("resident.backup"));
    }
    private byte[] image(int size) {
        byte[] bytes=new byte[size];ByteCodec.putU32le(bytes,0,0x20020000);ByteCodec.putU32le(bytes,4,0x08010101);return bytes;
    }
    private byte[] bundle(String extra,byte[] bootstrap)throws Exception {
        String manifest="format=NOODOE_INSTALLER_2\nlayout.version=2\napp.base=0x08010000\napp.bytes=0x70000\n"
                +"target.hardware=1\ntarget.boot.major=1\ntarget.boot.minor=0\ntarget.stock.major=5\ntarget.stock.minor=16\ntarget.model=AK550\ntarget.pcba=sr0601\ntarget.scope=observed\ntarget.deployment=no-swd-validated\ntarget.boot.sha256="+"00".repeat(32)+"\n";
        Map<String,byte[]> files=new LinkedHashMap<>();
        for(String role:new String[]{"bootstrap","cfw","stock"}) {
            byte[] data=role.equals("bootstrap")?bootstrap:role.equals("cfw")?combined():image(512);files.put(role+".bin",data);
            manifest+=role+".file="+role+".bin\n"+role+".sha256="+RecoveryBundle.sha(data)+"\n"+role+".major=5\n"+role+".minor="+(role.equals("stock")?16:17)+"\n";
        }
        byte[] resources=new byte[0x100000];files.put("resources.bin",resources);manifest+="resources.file=resources.bin\nresources.sha256="+RecoveryBundle.sha(resources)+"\n";
        files.put("manifest.properties",(manifest+extra).getBytes(java.nio.charset.StandardCharsets.UTF_8));
        ByteArrayOutputStream result=new ByteArrayOutputStream();try(ZipOutputStream zip=new ZipOutputStream(result)) {
            for(Map.Entry<String,byte[]> entry:files.entrySet()){zip.putNextEntry(new ZipEntry(entry.getKey()));zip.write(entry.getValue());zip.closeEntry();}
        }return result.toByteArray();
    }
    private byte[] combined(){byte[] b=new byte[0x70000];Arrays.fill(b,(byte)0xff);
        ByteCodec.putU32le(b,0,0x2002ff00);ByteCodec.putU32le(b,4,0x08010101);
        ByteCodec.putU32le(b,0x10000,0x2002ff00);ByteCodec.putU32le(b,0x10004,0x08020101);
        ByteCodec.putU32le(b,0x10200,0x51534352);ByteCodec.putU32le(b,0x10204,1);ByteCodec.putU32le(b,0x10208,1);ByteCodec.putU32le(b,0x10230,0x3250554e);ByteCodec.putU32le(b,0x10234,2);ByteCodec.putU32le(b,0x10238,2);ByteCodec.putU32le(b,0x1023c,0x60000);return b;}
    byte[] validArchive()throws Exception{return bundle("",image(512));}
    RecoveryBundle valid()throws Exception{return RecoveryBundle.read(new ByteArrayInputStream(validArchive()));}
    @Test public void observedIdentityIsNotDeploymentQualification()throws Exception {
        RecoveryBundle b=valid();b.manifest.remove("target.deployment");
        try{b.requireInstallable();fail();}catch(IOException expected){}
        b.manifest.setProperty("target.deployment","blocked-pending-hardware-validation");
        try{b.requireInstallable();fail();}catch(IOException expected){}
    }
    @Test public void strictBundleAndDeterministicPadding()throws Exception {
        RecoveryBundle bundle=valid();byte[] padded=bundle.paddedImage("bootstrap");assertEquals(0x70000,padded.length);
        assertEquals(255,padded[512]&255);assertEquals(64,bundle.sha256.length());
    }
    @Test public void fullDumpAndBadVectorsRejected()throws Exception {
        for(byte[] invalid:new byte[][]{image(0x80000),image(128),new byte[512]}) {
            try{RecoveryBundle.read(new ByteArrayInputStream(bundle("",invalid)));fail();}catch(IOException expected){}
        }
        byte[] wrong=image(512);ByteCodec.putU32le(wrong,4,0x08000101);
        try{RecoveryBundle.validateApp(wrong);fail();}catch(IOException expected){}
    }
    @Test public void duplicateManifestAndEscapesRejected()throws Exception {
        for(String extra:new String[]{"format=NOODOE_RECOVERY_1\n","unknown=escape\\value\n"})
            try{RecoveryBundle.read(new ByteArrayInputStream(bundle(extra,image(512))));fail();}catch(IOException expected){}
    }
    @Test public void journalPersistsUnknownAndBindsIdentity()throws Exception {
        File f=new File(Files.createTempDirectory("installer").toFile(),"journal");
        InstallJournal j=new InstallJournal(f);j.bind("AA:BB",valid().sha256);j.save("NDCP_COMMIT_RESULT_UNKNOWN");
        InstallJournal reloaded=new InstallJournal(f);assertEquals("NDCP_COMMIT_RESULT_UNKNOWN",reloaded.state());
        try{reloaded.bind("different",valid().sha256);fail();}catch(IOException expected){}
        try{reloaded.require("NDCP_VERIFIED");fail();}catch(IOException expected){}
    }
    private static final class StockFake implements InstallerTransport {
        byte[] pending; int responseIndex=128; final List<Integer> commands=new ArrayList<>();
        int phase, total; boolean failTerminate, implicitAck;
        @Override public void send(byte[] bytes)throws IOException {
            if(bytes.length==5&&bytes[0]==5){pending=new byte[26];pending[0]=(byte)0x85;ByteCodec.putU32le(pending,1,21);pending[6]=5;pending[8]=16;pending[11]=1;return;}
            SequenceFrame frame=SequenceFrame.decode(bytes);if(frame.getPayload().length==0)return;
            CommandFrame c=CommandFrame.decodeMany(frame.getPayload()).get(0);int id=c.getCommandId();commands.add(id);byte[] p=c.getPayload(),r;
            if(id==5){r=new byte[82];r[2]=5;r[4]=16;r[7]=1;r[14]=1;System.arraycopy("AK550".getBytes(),0,r,40,5);System.arraycopy("sr0601".getBytes(),0,r,75,6);}
            else if(id==12){r=new byte[11];r[2]=1;}
            else {
                r=new byte[id==13?16:6];r[2]=p[0];r[3]=p[1];r[4]=1;
                if(id==10){phase=p[10];if(phase==1)total=0;}
                if(id==13){total+=p.length-6;ByteCodec.putU32le(r,12,total);}
                if(id==11&&p[2]==2&&failTerminate){pending=null;return;}
            }
            pending=new SequenceFrame(implicitAck?0:0x40,responseIndex++,frame.getPacketIndex(),0,new CommandFrame(id,8,r).encode()).encode();
        }
        @Override public byte[] receive(long timeout)throws IOException {if(pending==null)throw new IOException("dropped ACK");byte[] p=pending;pending=null;return p;}
        @Override public void close(){}
    }
    @Test public void stockFullTransferEndsAtIgnHandoffWithoutAutosync()throws Exception {
        StockFake fake=new StockFake();InstallJournal j=new InstallJournal(new File(Files.createTempDirectory("stock").toFile(),"journal"));j.save("IMPORTED");
        new StockUpdateSession(fake).installBootstrap(valid(),j,text->{});
        assertEquals("STOCK_ACCEPTED_WAIT_IGN_OFF",j.state());assertEquals(3,fake.phase);assertEquals(0x70000,fake.total);
        for(int command:fake.commands)assertTrue(Arrays.asList(5,12,10,11,13).contains(command));
    }
    @Test public void failedTerminateNeverSendsDone()throws Exception {
        StockFake fake=new StockFake();fake.failTerminate=true;InstallJournal j=new InstallJournal(new File(Files.createTempDirectory("stockfail").toFile(),"journal"));j.save("IMPORTED");
        try{new StockUpdateSession(fake).installBootstrap(valid(),j,text->{});fail();}catch(IOException expected){}
        assertEquals("STOCK_TERMINATE_RESULT_UNKNOWN",j.state());assertEquals(1,fake.phase);
    }
    @Test public void piggybackedAckWithoutFlagAccepted()throws Exception {
        StockFake fake=new StockFake();fake.implicitAck=true;assertEquals(5,new StockUpdateSession(fake).identify().firmwareMajor);
    }
    @Test public void readOnlyIdentityDoesNotRequireBundleOrSendWrites()throws Exception {
        StockFake fake=new StockFake();File dir=Files.createTempDirectory("stock-identify-only").toFile();
        String result=new InstallerController(dir).run("stock-read-info","aa","",()->fake,text->{},data->{});
        assertTrue(result.contains("FW 5.16"));assertEquals(Arrays.asList(5),fake.commands);
        File[] evidence=dir.listFiles(f->f.getName().startsWith("stock-identity-"));assertNotNull(evidence);assertEquals(1,evidence.length);
        assertTrue(new File(dir,"logs").isDirectory());
        assertEquals(82,new File(evidence[0],"reply.bin").length());assertTrue(new File(evidence[0],"identity.json").isFile());
    }
    @Test public void exactPcbaAndBenchScopePreventInstallation()throws Exception {
        RecoveryBundle b=valid();byte[] body=new byte[82];body[2]=5;body[4]=16;body[7]=1;body[14]=1;
        System.arraycopy("AK550".getBytes(),0,body,40,5);System.arraycopy("sr0601".getBytes(),0,body,75,6);
        b.checkStock(DeviceInfo.fromFramedReply(body));body[75]='S';
        try{b.checkStock(DeviceInfo.fromFramedReply(body));fail();}catch(IOException expected){}
        b.manifest.setProperty("target.scope","bench-only");
        try{b.requireInstallable();fail();}catch(IOException expected){assertTrue(expected.getMessage().contains("Bench-only"));}
    }
    @Test public void ndcpGoldenFrame() {
        byte[] frame=NdcpSession.encode(0x45,9,new byte[0],0);
        assertEquals("4e444350014500000900000000000000a85267b5",ByteCodec.hex(frame).replace(" ","").toLowerCase(Locale.ROOT));
    }
    @Test public void unknownCommitIsDurableAndCannotBeResent()throws Exception {
        String hash=RecoveryBundle.sha(image(512));InstallJournal j=new InstallJournal(new File(Files.createTempDirectory("commit").toFile(),"journal"));
        j.values.setProperty("transaction","7");j.values.setProperty("image.sha256",hash);j.save("NDCP_VERIFIED");
        InstallerTransport fake=new InstallerTransport(){byte[] pending;
            public void send(byte[] bytes){if((bytes[5]&255)==0x45){byte[] r=new byte[84];ByteCodec.putU32le(r,4,3);ByteCodec.putU32le(r,8,7);System.arraycopy(NdcpSession.hex(hash),0,r,52,32);pending=NdcpSession.encode(0x45,(int)ByteCodec.u32le(bytes,8),r,1);}else pending=null;}
            public byte[] receive(long t)throws IOException {if(pending==null)throw new IOException("lost COMMIT ACK");byte[] r=pending;pending=null;return r;}
            public void close(){}
        };
        try{new NdcpSession(fake).commit(j);fail();}catch(IOException expected){}
        assertEquals("NDCP_COMMIT_RESULT_UNKNOWN",j.state());
        try{new NdcpSession(fake).commit(j);fail();}catch(IOException expected){assertTrue(expected.getMessage().contains("blocked"));}
    }
    private File emptyNor()throws Exception {
        File file=File.createTempFile("noodoe-nor-", ".bin");file.deleteOnExit();byte[] metadata=new byte[0x9000];
        ByteCodec.putU16le(metadata,11,4096);metadata[13]=8;ByteCodec.putU16le(metadata,14,1);metadata[16]=2;
        ByteCodec.putU16le(metadata,17,512);ByteCodec.putU16le(metadata,22,2);ByteCodec.putU32le(metadata,32,0x7f80);
        metadata[510]=0x55;metadata[511]=(byte)0xaa;metadata[21]=(byte)0xf8;
        for(int base:new int[]{0x1000,0x3000}){metadata[base]=(byte)0xf8;metadata[base+1]=(byte)0xff;metadata[base+2]=(byte)0xff;}
        try(RandomAccessFile out=new RandomAccessFile(file,"rw")){out.setLength(0x8000000L);out.write(BootstrapFatPlan.swap(metadata));}return file;
    }
    @Test public void independentFatAllocationAndRecordCrc()throws Exception {
        File nor=emptyNor();try(BootstrapFatPlan plan=new BootstrapFatPlan(nor)) {
            assertEquals(0x9000,plan.allocate(4,524288));
            assertEquals("CFWREC  DAT",new String(plan.metadata,0x5000,11,java.nio.charset.StandardCharsets.US_ASCII));
            assertArrayEquals(Arrays.copyOfRange(plan.metadata,0x1000,0x3000),Arrays.copyOfRange(plan.metadata,0x3000,0x5000));
            assertEquals(2,ByteCodec.u16le(plan.metadata,0x5000+26));
        }
        byte[] record=BootstrapFatPlan.record(1,new long[]{1,2,3},new byte[]{9,8,7},4);
        assertEquals(0x314a4643L,ByteCodec.u32le(record,0));assertEquals(4,ByteCodec.u32le(record,12));
        assertEquals(NdcpSession.crc(record,4088),ByteCodec.u32le(record,4088));assertEquals(0x31544d43L,ByteCodec.u32le(record,4092));
    }
    @Test public void fatMirrorCorruptionAndOrphanRejected()throws Exception {
        for(boolean mirror:new boolean[]{true,false}) {
            File nor=emptyNor();try(RandomAccessFile file=new RandomAccessFile(nor,"rw")) {
                byte[] bytes=new byte[0x9000];file.readFully(bytes);bytes=BootstrapFatPlan.swap(bytes);
                bytes[0x1000+3]=(byte)0xff;bytes[0x1000+4]=15;
                if(!mirror){bytes[0x3000+3]=(byte)0xff;bytes[0x3000+4]=15;}
                file.seek(0);file.write(BootstrapFatPlan.swap(bytes));
            }
            try(BootstrapFatPlan ignored=new BootstrapFatPlan(nor)){fail();}catch(IOException expected){}
        }
    }
    @Test public void identityMustMatchActualImageBlAndUid()throws Exception {
        RecoveryBundle bundle=valid();InstallJournal journal=new InstallJournal(new File(Files.createTempDirectory("identity").toFile(),"j"));
        InstallerTransport fake=new InstallerTransport(){byte[] pending;int uid=1;
            public void send(byte[] request){byte[] r=new byte[88];ByteCodec.putU32le(r,4,1);ByteCodec.putU32le(r,8,1);ByteCodec.putU32le(r,12,uid++);
                try{System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(bundle.paddedImage("bootstrap"))),0,r,24,32);}catch(Exception e){throw new RuntimeException(e);}
                pending=NdcpSession.encode(0x58,(int)ByteCodec.u32le(request,8),r,1);}
            public byte[] receive(long ms){byte[] out=pending;pending=null;return out;}public void close(){}
        };
        NdcpSession session=new NdcpSession(fake);assertEquals(1,session.identity(bundle,journal,1)[0]);
        try{session.identity(bundle,journal,1);fail();}catch(IOException expected){assertTrue(expected.getMessage().contains("UID"));}
    }
    /** In-memory radio fixture, never a claim about the controller or NOR. */
    private final class ProductRadio implements InstallerTransport {
        final RecoveryBundle bundle; final List<Integer> commands=new ArrayList<>();
        byte[] pending,digest;long flags,updateState,offset=4096,tx=42,bootState=4;boolean loseCommit,wrongGate,sameImage;
        ProductRadio(RecoveryBundle b)throws Exception{bundle=b;digest=NdcpSession.hex(RecoveryBundle.sha(GateContainers.product(b.paddedImage("cfw"))));}
        public void send(byte[] frame)throws IOException {
            int op=frame[5]&255,sequence=(int)ByteCodec.u32le(frame,8);commands.add(op);byte[] p=Arrays.copyOfRange(frame,16,frame.length-4),r;
            try{
                switch(op){
                case 0x58:r=NdcpSession.words(0,1,2,1,2,3);r=Arrays.copyOf(r,88);break;
                case 0x64:r=Arrays.copyOf(NdcpSession.words(0,2),40);System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(Arrays.copyOf(bundle.paddedImage("cfw"),0x10000))),0,r,8,32);if(wrongGate)r[8]^=1;break;
                case 0x5b:r=Arrays.copyOf(NdcpSession.words(0,2,7,bootState,flags,0,1,9,8,3,tx,0),80);if(sameImage)System.arraycopy(digest,0,r,48,32);break;
                case 0x61:assertEquals(tx,ByteCodec.u32le(p,0));flags=0;r=NdcpSession.words(0);break;
                case 0x45:r=Arrays.copyOf(NdcpSession.words(0,updateState,tx,0x60000,offset),84);System.arraycopy(digest,0,r,52,32);break;
                case 0x62:r=NdcpSession.words(0);break;
                case 0x63:r=NdcpSession.words(0,3,0);break;
                case 0x40:assertEquals(0x60000,ByteCodec.u32le(p,8));assertEquals(2,ByteCodec.u32le(p,52));tx=ByteCodec.u32le(p,0);updateState=1;r=NdcpSession.words(0,1,tx,offset,0);break;
                case 0x86:r=NdcpSession.words(0,2,512,4096,0);break;
                case 0x41:assertEquals(offset,ByteCodec.u32le(p,4));assertEquals(520,p.length);offset+=512;r=NdcpSession.words(0,1,tx,offset,0);break;
                case 0x42:updateState=3;r=Arrays.copyOf(NdcpSession.words(0,3,tx,0x60000,offset),52);System.arraycopy(digest,0,r,20,32);break;
                case 0x43:updateState=4;if(loseCommit)throw new IOException("lost COMMIT ACK");r=NdcpSession.words(0,4,tx,offset,0);break;
                case 0x47:updateState=5;r=NdcpSession.words(0);break;
                default:throw new IOException("Unexpected command "+op);
                }
            }catch(IOException e){throw e;}catch(Exception e){throw new IOException(e);}
            pending=NdcpSession.encode(op,sequence,r,1);
        }
        public byte[] receive(long ms)throws IOException{if(pending==null)throw new IOException("no reply");byte[] r=pending;pending=null;return r;}
        public void close(){}
    }
    private InstallJournal journal()throws Exception{return new InstallJournal(new File(Files.createTempDirectory("product-test").toFile(),"journal"));}
    @Test public void stockRestoreRequiresIdleAndFreshRecoveryAudit()throws Exception {
        RecoveryBundle b=valid();ProductRadio radio=new ProductRadio(b);InstallJournal j=journal();
        new RoutineUpdateSession(new NdcpSession(radio)).prepareMaintenance(b,j,m->{});
        assertTrue(radio.commands.contains(0x62));assertTrue(radio.commands.contains(0x63));assertFalse(radio.commands.contains(0x48));
        radio.commands.clear();radio.updateState=1;
        try{new RoutineUpdateSession(new NdcpSession(radio)).prepareMaintenance(b,j,m->{});fail();}catch(IOException expected){}
        assertFalse(radio.commands.contains(0x62));
    }
    @Test public void rollbackAcknowledgementDoesNotInstallOrReset()throws Exception {
        RecoveryBundle b=valid();ProductRadio radio=new ProductRadio(b);radio.flags=4;InstallJournal j=journal();
        new ProductBootSession(new NdcpSession(radio)).acknowledge(b,j);
        assertEquals("ROLLBACK_ACKNOWLEDGED",j.state());
        assertEquals(1,Collections.frequency(radio.commands,0x61));assertFalse(radio.commands.contains(0x43));assertFalse(radio.commands.contains(0x47));
    }
    @Test public void routineResumesAtSectorAndQueriesLostCommitBeforeReset()throws Exception {
        RecoveryBundle b=valid();ProductRadio radio=new ProductRadio(b);radio.loseCommit=true;InstallJournal j=journal();
        try{new RoutineUpdateSession(new NdcpSession(radio)).update(b,j,m->{});fail();}catch(IOException expected){}
        assertEquals("PRODUCT_COMMIT_RESULT_UNKNOWN",j.state());assertEquals(760,Collections.frequency(radio.commands,0x41));assertFalse(radio.commands.contains(0x47));
        radio.commands.clear();radio.loseCommit=false;
        assertTrue(new RoutineUpdateSession(new NdcpSession(radio)).update(b,j,m->{}));
        assertEquals(1,Collections.frequency(radio.commands,0x45));assertEquals(1,Collections.frequency(radio.commands,0x47));
        assertFalse(radio.commands.contains(0x40));assertFalse(radio.commands.contains(0x43));
    }
    @Test public void freshRoutineUpdateUploadsOnlyProductAndCommitsOnce()throws Exception {
        RecoveryBundle b=valid();ProductRadio radio=new ProductRadio(b);radio.offset=0;InstallJournal j=journal();
        assertTrue(new RoutineUpdateSession(new NdcpSession(radio)).update(b,j,m->{}));
        assertEquals(768,Collections.frequency(radio.commands,0x41));assertEquals(1,Collections.frequency(radio.commands,0x43));
        assertEquals(1,Collections.frequency(radio.commands,0x47));assertEquals("WAIT_CFW_BOOT",j.state());
        assertFalse(radio.commands.contains(0x52)); // no FAT allocation or Gate write
    }
    @Test public void unconfirmedCurrentBootAndDifferentGateCannotStartUpdate()throws Exception {
        RecoveryBundle b=valid();
        for(int failure=0;failure<2;failure++){
            ProductRadio radio=new ProductRadio(b);if(failure==0)radio.bootState=3;else radio.wrongGate=true;
            try{new RoutineUpdateSession(new NdcpSession(radio)).update(b,journal(),m->{});fail();}catch(IOException expected){}
            assertFalse(radio.commands.contains(0x62));assertFalse(radio.commands.contains(0x40));assertFalse(radio.commands.contains(0x43));
        }
    }
    @Test public void alreadyRunningCandidateIsConfirmedWithoutReupload()throws Exception {
        RecoveryBundle b=valid();ProductRadio radio=new ProductRadio(b);radio.sameImage=true;radio.bootState=3;
        assertFalse(new RoutineUpdateSession(new NdcpSession(radio)).update(b,journal(),m->{}));
        assertFalse(radio.commands.contains(0x40));assertFalse(radio.commands.contains(0x47));
    }
    @Test public void unrelatedRemoteTransactionCannotBeOverwritten()throws Exception {
        RecoveryBundle b=valid();
        for(int state:new int[]{1,2,3,4,5}){
            ProductRadio radio=new ProductRadio(b);radio.updateState=state;
            try{new RoutineUpdateSession(new NdcpSession(radio)).update(b,journal(),m->{});fail();}catch(IOException expected){}
            assertFalse(radio.commands.contains(0x40));assertFalse(radio.commands.contains(0x43));assertFalse(radio.commands.contains(0x47));
        }
    }
    @Test public void healthyOlderProductCanBeInspectedThenUpdatedUsingNewBundle()throws Exception {
        RecoveryBundle b=valid();ProductRadio radio=new ProductRadio(b);radio.offset=0;InstallJournal j=journal();
        String message=new ProductBootSession(new NdcpSession(radio)).inspectCurrent(b,j,m->{});
        assertTrue(message.contains("새 펌웨어는 아직 설치하지"));
        assertEquals("CURRENT_CFW_CONFIRMED",j.state());
        for(int op:new int[]{0x40,0x43,0x47,0x5c,0x62})assertFalse(radio.commands.contains(op));
        radio.commands.clear();
        assertTrue(new RoutineUpdateSession(new NdcpSession(radio)).update(b,j,m->{}));
        assertEquals(768,Collections.frequency(radio.commands,0x41));
        assertEquals(1,Collections.frequency(radio.commands,0x43));
    }
    @Test public void differentGateDoesNotPreventCurrentInspectionOrKeepDataRecovery()throws Exception {
        RecoveryBundle b=valid();ProductRadio radio=new ProductRadio(b);radio.wrongGate=true;InstallJournal j=journal();
        new ProductBootSession(new NdcpSession(radio)).inspectCurrent(b,j,m->{});
        new RoutineUpdateSession(new NdcpSession(radio)).prepareMaintenance(b,j,m->{});
        assertTrue(radio.commands.contains(0x62));assertTrue(radio.commands.contains(0x63));
        try{new RoutineUpdateSession(new NdcpSession(radio)).update(b,j,m->{});fail();}
        catch(IOException expected){assertTrue(expected.getMessage().contains("RecoveryGate"));}
        assertFalse(radio.commands.contains(0x40));assertFalse(radio.commands.contains(0x43));
    }
    @Test public void oldRunningProductCannotFalselyConfirmNewCandidate()throws Exception {
        RecoveryBundle b=valid();ProductRadio radio=new ProductRadio(b);
        try{new ProductBootSession(new NdcpSession(radio)).confirm(b,journal(),m->{});fail();}
        catch(IOException expected){assertTrue(expected.getMessage().contains("아직 실행 중이 아니"));}
        assertFalse(radio.commands.contains(0x5c));assertFalse(radio.commands.contains(0x40));
    }
    @Test public void existingCandidateOrRemoteWorkCannotBeClearedByInspectingOldProduct()throws Exception {
        RecoveryBundle b=valid();
        for(int which=0;which<4;which++){
            ProductRadio radio=new ProductRadio(b);InstallJournal j=journal();
            if(which==0)j.values.setProperty("transaction","42");
            if(which==1)j.save("WAIT_CFW_BOOT");
            if(which==2)radio.updateState=3;
            if(which==3){radio.bootState=3;radio.flags=1;}
            try{new ProductBootSession(new NdcpSession(radio)).inspectCurrent(b,j,m->{});fail();}catch(IOException expected){}
            assertNotEquals("CURRENT_CFW_CONFIRMED",j.state());
            for(int op:new int[]{0x40,0x43,0x47,0x5c})assertFalse(radio.commands.contains(op));
        }
    }
    @Test public void newPhoneProductBindingDoesNotRequireLostBootstrapBackupOrWriteTarget()throws Exception {
        CaptureRadio radio=new CaptureRadio();RecoveryBundle b=captureBundle();InstallJournal j=journal();
        byte[] r=radio.identity();ByteCodec.putU32le(r,8,2);
        TargetBinding.verifyProduct(new NdcpSession(radio),b,j,r);
        assertEquals(0,radio.reads);assertFalse(j.values.containsKey("resident.backup"));
        assertEquals("product-observed-no-raw-backup",j.values.getProperty("resident.binding"));
        InstallJournal next=new InstallJournal(new File(j.evidenceRoot(),"next-package.journal"));
        TargetBinding.verifyProduct(new NdcpSession(radio),b,next,r);
        r[56]^=1;
        try{TargetBinding.verifyProduct(new NdcpSession(radio),b,next,r);fail();}catch(IOException expected){}
        assertEquals(0,radio.reads);
    }
    @Test public void productBindingNeverBypassesExistingOriginalBackup()throws Exception {
        CaptureRadio radio=new CaptureRadio();RecoveryBundle b=captureBundle();InstallJournal j=journal();
        TargetBinding.verify(new NdcpSession(radio),b,j,radio.identity(),1);
        byte[] r=radio.identity();ByteCodec.putU32le(r,8,2);r[56]^=1;
        try{TargetBinding.verifyProduct(new NdcpSession(radio),b,j,r);fail();}catch(IOException expected){}
        assertEquals(274,radio.reads);
    }

}
