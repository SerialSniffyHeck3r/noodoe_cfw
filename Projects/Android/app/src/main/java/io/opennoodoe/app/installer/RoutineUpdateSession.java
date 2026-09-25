package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import java.io.IOException;
import java.util.Arrays;

/** Product-only updates never upload Gate, stock BL or an arbitrary address.
 * A reconnect queries remote state before considering any write or reset. */
public final class RoutineUpdateSession {
    private final NdcpSession session;
    public RoutineUpdateSession(NdcpSession session){this.session=session;}
    private static long u(byte[] b,int offset){return ByteCodec.u32le(b,offset);}
    /** Existing-device inspection/restoration is independent of a NEW Gate in
     * the selected ZIP. Only an APP update must prove Gate compatibility. */
    byte[] identifyCurrent(RecoveryBundle bundle,InstallJournal journal)throws Exception {
        byte[] r=session.runningIdentity(2);
        String uid=u(r,12)+","+u(r,16)+","+u(r,20);
        if(journal.values.containsKey("uid")&&!journal.values.getProperty("uid").equals(uid))throw new IOException("Device UID changed");
        journal.values.setProperty("uid",uid);
        byte[] gate=session.request(0x64,new byte[0]);
        if(gate.length!=40||u(gate,0)!=0||u(gate,4)!=2)throw new IOException("Unsupported RecoveryGate identity protocol");
        journal.values.setProperty("source.gate.sha256",BootstrapProvisioner.toHex(Arrays.copyOfRange(gate,8,40)));
        TargetBinding.verifyProduct(session,bundle,journal,r);
        journal.values.setProperty("source.app.sha256",BootstrapProvisioner.toHex(Arrays.copyOfRange(r,24,56)));
        return r;
    }
    byte[] identify(RecoveryBundle bundle,InstallJournal journal)throws Exception {
        byte[] r=identifyCurrent(bundle,journal);
        String expected=RecoveryBundle.sha(Arrays.copyOf(bundle.paddedImage("cfw"),0x10000));
        if(!expected.equals(journal.values.getProperty("source.gate.sha256")))
            throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0575,"이 ZIP은 기기의 RecoveryGate와 달라요. 앱 버전을 내릴 필요는 없어요. 데이터 유지 순정 복귀 → 이 ZIP의 Bootstrap → 새 CFW 순서로 복구 계층을 함께 갱신해야 해요."));
        return r;
    }
    /** Stock restore shares the read-only FAT/recovery proof. It cannot borrow
     * an upload-in-progress or silently acknowledge a rollback result. */
    public void prepareMaintenance(RecoveryBundle bundle,InstallJournal journal,StockUpdateSession.Progress progress)throws Exception {
        bundle.requireInstallable();identifyCurrent(bundle,journal);progress.role("product");
        ProductBootSession.Status boot=new ProductBootSession(session).status();
        if(!boot.confirmed()||boot.rolledBack())throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0576,"먼저 부팅 또는 복귀 결과를 확인해 주세요."));
        byte[] update=session.status();long state=u(update,4);
        if(state!=0&&state!=6)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0577,"업데이트 처리 중에는 순정 복원을 시작하지 않아요."));
        preflight(progress);
    }
    private void preflight(StockUpdateSession.Progress progress)throws Exception {
        session.request(0x62,new byte[0]);long deadline=System.nanoTime()+60_000_000_000L;
        while(System.nanoTime()<deadline){byte[] r=session.request(0x63,new byte[0]);
            if(r.length!=12)throw new IOException("Preflight ABI mismatch");
            if(u(r,4)==3&&u(r,8)==0)return;
            if(u(r,4)==4)throw new IOException("Recovery preflight failed: "+u(r,8));
            progress.update(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0578,"기기의 복구본과 저장 구조를 확인하고 있어요."));Thread.sleep(50);
        }
        throw new IOException("Read-only recovery preflight timed out");
    }
    public boolean update(RecoveryBundle bundle,InstallJournal journal,StockUpdateSession.Progress progress)throws Exception {
        return transfer(bundle,journal,progress,false);
    }
    public boolean diagnostic(RecoveryBundle bundle,InstallJournal journal,StockUpdateSession.Progress progress)throws Exception {
        return transfer(bundle,journal,progress,true);
    }
    /** Shared sector transfer/resume/hash/commit implementation, distinct image role and journal evidence. */
    private boolean transfer(RecoveryBundle bundle,InstallJournal journal,StockUpdateSession.Progress progress,boolean diagnostic)throws Exception {
        progress.stage("connect",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0579,"현재 CFW·기기·복구 Gate와 업데이트 ZIP을 대조하고 있어요."),0,0,"");
        bundle.requireInstallable();identify(bundle,journal);progress.role("product");
        if(diagnostic){
            if(!bundle.manifest.containsKey("diagnostic.file"))throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0580,"진단 이미지가 포함된 최신 ZIP을 선택해 주세요."));
            byte[] cap=session.request(0x96,new byte[0]);
            if(cap.length!=12||u(cap,4)!=1||(u(cap,8)&1)==0)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0581,"이 Gate는 임시 진단을 지원하지 않아요. 순정 복귀 → 새 Bootstrap → CFW로 Gate를 먼저 갱신해 주세요."));
        }
        byte[] image=diagnostic?bundle.image("diagnostic"):GateContainers.product(bundle.paddedImage("cfw"));
        if(diagnostic)RecoveryBundle.validateDiagnostic(image);String hash=RecoveryBundle.sha(image);byte[] digest=NdcpSession.hex(hash);
        ProductBootSession.Status boot=new ProductBootSession(session).status();
        if(boot.rolledBack())throw new IOException(boot.result());
        if(Arrays.equals(boot.sha,digest))return false; // Reconcile by boot confirmation, never retransmit.
        if(!boot.confirmed())throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0582,"현재 CFW의 정상 실행 확정이 아직 끝나지 않았어요. 먼저 CFW 정상 부팅 확인을 실행해 주세요."));
        byte[] remote=session.status();
        if(remote.length!=84)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0583,"업데이트 상태 응답 형식이 맞지 않아요."));
        boolean same=journal.values.containsKey("transaction")&&hash.equals(journal.values.getProperty("image.sha256"))&&u(remote,8)==RecoveryBundle.number(journal.values,"transaction");
        if(same&&(u(remote,4)==4||u(remote,4)==5)){
            if(!Arrays.equals(Arrays.copyOfRange(remote,52,84),digest))throw new IOException("Committed image differs");
            if(u(remote,4)==5)throw new IOException("Reset already pending. Reconnect to query the boot result.");
            journal.save(diagnostic?"DIAGNOSTIC_COMMITTED":"PRODUCT_COMMITTED");reset(journal,diagnostic);return true;
        }
        if(same&&u(remote,4)==3&&u(remote,16)==image.length&&Arrays.equals(Arrays.copyOfRange(remote,52,84),digest)){
            preflight(progress);commit(journal,RecoveryBundle.number(journal.values,"transaction"),digest,diagnostic);return true;
        }
        if(!journal.state().startsWith("RESOURCE_")&&(journal.state().contains("COMMIT_RESULT_UNKNOWN")||journal.state().contains("RESET_RESULT_UNKNOWN")))
            throw new IOException("Cannot reconcile committed operation; no command was repeated");
        if(u(remote,4)!=0&&u(remote,4)!=6&&!same)
            throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0584,"기기에 다른 업데이트 작업이 남아 있어요. 해당 ZIP과 작업 기록으로 결과를 먼저 확인해 주세요."));
        progress.stage("audit",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0585,"기존 정상 CFW와 순정 복구본을 보존할 수 있는지 검사하고 있어요."),0,0,"");preflight(progress);
        TransferCapabilities negotiated=TransferCapabilities.query(session);
        if(!diagnostic)new ResourceUpdateSession(session).ensure(bundle,image,journal,progress,negotiated);
        long tx=hash.equals(journal.values.getProperty("image.sha256"))&&journal.values.containsKey("transaction")?
            RecoveryBundle.number(journal.values,"transaction"):(new java.security.SecureRandom().nextInt()&0x7fffffffL);
        if(tx==0)tx=1;
        long version=diagnostic?1:(RecoveryBundle.number(bundle.manifest,"cfw.major")|(RecoveryBundle.number(bundle.manifest,"cfw.minor")<<16));
        journal.values.setProperty("transaction",Long.toString(tx));journal.values.setProperty("image.sha256",hash);journal.values.setProperty("version",Long.toString(version));
        byte[] begin=new byte[56];ByteCodec.putU32le(begin,0,tx);ByteCodec.putU32le(begin,4,version);ByteCodec.putU32le(begin,8,image.length);
        ByteCodec.putU32le(begin,12,NdcpSession.crc(image,image.length));System.arraycopy(digest,0,begin,16,32);ByteCodec.putU32le(begin,48,0x53544147);ByteCodec.putU32le(begin,52,diagnostic?4:2);
        journal.save(diagnostic?"DIAGNOSTIC_BEGIN_RESULT_UNKNOWN":"PRODUCT_BEGIN_RESULT_UNKNOWN");byte[] answer=session.request(0x40,begin);
        if(answer.length!=20||u(answer,4)!=1||u(answer,8)!=tx||u(answer,12)>image.length||(u(answer,12)&4095)!=0)throw new IOException("Invalid sector resume proof");
        TransferCapabilities caps=negotiated;int offset=(int)u(answer,12);
        long snapshotAt=0;
        for(;offset<image.length;){int count=caps.count(offset,image.length-offset);byte[] chunk=new byte[8+count];ByteCodec.putU32le(chunk,0,tx);ByteCodec.putU32le(chunk,4,offset);System.arraycopy(image,offset,chunk,8,count);
            answer=session.request(0x41,chunk);
            if(answer.length!=20||u(answer,4)!=1||u(answer,8)!=tx||u(answer,12)!=offset+count)throw new IOException("DATA transaction/state/offset differs");
            if(((offset+count)&4095)==0){journal.values.setProperty("offset",Integer.toString(offset+count));journal.save(diagnostic?"DIAGNOSTIC_SECTOR_VERIFIED":"PRODUCT_SECTOR_VERIFIED");}
            progress.stage("stage",diagnostic?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0586,"진단 펌웨어 전송·검증 중 · 설정과 사진은 유지해요."):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0587,"새 CFW 전송·섹터 검증 중"),offset+count,image.length,"B");offset+=count;
            long now=System.nanoTime();if(caps.progress&&now-snapshotAt>=200_000_000L){snapshotAt=now;progress.installStatus(new InstallProgressSnapshot(session.request(0x8c,new byte[0])));}
        }
        journal.save(diagnostic?"DIAGNOSTIC_FINISH_RESULT_UNKNOWN":"PRODUCT_FINISH_RESULT_UNKNOWN");answer=session.request(0x42,NdcpSession.words(tx));
        if(answer.length!=52||u(answer,4)!=3||u(answer,8)!=tx||u(answer,16)!=image.length||!Arrays.equals(Arrays.copyOfRange(answer,20,52),digest))throw new IOException("Full physical verification failed");
        commit(journal,tx,digest,diagnostic);return true;
    }
    private void commit(InstallJournal journal,long tx,byte[] digest,boolean diagnostic)throws Exception {
        byte[] commit=new byte[40];ByteCodec.putU32le(commit,0,tx);ByteCodec.putU32le(commit,4,0x434f4d54);System.arraycopy(digest,0,commit,8,32);
        journal.save(diagnostic?"DIAGNOSTIC_COMMIT_RESULT_UNKNOWN":"PRODUCT_COMMIT_RESULT_UNKNOWN");byte[] answer=session.request(0x43,commit);
        if(answer.length!=20||u(answer,4)!=4||u(answer,8)!=tx)throw new IOException("Commit result unknown");
        journal.save(diagnostic?"DIAGNOSTIC_COMMITTED":"PRODUCT_COMMITTED");reset(journal,diagnostic);
    }
    private void reset(InstallJournal journal,boolean diagnostic)throws Exception {
        journal.save(diagnostic?"DIAGNOSTIC_RESET_RESULT_UNKNOWN":"PRODUCT_RESET_RESULT_UNKNOWN");session.request(0x47,NdcpSession.words(RecoveryBundle.number(journal.values,"transaction"),0x52535421));
        Thread.sleep(2000);journal.save(diagnostic?"WAIT_DIAGNOSTIC_BOOT":"WAIT_CFW_BOOT");
    }
}
