package io.opennoodoe.app.installer;

import io.opennoodoe.app.protocol.ByteCodec;
import java.io.*;
import java.util.Arrays;

/** Uploads a release-pinned standalone cleaner through the common NDCP updater.
 * This does not delete data: the cleaner asks for a fresh local O hold. Once
 * reset is requested, no Bluetooth progress or deletion success is invented. */
public final class UninstallSession {
    private final NdcpSession session;
    public UninstallSession(NdcpSession s){session=s;}
    private static long u(byte[] b,int n){return ByteCodec.u32le(b,n);}
    /** Query only: an uncertain COMMIT/RESET must never be replayed by reconciliation. */
    public void reconcile(RecoveryBundle bundle,InstallJournal j)throws Exception {
        new RoutineUpdateSession(session).identify(bundle,j);
        byte[] image=bundle.image("uninstall"),s=session.status();
        capability(session.request(0x93,new byte[0]),image,j.values.getProperty("uid"));
        if(s.length!=84||!j.values.containsKey("uninstall.tx")||u(s,8)!=RecoveryBundle.number(j.values,"uninstall.tx")||
           !RecoveryBundle.sha(image).equals(j.values.getProperty("uninstall.sha256")))
            throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0622,"삭제 작업을 현재 기기 상태와 대조할 수 없어요. 재전송하지 않았어요. 순정 복귀 확인 또는 기기 화면을 확인해 주세요."));
        long state=u(s,4);
        if(state>=3&&state<=5&&!Arrays.equals(Arrays.copyOfRange(s,52,84),NdcpSession.hex(RecoveryBundle.sha(image))))
            throw new IOException("Uninstall verification identity differs");
        j.values.setProperty("uninstall.queried.state",Long.toString(state));
        if(state==5)j.save("UNINSTALL_DEVICE_RUNNING");
        else if(state==4)j.save("UNINSTALL_COMMITTED");
        else if(state==3)j.save("UNINSTALL_VERIFIED");
        else if(state==1)j.save("UNINSTALL_SECTOR_VERIFIED");
        else throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0623,"삭제 작업은 아직 확인 대기 중이에요. 기기 상태 ")+state+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0624,". 변경 명령은 보내지 않았어요."));
    }
    static void capability(byte[] r,byte[] image,String uid)throws IOException {
        RecoveryBundle.validateUninstall(image);
        if(r.length!=64||u(r,4)!=1||(u(r,8)&2)==0||u(r,24)!=image.length||u(r,28)!=7||
           !(u(r,12)+","+u(r,16)+","+u(r,20)).equals(uid)||
           !Arrays.equals(Arrays.copyOfRange(r,32,64),NdcpSession.hex(RecoveryBundle.sha(image))))
            throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0625,"삭제 펌웨어와 현재 CFW의 지원 정보가 달라요. 이 ZIP의 CFW로 먼저 업데이트해 주세요. 데이터 유지 복귀로 대신 실행하지 않아요."));
    }
    public void run(RecoveryBundle bundle,InstallJournal j,StockUpdateSession.Progress p)throws Exception {
        bundle.requireInstallable();
        if(!bundle.manifest.containsKey("uninstall.file"))throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0626,"삭제 전용 펌웨어가 든 최신 ZIP이 필요해요."));
        byte[] image=bundle.image("uninstall"),digest=NdcpSession.hex(RecoveryBundle.sha(image));
        new RoutineUpdateSession(session).identify(bundle,j);p.role("product");
        try{capability(session.request(0x93,new byte[0]),image,j.values.getProperty("uid"));}
        catch(NdcpSession.DeviceRejected old){throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0627,"현재 CFW는 완전 삭제를 지원하지 않아요. 먼저 CFW를 업데이트해 주세요."),old);}
        byte[] state=session.status();if(state.length!=84)throw new IOException("Invalid update status");
        String hash=RecoveryBundle.sha(image);
        boolean same=hash.equals(j.values.getProperty("uninstall.sha256"))&&j.values.containsKey("uninstall.tx")&&
            u(state,8)==RecoveryBundle.number(j.values,"uninstall.tx");
        if(same&&u(state,4)>=3&&u(state,4)<=5){
            if(!Arrays.equals(Arrays.copyOfRange(state,52,84),digest))throw new IOException("Uninstall verification identity differs");
            if(u(state,4)==5){j.save("UNINSTALL_DEVICE_RUNNING");return;}
            if(u(state,4)==4){reset(j);return;}
            if(u(state,16)!=image.length)throw new IOException("Uninstall image is not fully verified");
            // Fresh authorization after reconnect, with no second upload.
            authorize(p);commit(j,digest);return;
        }
        if(j.state().startsWith("UNINSTALL_")&&(j.state().contains("COMMIT")||j.state().contains("RESET")||j.state().equals("UNINSTALL_DEVICE_RUNNING")))
            throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0628,"삭제 도구 설치 결과가 아직 불명확해요. 순정 복귀 확인부터 실행해 주세요. 재전송하지 않았어요."));
        if(u(state,4)!=0&&u(state,4)!=6&&!(same&&u(state,4)==1))throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0629,"기기에 처리 중인 작업이 남아 있어요. 상태를 확인해 주세요."));
        if(same&&u(state,4)==1)authorize(p);else new RoutineUpdateSession(session).prepareMaintenance(bundle,j,p);
        // Export is best effort; a failed/partial export is retained and named as such.
        if(!j.values.containsKey("uninstall.log.attempted")){
            j.values.setProperty("uninstall.log.attempted","true");j.save("UNINSTALL_LOG_EXPORT");
            try{File f=DeviceLogDownload.run(session,new File(j.evidenceRoot(),"before-uninstall"),p);
                j.values.setProperty("uninstall.log.file",f.getCanonicalPath());}
            catch(Exception e){j.values.setProperty("uninstall.log.result","unavailable-or-partial");}
        }
        long tx=same&&u(state,4)==1?RecoveryBundle.number(j.values,"uninstall.tx"):(new java.security.SecureRandom().nextInt()&0x7fffffffL);if(tx==0)tx=1;
        j.values.setProperty("return.mode","ERASE_CFW_DATA");j.values.setProperty("uninstall.tx",Long.toString(tx));
        j.values.setProperty("uninstall.sha256",hash);j.values.setProperty("transaction",Long.toString(tx));
        byte[] begin=new byte[56];ByteCodec.putU32le(begin,0,tx);ByteCodec.putU32le(begin,4,7);
        ByteCodec.putU32le(begin,8,image.length);ByteCodec.putU32le(begin,12,NdcpSession.crc(image,image.length));
        System.arraycopy(digest,0,begin,16,32);ByteCodec.putU32le(begin,48,0x53544147);ByteCodec.putU32le(begin,52,3);
        j.save("UNINSTALL_BEGIN_RESULT_UNKNOWN");byte[] r=session.request(0x40,begin);
        if(r.length!=20||u(r,4)!=1||u(r,8)!=tx||u(r,12)>image.length)throw new IOException("Uninstall begin not confirmed");
        TransferCapabilities caps=TransferCapabilities.query(session);
        for(int offset=(int)u(r,12);offset<image.length;){int n=caps.count(offset,image.length-offset);byte[] b=new byte[n+8];
            ByteCodec.putU32le(b,0,tx);ByteCodec.putU32le(b,4,offset);System.arraycopy(image,offset,b,8,n);
            r=session.request(0x41,b);offset+=n;
            if(r.length!=20||u(r,4)!=1||u(r,8)!=tx||u(r,12)!=offset)throw new IOException("Uninstall transfer offset differs");
            p.stage("stage",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0630,"삭제 전용 펌웨어 전송 · 아직 CFW 데이터는 지우지 않아요."),offset,image.length,"B");
            if((offset&4095)==0){j.values.setProperty("offset",Integer.toString(offset));j.save("UNINSTALL_SECTOR_VERIFIED");}
        }
        j.save("UNINSTALL_VERIFY_RESULT_UNKNOWN");r=session.request(0x42,NdcpSession.words(tx));
        if(r.length!=52||u(r,4)!=3||u(r,8)!=tx||u(r,16)!=image.length||!Arrays.equals(Arrays.copyOfRange(r,20,52),digest))
            throw new IOException("Uninstall full physical verification failed");
        commit(j,digest);
    }
    private void authorize(StockUpdateSession.Progress p)throws Exception {
        session.request(0x62,new byte[0]);long until=System.nanoTime()+60_000_000_000L;
        while(System.nanoTime()<until){byte[] r=session.request(0x63,new byte[0]);
            if(r.length!=12)throw new IOException("Invalid recovery audit");
            if(u(r,4)==3&&u(r,8)==0)return;
            if(u(r,4)==4)throw new IOException("Recovery audit failed");Thread.sleep(50);
        }throw new IOException("Recovery audit timeout");
    }
    private void commit(InstallJournal j,byte[] digest)throws Exception {
        long tx=RecoveryBundle.number(j.values,"uninstall.tx");byte[] b=new byte[40];
        ByteCodec.putU32le(b,0,tx);ByteCodec.putU32le(b,4,0x434f4d54);System.arraycopy(digest,0,b,8,32);
        j.save("UNINSTALL_COMMIT_RESULT_UNKNOWN");byte[] r=session.request(0x43,b);
        if(r.length!=20||u(r,4)!=4||u(r,8)!=tx)throw new IOException("Uninstall commit result unknown");
        j.save("UNINSTALL_COMMITTED");reset(j);
    }
    private void reset(InstallJournal j)throws Exception {
        j.save("UNINSTALL_RESET_RESULT_UNKNOWN");
        session.request(0x47,NdcpSession.words(RecoveryBundle.number(j.values,"uninstall.tx"),0x52535421));
        Thread.sleep(2000);j.save("UNINSTALL_DEVICE_RUNNING");
    }
}
