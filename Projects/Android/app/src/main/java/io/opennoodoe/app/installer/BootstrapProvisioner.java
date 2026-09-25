package io.opennoodoe.app.installer;

import io.opennoodoe.app.protocol.ByteCodec;
import java.io.*;
import java.security.MessageDigest;
import java.util.*;

/** Scoped preimages, independent FAT plan and device-local unchanged-data verification. */
public final class BootstrapProvisioner {
    public interface JpegCheck {void validate(byte[] data)throws IOException;}
    private final NdcpSession session;
    private final InstallJournal journal;
    private final StockUpdateSession.Progress progress;
    private final BootstrapProgress monitor;
    public BootstrapProvisioner(NdcpSession s,InstallJournal j,StockUpdateSession.Progress p){session=s;journal=j;progress=p;monitor=new BootstrapProgress(s,p);}
    private byte[] request(int opcode,byte[] payload)throws IOException {
        byte[] r=session.request(opcode,payload);require(r.length>=32,"Bootstrap response truncated");
        require(ByteCodec.u32le(r,8)==0&&ByteCodec.u32le(r,4)!=9,"Bootstrap failed; do not automatically retry");return r;
    }
    public void run(RecoveryBundle bundle,File out,JpegCheck jpeg)throws Exception {
        journal.require("BOOTSTRAP_IDENTIFIED");long[] uid=session.identity(bundle,journal,1);progress.role("bootstrap");
        require(!out.exists()&&out.mkdirs(),"Use a new evidence directory; prior attempt is preserved");
        if(TransferCapabilities.query(session).scopedV2){
            require(out.getUsableSpace()>=32L*1024*1024,"At least 32MiB free phone storage required for FAT/log evidence");
            journal.values.setProperty("backup.directory",out.getCanonicalPath());journal.values.setProperty("backup.mode","scoped-v2");journal.save("BACKUP_IN_PROGRESS");
            new ScopedProvisioningV2(session,journal,progress,out).run(bundle,uid,initialImages(bundle,journal,uid));return;
        }
        require(!"restore".equals(journal.values.getProperty("reinstall.policy")),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0416,"보관 데이터 복원은 scoped-v2 설치 도구가 필요해요."));
        require(out.getUsableSpace()>=512L*1024*1024,"At least512MiB free phone storage required for legacy evidence/planning");
        journal.values.setProperty("backup.directory",out.getCanonicalPath());journal.values.setProperty("backup.mode","scoped-v1");journal.save("BACKUP_IN_PROGRESS");
        File expected=new File(out,"expected.bin");ScopedBackup scoped=new ScopedBackup(session,progress,expected);scoped.snapshot();
        byte[][] images=initialImages(bundle,journal,uid);
        int existing;try(BootstrapFatPlan fs=new BootstrapFatPlan(expected)){existing=fs.existingContainers();}
        require(existing==0||existing==9,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0417,"CFW 파일 생성이 일부만 끝난 상태예요. 덮어쓰지 않았어요. 진단 자료를 보관해 주세요."));
        journal.values.setProperty("backup.reused",existing==9?"true":"false");journal.save("BACKUP_IN_PROGRESS");
        byte[] originalHash;
        if(existing==9)originalHash=scoped.reuse(images);
        else {
            originalHash=scoped.begin(images);
            for(int kind:ScopedBackup.ORDER)install(expected,originalHash,kind,images[kind],new File(out,BootstrapFatPlan.NAMES[kind]));
        }
        journal.save("POSTIMAGE_VERIFY_IN_PROGRESS");scoped.finish();
        journal.values.setProperty("backup.sha256",toHex(originalHash));journal.values.setProperty("planned.sha256",toHex(hash(expected)));journal.save("PROVISIONED");
        // Keep the same authenticated session open for update authorization/stage/commit/reset.
        session.request(0x1f,NdcpSession.words(uid[0],uid[1],uid[2],0x42414b32));
        progress.update(existing==9?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0418,"이전 설치 파일 재사용·내용·보존 검증 완료"):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0419,"새 CFW 설정·빈 사진 슬롯 준비 및 기존 파일 보존 검증 완료"));
    }
    static byte[][] initialImages(RecoveryBundle bundle,InstallJournal journal,long[] uid)throws Exception {
        byte[][] images=new byte[9][];
        {
            images[0]=bundle.image("resources");require(images[0].length==1048576,"Resources container must be1MiB");
            images[1]=BootstrapFatPlan.filled(131072);System.arraycopy(BootstrapFatPlan.record(1,uid,new byte[0],0),0,images[1],0,4096);
            images[2]=BootstrapFatPlan.filled(262144);System.arraycopy(BootstrapFatPlan.record(2,uid,new byte[0],0),0,images[2],0,4096);
            images[3]=freshPhotos(uid);
            // Fresh install: no stock or bench photos are imported. Empty banks are valid.
        }
        byte[] stock=bundle.paddedImage("stock");require(RecoveryBundle.sha(stock).equals("162faeecc64bf56d828570168785e66d1a7da0152ef912bf83f9a19094d91edf"),"Pinned V5.16 recovery APP required");
        images[4]=BootstrapFatPlan.filled(524288);byte[] rec=images[4];long[] header={0x3152434e,2,4096,524288,uid[0],uid[1],uid[2],0x00100005,0x70000,TargetBinding.version(journal)};
        for(int i=0;i<header.length;i++)ByteCodec.putU32le(rec,i*4,header[i]);System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(stock)),0,rec,40,32);
        System.arraycopy(NdcpSession.hex(RecoveryBundle.required(journal.values,"resident.sha256")),0,rec,72,32);
        ByteCodec.putU32le(rec,4088,NdcpSession.crc(rec,4088));ByteCodec.putU32le(rec,4092,0x31544d43);System.arraycopy(stock,0,rec,4096,stock.length);
        byte[] product=GateContainers.product(bundle.paddedImage("cfw"));
        long version=RecoveryBundle.number(bundle.manifest,"cfw.major")|(RecoveryBundle.number(bundle.manifest,"cfw.minor")<<16);
        images[5]=GateContainers.image(product,uid,0,version);images[6]=GateContainers.image(product,uid,1,version);images[7]=GateContainers.journal(product,uid,"restore".equals(journal.values.getProperty("reinstall.policy")));
        images[8]=DeviceLogContainer.create(uid);
        return images;
    }
    /** A clean CFW gallery contains no imported stock/bench pixels. */
    static byte[] freshPhotos(long[] uid) {
        byte[] image=BootstrapFatPlan.filled(1048576);
        System.arraycopy(BootstrapFatPlan.record(3,uid,NdcpSession.words(0x31465043,1),0),0,image,0,4096);
        return image;
    }
    /** Continue only AFTER all containers were durably verified. Partial FAT
     * publication is not resumable here. A new link needs fresh A/B proof. */
    public void resume(RecoveryBundle bundle,File out)throws Exception {
        require(BootstrapInstallFlow.resume(journal),"No resumable installation is selected");
        long[] uid=session.identity(bundle,journal,1);progress.role("bootstrap");byte[] status=session.status();
        long state=ByteCodec.u32le(status,4);
        require(state!=4&&state!=5&&ByteCodec.u32le(status,32)==TargetBinding.version(journal)&&ByteCodec.u32le(status,48)==0,
                io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0420,"설치 확정 여부가 불명확해요. 작업 확인부터 해 주세요."));
        if("scoped-v2".equals(journal.values.getProperty("backup.mode"))){
            if(!TransferCapabilities.query(session).scopedV2)throw new IOException("This device cannot validate scoped-v2 evidence");
            if(state!=0){byte[] aborted=session.request(0x44,NdcpSession.words(ByteCodec.u32le(status,8)));if(ByteCodec.u32le(aborted,4)!=0)throw new IOException("Previous stage was not cancelled");}
            File fresh=new File(RecoveryBundle.required(journal.values,"backup.directory"),"resume-"+java.util.UUID.randomUUID());
            new ScopedProvisioningV2(session,journal,progress,fresh).run(bundle,uid,initialImages(bundle,journal,uid));return;
        }
        File expected=new File(RecoveryBundle.required(journal.values,"backup.directory"),"expected.bin");
        String planned=RecoveryBundle.required(journal.values,"planned.sha256");
        require(expected.length()==0x8000000L&&toHex(hash(expected)).equals(planned),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0421,"원래 설치 계획 파일을 확인할 수 없어요."));
        if(state!=0){byte[] aborted=session.request(0x44,NdcpSession.words(ByteCodec.u32le(status,8)));if(ByteCodec.u32le(aborted,4)!=0)throw new IOException("Previous stage was not cancelled");}
        if(!"scoped-v1".equals(journal.values.getProperty("backup.mode")))throw new IOException("Use the installer matching the original backup format");
        new ScopedBackup(session,progress,expected).resume();journal.save("PROVISIONED");
        session.request(0x1f,NdcpSession.words(uid[0],uid[1],uid[2],0x42414b32));
    }
    static boolean equalPrefix(File a,File b,long bytes)throws IOException {
        if(bytes<0||a.length()<bytes||b.length()<bytes)return false;
        try(DataInputStream x=new DataInputStream(new FileInputStream(a));DataInputStream y=new DataInputStream(new FileInputStream(b))){
            byte[] bx=new byte[32768],by=new byte[32768];
            for(long pos=0;pos<bytes;pos+=bx.length){int n=(int)Math.min(bx.length,bytes-pos);x.readFully(bx,0,n);y.readFully(by,0,n);for(int i=0;i<n;i++)if(bx[i]!=by[i])return false;}
        }return true;
    }
    public byte[] backup(File out)throws Exception {
        require(out.getParentFile().getUsableSpace()>=300L*1024*1024,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0422,"전체 NOR 이중 백업에는 300MiB 이상의 빈 공간이 필요해요."));
        require(out.mkdirs(),"Backup directory must be new");
        for(int pass=0;pass<2;pass++) {
            request(0x50,NdcpSession.words(pass));File f=new File(out,pass==0?"A.bin":"B.bin");
            try(FileOutputStream output=new FileOutputStream(f)) {
                for(int offset=0;offset<0x8000000;) {
                    int count=Math.min(480,0x8000000-offset);byte[] r=request(0x51,NdcpSession.words(offset,count));
                    require(r.length==32+count&&ByteCodec.u32le(r,24)==offset+count,"Backup offset mismatch");output.write(r,32,count);offset+=count;
                    progress.stage("backup-full-"+pass,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0423,"정밀 진단: 전체 NOR 독립 백업 ")+(pass+1)+" / 2",offset,134217728,"B");
                }
                output.getFD().sync();
            }
        }
        File a=new File(out,"A.bin"),b=new File(out,"B.bin");byte[] digest=hash(a),r=request(0x56,new byte[0]);
        require(r.length==64&&ByteCodec.u32le(r,28)==1&&Arrays.equals(digest,Arrays.copyOfRange(r,32,64))
                &&Arrays.equals(digest,hash(b))&&equal(a,b),"Independent NOR A/B or device hash differs");
        write(new File(out,"sha256.txt"),toHex(digest).getBytes(java.nio.charset.StandardCharsets.US_ASCII));return digest;
    }
    private void install(File expected,byte[] backup,int kind,byte[] image,File out)throws Exception {
        require(out.mkdirs(),"Install evidence directory must be new");byte[] before,after;long address;
        try(BootstrapFatPlan fs=new BootstrapFatPlan(expected)){before=fs.metadata.clone();address=fs.allocate(kind,image.length);after=fs.metadata.clone();}
        write(new File(out,"metadata-before.bin"),BootstrapFatPlan.swap(before));write(new File(out,"metadata-after.bin"),BootstrapFatPlan.swap(after));
        try(RandomAccessFile raw=new RandomAccessFile(expected,"r")){byte[] old=new byte[image.length];raw.seek(address);raw.readFully(old);write(new File(out,"extent-before.bin"),old);}
        write(new File(out,"payload.bin"),image);write(new File(out,"extent-after.bin"),BootstrapFatPlan.swap(image));
        byte[] digest=hash(new File(out,"payload.bin")),begin=new byte[72];ByteCodec.putU32le(begin,0,kind);ByteCodec.putU32le(begin,4,image.length);
        System.arraycopy(digest,0,begin,8,32);System.arraycopy(backup,0,begin,40,32);
        journal.values.setProperty("create.kind",Integer.toString(kind));journal.values.setProperty("create.address",Long.toString(address));journal.save("CREATE_BEGIN_RESULT_UNKNOWN");
        request(0x52,begin);
        for(int offset=0;offset<image.length;){
            progress.stage("file-upload-"+kind,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0424,"파일 준비 · ")+BootstrapFatPlan.NAMES[kind],offset,image.length,"B");
            int blank=0;while(blank<65536&&offset+blank<image.length&&image[offset+blank]==(byte)255)blank++;
            if(blank>=64){request(0x83,NdcpSession.words(offset,blank));offset+=blank;continue;}
            int n=Math.min(ScopedBackup.CHUNK,image.length-offset);byte[] data=new byte[4+n];ByteCodec.putU32le(data,0,offset);System.arraycopy(image,offset,data,4,n);request(0x53,data);offset+=n;
        }
        request(0x54,new byte[0]);byte[] ready=poll(6,180000);
        require(ByteCodec.u32le(ready,16)==address&&ByteCodec.u32le(ready,20)==image.length,"Device allocation differs from independent plan");
        ByteArrayOutputStream metadata=new ByteArrayOutputStream();
        for(int offset=0;offset<0x9000;offset+=480){byte[] r=request(0x56,NdcpSession.words(offset));metadata.write(r,32,r.length-32);}
        require(Arrays.equals(after,metadata.toByteArray()),"Device prepared metadata differs");write(new File(out,"device-prepared.bin"),metadata.toByteArray());
        journal.save("CREATE_COMMIT_RESULT_UNKNOWN");request(0x55,digest);poll(8,600000);
        try(RandomAccessFile raw=new RandomAccessFile(expected,"rw")){raw.seek(0);raw.write(BootstrapFatPlan.swap(after));raw.seek(address);raw.write(BootstrapFatPlan.swap(image));raw.getFD().sync();}
        journal.save("FILE_CREATED");progress.update(BootstrapFatPlan.NAMES[kind]+" physical readback verified");
    }
    private byte[] poll(int state,long timeout)throws Exception {
        long end=System.nanoTime()+timeout*1_000_000L;
        do{byte[] r=request(0x56,new byte[0]);if(ByteCodec.u32le(r,4)==state)return r;monitor.poll();Thread.sleep(20);}while(System.nanoTime()<end);
        throw new IOException("Bootstrap timeout; preserve journal and do not retry publication");
    }
    public static byte[] hash(File f)throws Exception{MessageDigest d=MessageDigest.getInstance("SHA-256");try(InputStream in=new FileInputStream(f)){byte[] b=new byte[32768];int n;while((n=in.read(b))!=-1)d.update(b,0,n);}return d.digest();}
    static String toHex(byte[] b){StringBuilder s=new StringBuilder();for(byte v:b)s.append(String.format(Locale.ROOT,"%02x",v&255));return s.toString();}
    public static boolean equal(File a,File b)throws IOException{if(a.length()!=b.length())return false;try(DataInputStream x=new DataInputStream(new FileInputStream(a));DataInputStream y=new DataInputStream(new FileInputStream(b))){byte[] bx=new byte[32768],by=new byte[32768];for(long pos=0;pos<a.length();pos+=bx.length){int n=(int)Math.min(bx.length,a.length()-pos);x.readFully(bx,0,n);y.readFully(by,0,n);for(int i=0;i<n;i++)if(bx[i]!=by[i])return false;}}return true;}
    private static void copy(File from,File to)throws IOException{try(InputStream in=new FileInputStream(from);FileOutputStream out=new FileOutputStream(to)){byte[] b=new byte[32768];int n;while((n=in.read(b))!=-1)out.write(b,0,n);out.getFD().sync();}}
    private static void write(File file,byte[] bytes)throws IOException{try(FileOutputStream out=new FileOutputStream(file)){out.write(bytes);out.getFD().sync();}io.opennoodoe.app.diagnostics.DurableFiles.directory(file.getParentFile());}
    private static void require(boolean ok,String message)throws IOException{BootstrapFatPlan.require(ok,message);}
}
