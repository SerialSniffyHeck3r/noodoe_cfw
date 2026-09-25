package io.opennoodoe.app.installer;

import io.opennoodoe.app.protocol.ByteCodec;
import java.io.IOException;
import java.util.Arrays;

/** Exact Product candidate handshake. A successful SPP reply is not a healthy
 * boot: only the device's durable CONFIRMED record ends this transaction. */
public final class ProductBootSession {
    public static final class TerminalFailure extends IOException {
        TerminalFailure(String message){super(message);}
    }
    public static final class Status {
        public final long sequence,state,flags,failedVersion,restoredVersion,reason,transaction;
        public final byte[] sha;
        public Status(byte[] r)throws IOException {
            if(r.length!=80||ByteCodec.u32le(r,0)!=0||ByteCodec.u32le(r,4)!=2)throw new IOException("Unsupported boot journal protocol");
            sequence=ByteCodec.u32le(r,8);state=ByteCodec.u32le(r,12);flags=ByteCodec.u32le(r,16);
            failedVersion=ByteCodec.u32le(r,28);restoredVersion=ByteCodec.u32le(r,32);reason=ByteCodec.u32le(r,36);transaction=ByteCodec.u32le(r,40);
            if(state<1||state>6||(flags&~15L)!=0)throw new IOException("Invalid boot state");
            sha=Arrays.copyOfRange(r,48,80);
        }
        public boolean confirmed(){return state==4&&(flags&9)==0;}
        // Historical failure fields intentionally survive acknowledgement.
        // Only a pending result blocks a subsequent update.
        public boolean rolledBack(){return (flags&6)!=0;}
        public String result(){return "Failed version "+failedVersion+", restored "+restoredVersion+", reason "+reason+", log "+transaction;}
    }
    private final NdcpSession ndcp;
    public ProductBootSession(NdcpSession ndcp){this.ndcp=ndcp;}
    public Status status()throws IOException{return new Status(ndcp.request(0x5b,new byte[0]));}
    /** The manual connection/result screen may see a healthy OLD release.
     * It is not a failed install and must not be required to equal a NEW ZIP.
     * Automatic post-reset confirmation below stays candidate-exact. */
    public String inspectCurrent(RecoveryBundle bundle,InstallJournal journal,StockUpdateSession.Progress progress)throws Exception {
        byte[] identity=new RoutineUpdateSession(ndcp).identifyCurrent(bundle,journal);
        Status s=status();recordRollback(s,journal);progress.role("product");
        byte[] desired=NdcpSession.hex(RecoveryBundle.sha(GateContainers.product(bundle.paddedImage("cfw"))));
        if(Arrays.equals(desired,s.sha)){
            confirm(bundle,journal,progress);return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0557,"선택한 CFW의 정상 실행·영구 확정을 확인했어요.");
        }
        if(!s.confirmed())throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0558,"기존 CFW가 아직 시험 부팅 중이에요. 그 버전을 설치한 ZIP으로 결과를 확인한 뒤 최신 ZIP을 선택하세요."));
        // Never clear a locally recorded candidate operation merely because a
        // different, previously confirmed image is still alive.
        if(journal.values.containsKey("transaction")||journal.values.containsKey("image.sha256")||
           journal.state().contains("RESULT_UNKNOWN")||journal.state().equals("WAIT_CFW_BOOT"))
            throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0559,"현재는 이전 CFW가 실행 중이에요. 새 버전 설치 완료로 처리하지 않았어요. ‘CFW 업데이트’에서 기기 작업 상태를 조회하고 이어가세요."));
        byte[] update=ndcp.status();long state=ByteCodec.u32le(update,4);
        if(state!=0&&state!=6)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0560,"현재 CFW에 진행 중인 업데이트가 있어요. ‘CFW 업데이트’에서 같은 ZIP으로 이어가세요."));
        journal.values.setProperty("current.product.sha256",BootstrapProvisioner.toHex(s.sha));
        journal.values.setProperty("current.app.sha256",BootstrapProvisioner.toHex(Arrays.copyOfRange(identity,24,56)));
        journal.save("CURRENT_CFW_CONFIRMED");
        progress.stage("connect",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0561,"기존 CFW가 정상 실행 중이에요. 선택한 새 ZIP으로 업데이트할 수 있어요."),0,0,"");
        return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0562,"기존 CFW 연결·정상 실행 확인 완료. 새 펌웨어는 아직 설치하지 않았어요. ‘CFW 업데이트’를 눌러 최신 버전으로 올리세요. APK를 구버전으로 내릴 필요가 없어요.");
    }
    public void confirm(RecoveryBundle bundle,InstallJournal journal,StockUpdateSession.Progress progress)throws Exception {
        confirm(bundle,journal,progress,System.nanoTime()+185_000_000_000L);
    }
    /** Reconnects share one deadline; reopening SPP must not restart a 3-minute wait. */
    public void confirm(RecoveryBundle bundle,InstallJournal journal,StockUpdateSession.Progress progress,long deadline)throws Exception {
        // Authenticate the target before reporting a fallback, but do not
        // require the failed candidate to still be running after rollback.
        new RoutineUpdateSession(ndcp).identify(bundle,journal);
        recordRollback(status(),journal);
        ndcp.identity(bundle,journal,2);progress.role("product");progress.stage("health",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0563,"새 버전이 잘 켜졌는지 30초 동안 확인해요."),0,0,"");
        byte[] product=Arrays.copyOfRange(bundle.paddedImage("cfw"),0x10000,0x70000);
        byte[] expected=NdcpSession.hex(RecoveryBundle.sha(product));
        boolean sent=false;
        while(System.nanoTime()<deadline){
            Status s=status();
            recordRollback(s,journal);
            if(!Arrays.equals(expected,s.sha))throw new TerminalFailure("Candidate Product SHA differs from selected package");
            if(s.confirmed()){journal.values.remove("boot.waiting.for");journal.save("CFW_CONFIRMED");progress.stage("done",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0358,"업데이트가 끝났어요. 이제 사용할 수 있어요."),1,1,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0270,"완료"));return;}
            if((s.flags&1)!=0&&!sent){
                String binding=VisualInstallConfirmation.binding(journal,s.sequence,expected);
                byte[] caps=ndcp.request(0x0d,new byte[0]);
                boolean visual=caps.length==16&&(ByteCodec.u32le(caps,12)&65536)!=0;
                long screenDeadline=deadline;
                if(visual){byte[] remaining=ndcp.request(0x9c,new byte[0]);
                    if(remaining.length!=8)throw new IOException("Invalid trial deadline");
                    long ms=ByteCodec.u32le(remaining,4);
                    if(ms==0){
                        Status ended=status();recordRollback(ended,journal);
                        // Confirmation may commit between the status snapshot
                        // and remaining-time query. Zero is not proof of failure.
                        if(Arrays.equals(ended.sha,expected)&&(ended.flags&1)==0)continue;
                        throw new TerminalFailure("Trial window expired. Query the device boot result.");
                    }
                    if(ms>180000)throw new TerminalFailure("Invalid trial deadline");
                    screenDeadline=Math.min(deadline,System.nanoTime()+ms*1000000L);
                    deadline=screenDeadline;
                }
                VisualInstallConfirmation.require(journal,progress,binding,screenDeadline,()->{
                    Status live=status();recordRollback(live,journal);
                    if(!Arrays.equals(live.sha,expected))throw new TerminalFailure("The running candidate changed while waiting for screen confirmation.");
                });
                progress.stage("health",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.boot_screen_accepted,"화면 확인을 받았어요. 기기의 정상 실행·저장 완료를 확인해요."),0,0,"");
                // A long user wait never authorizes a changed candidate.
                Status checked=status();recordRollback(checked,journal);
                // A previously delivered ACK can commit while this connection is
                // inspecting it. Finish/reset-pending is a query state, not an
                // instruction to ACK the old generation or report a failure.
                if(Arrays.equals(checked.sha,expected)&&(checked.flags&1)==0)continue;
                if(checked.sequence!=s.sequence||!Arrays.equals(checked.sha,expected)||(checked.flags&1)==0)
                    throw new TerminalFailure("The trial changed while waiting for screen confirmation.");
                byte[] ack=new byte[36];ByteCodec.putU32le(ack,0,s.sequence);System.arraycopy(expected,0,ack,4,32);
                journal.values.setProperty("boot.sequence",Long.toString(s.sequence));
                journal.save("TRIAL_CONFIRM_RESULT_UNKNOWN");if(visual)ndcp.request(0x9c,ack);ndcp.request(0x5c,ack);sent=true;journal.save("TRIAL_HEALTH_WAIT");
            }
            progress.update(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0564,"거의 다 됐어요. 새 버전을 확인하고 있어요."));Thread.sleep(500);
        }
        throw new IOException("Boot confirmation timed out; reconnect to query the result. No COMMIT or RESET was repeated.");
    }
    /** Explicit acknowledgement; failure evidence remains, no boot/reset. */
    public String acknowledge(RecoveryBundle bundle,InstallJournal journal)throws Exception {
        new RoutineUpdateSession(ndcp).identify(bundle,journal);Status before=status();
        if(!before.confirmed()||!before.rolledBack()||before.transaction==0)
            throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0565,"확인할 복귀 결과가 없거나 정상 실행 확인 중이에요."));
        journal.values.setProperty("rollback.transaction",Long.toString(before.transaction));
        journal.values.setProperty("failed.version",Long.toString(before.failedVersion));
        journal.values.setProperty("restored.version",Long.toString(before.restoredVersion));
        journal.values.setProperty("rollback.reason",Long.toString(before.reason));
        journal.save("ROLLBACK_ACK_RESULT_UNKNOWN");ndcp.request(0x61,NdcpSession.words(before.transaction));
        long deadline=System.nanoTime()+10_000_000_000L;
        while(System.nanoTime()<deadline){Status after=status();
            if(after.transaction!=before.transaction||!Arrays.equals(after.sha,before.sha))throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0566,"확인 중 부팅 상태가 바뀌었어요."));
            if(!after.rolledBack()){journal.save("ROLLBACK_ACKNOWLEDGED");return before.result();}
            Thread.sleep(100);
        }
        throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0567,"확인 결과가 아직 저장되지 않았어요. 상태를 다시 조회해 주세요."));
    }
    private static void recordRollback(Status s,InstallJournal journal)throws IOException {
        if(!s.rolledBack())return;
        journal.values.setProperty("failed.version",Long.toString(s.failedVersion));
        journal.values.setProperty("restored.version",Long.toString(s.restoredVersion));
        journal.values.setProperty("rollback.reason",Long.toString(s.reason));
        journal.values.setProperty("rollback.transaction",Long.toString(s.transaction));
        journal.save("CFW_ROLLED_BACK");throw new TerminalFailure(s.result());
    }
}
