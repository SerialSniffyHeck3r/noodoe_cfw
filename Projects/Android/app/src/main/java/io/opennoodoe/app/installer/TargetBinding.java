package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import io.opennoodoe.app.diagnostics.DurableFiles;
import java.io.*;
import java.util.Arrays;

/** First-use capture is not factory authentication. The exact original bytes
 * survive package changes; future sessions must match that same device BL.
 * All file contents are private local evidence, never event-log payloads. */
public final class TargetBinding {
    private static long u(byte[] b,int at){return ByteCodec.u32le(b,at);}
    static long version(RecoveryBundle b)throws IOException {
        return RecoveryBundle.number(b.manifest,"target.boot.major")|(RecoveryBundle.number(b.manifest,"target.boot.minor")<<16);
    }
    static long version(InstallJournal j)throws IOException {
        return RecoveryBundle.number(j.values,"resident.version");
    }
    private static File path(InstallJournal j,String uid)throws IOException {
        if(!uid.matches("[0-9]+,[0-9]+,[0-9]+"))throw new IOException("Invalid target UID");
        return new File(new File(j.evidenceRoot(),"targets"),uid.replace(',','-')+".lower64.bin");
    }
    static void verify(NdcpSession session,RecoveryBundle bundle,InstallJournal journal,byte[] identity,int role)throws Exception {
        String uid=u(identity,12)+","+u(identity,16)+","+u(identity,20);
        if(journal.values.containsKey("uid")&&!uid.equals(journal.values.getProperty("uid")))throw new IOException("Device UID changed");
        DeviceAttempts.checkUid(journal,uid);
        byte[] actual=Arrays.copyOfRange(identity,56,88);
        String binding=bundle.manifest.getProperty("target.boot.binding", "pinned");
        if(!binding.equals("device-capture")&&!Arrays.equals(actual,NdcpSession.hex(RecoveryBundle.required(bundle.manifest,"target.boot.sha256"))))
            throw new IOException("Resident BL does not match pinned image");
        if(binding.equals("pinned")){
            journal.values.setProperty("resident.version",Long.toString(version(bundle)));
            journal.values.setProperty("resident.sha256",RecoveryBundle.required(bundle.manifest,"target.boot.sha256"));return;
        }
        File file=path(journal,uid);byte[] lower;
        if(file.isFile()){
            try(InputStream in=new FileInputStream(file)){lower=RecoveryBundle.bounded(in,65536);}
        }else{
            if(role!=1)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0618,"이 기기의 원본 백업이 없어요. Bootstrap에서 먼저 보관해 주세요."));
            byte[] a=read(session,version(bundle)),b=read(session,version(bundle));
            if(!Arrays.equals(a,b))throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0619,"원본을 두 번 읽은 값이 달라요. 저장이나 설치를 시작하지 않았어요."));
            lower=a;
            validate(lower,actual,version(bundle));
            File parent=file.getParentFile();if(!parent.isDirectory()&&!parent.mkdirs())throw new IOException("Target backup directory unavailable");
            File temporary=new File(file+".partial");
            if(temporary.exists())throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0620,"이전 원본 백업 작업이 남아 있어요. 기록을 먼저 확인해 주세요."));
            try(FileOutputStream out=new FileOutputStream(temporary)){out.write(lower);out.getFD().sync();}
            try(InputStream in=new FileInputStream(temporary)){if(!Arrays.equals(lower,RecoveryBundle.bounded(in,65536)))throw new IOException("Target backup readback mismatch");}
            if(!temporary.renameTo(file))throw new IOException("Cannot publish original target backup");
            DurableFiles.directory(parent);
        }
        validate(lower,actual,version(bundle));
        journal.values.setProperty("uid",uid);
        journal.values.setProperty("resident.version",Long.toString(version(bundle)));
        journal.values.setProperty("resident.sha256",RecoveryBundle.sha(Arrays.copyOf(lower,32768)));
        journal.values.setProperty("resident.backup",file.getCanonicalPath());
        journal.values.setProperty("resident.backup.sha256",RecoveryBundle.sha(lower));
        journal.save(journal.state());
    }
    /** Product updates do not modify BL/factory data and must also work on a
     * replacement phone. Preserve any original backup; if absent, bind the
     * observed BL hash to UID without pretending to have captured its bytes. */
    static void verifyProduct(NdcpSession session,RecoveryBundle bundle,InstallJournal journal,byte[] identity)throws Exception {
        if(identity.length!=88||u(identity,0)!=0||u(identity,4)!=1||u(identity,8)!=2)
            throw new IOException("Invalid Product identity");
        String uid=u(identity,12)+","+u(identity,16)+","+u(identity,20);
        if(journal.values.containsKey("uid")&&!uid.equals(journal.values.getProperty("uid")))throw new IOException("Device UID changed");
        DeviceAttempts.checkUid(journal,uid);
        File original=path(journal,uid);
        if(original.isFile()||"pinned".equals(bundle.manifest.getProperty("target.boot.binding","pinned"))){
            verify(session,bundle,journal,identity,2);return;
        }
        byte[] actual=Arrays.copyOfRange(identity,56,88);
        String digest=BootstrapProvisioner.toHex(actual);
        boolean zero=true,erased=true;for(byte value:actual){zero&=value==0;erased&=value==(byte)255;}
        if(zero||erased)throw new IOException("Missing resident BL identity");
        if(!"device-capture".equals(bundle.manifest.getProperty("target.boot.binding"))&&
           !digest.equals(RecoveryBundle.required(bundle.manifest,"target.boot.sha256")))
            throw new IOException("Resident BL does not match pinned image");
        InstallJournal observed=new InstallJournal(new File(original.getParentFile(),uid.replace(',','-')+".product-binding"));
        String prior=observed.values.getProperty("resident.sha256",digest);
        if(!prior.equals(digest)||!uid.equals(observed.values.getProperty("uid",uid)))
            throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0621,"기기의 BL 식별값이 이전 연결과 달라요. 원본 기록을 보존하고 확인해 주세요."));
        observed.values.setProperty("uid",uid);observed.values.setProperty("resident.sha256",digest);
        observed.save("PRODUCT_BL_OBSERVED_NO_RAW_BACKUP");
        journal.values.setProperty("uid",uid);journal.values.setProperty("resident.sha256",digest);
        journal.values.setProperty("resident.version",Long.toString(version(bundle)));
        journal.values.setProperty("resident.binding","product-observed-no-raw-backup");
        journal.save(journal.state());
    }
    static void validate(byte[] lower,byte[] actual,long version)throws IOException {
        if(lower.length!=65536||u(lower,0x8000)!=version||!Arrays.equals(actual,NdcpSession.hex(RecoveryBundle.sha(Arrays.copyOf(lower,32768)))))
            throw new IOException("Original target BL/version changed");
    }
    private static byte[] read(NdcpSession session,long version)throws Exception {
        byte[] lower=new byte[65536];long deadline=System.nanoTime()+60_000_000_000L;
        for(int off=0;off<lower.length;){int n=Math.min(480,lower.length-off);
            byte[] r;
            try{r=session.request(0x60,NdcpSession.words(off,n));}
            catch(NdcpSession.DeviceRejected busy){if(busy.result!=3||System.nanoTime()>=deadline)throw busy;Thread.sleep(100);continue;}
            if(r.length!=32+n||u(r,4)!=1||u(r,8)!=off||u(r,12)!=n||u(r,16)!=version||
               (u(r,20)!=3&&u(r,20)!=4)||u(r,24)<3||(off==0&&u(r,28)!=1))throw new IOException("Unsupported target profile/export");
            System.arraycopy(r,32,lower,off,n);off+=n;
        }return lower;
    }
}
