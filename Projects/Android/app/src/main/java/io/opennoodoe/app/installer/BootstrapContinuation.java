package io.opennoodoe.app.installer;

import io.opennoodoe.app.protocol.ByteCodec;
import java.io.IOException;
import java.util.Arrays;

/** Resume only this target-bound, fully staged image. Never erase or infer a
 * successful write from a phone journal. Call after identity/UID validation. */
final class BootstrapContinuation {
    static final int NONE=0, RECHECK=1, INSTALLING=2;
    static int inspect(byte[] status, byte[] image, InstallJournal journal) throws IOException {
        if(status.length!=84)throw new IOException("Unsupported update status");
        long state=ByteCodec.u32le(status,4);
        if(state==0||state==2)return NONE;
        if(state!=1&&state!=3&&state!=4&&state!=5&&state!=6)return NONE;
        boolean complete=state==3||state==4||state==5;
        if(!complete&&ByteCodec.u32le(status,12)!=image.length)return NONE;
        String tx=journal.values.getProperty("transaction");
        byte[] sha=NdcpSession.hex(RecoveryBundle.sha(image));
        if(tx==null||ByteCodec.u32le(status,8)!=RecoveryBundle.number(journal.values,"transaction")||
           ByteCodec.u32le(status,12)!=image.length||ByteCodec.u32le(status,16)>image.length||
           (complete&&ByteCodec.u32le(status,16)!=image.length)||
           ByteCodec.u32le(status,20)!=RecoveryBundle.number(journal.values,"version")||
           ByteCodec.u32le(status,24)!=NdcpSession.crc(image,image.length)||
           (complete&&!Arrays.equals(sha,Arrays.copyOfRange(status,52,84)))||
           !RecoveryBundle.sha(image).equals(journal.values.getProperty("image.sha256")))
            throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0385,"기기에 남은 설치 파일이 이 기기·ZIP의 기록과 다릅니다. 덮어쓰지 않았어요."));
        if(ByteCodec.u32le(status,32)!=TargetBinding.version(journal))throw new IOException("Resident version changed");
        if(state==1||state==3||state==6){if(ByteCodec.u32le(status,48)!=0)throw new IOException("Boot request is unresolved");return RECHECK;}
        if(ByteCodec.u32le(status,36)!=RecoveryBundle.number(journal.values,"version")||
           ByteCodec.u32le(status,40)!=0x7f90||ByteCodec.u32le(status,44)!=image.length||
           ByteCodec.u32le(status,48)!=NdcpSession.crc(image,image.length))
            throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0386,"설치 확정 기록이 이미지와 다릅니다. 재시작 요청을 반복하지 않았어요."));
        return INSTALLING;
    }
    static void reverify(NdcpSession session, InstallJournal journal, StockUpdateSession.Progress progress)throws Exception {
        progress.stage("stage",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0387,"이미 받은 파일을 다시 읽어 검증해요. 파일은 재전송하지 않아요."),0,RecoveryBundle.APP_BYTES,"");
        journal.save("NDCP_FINISH_RESULT_UNKNOWN");
        byte[] r=session.request(0x42,NdcpSession.words(RecoveryBundle.number(journal.values,"transaction")));
        if(r.length!=52||ByteCodec.u32le(r,4)!=3||ByteCodec.u32le(r,16)!=RecoveryBundle.APP_BYTES||
           !Arrays.equals(Arrays.copyOfRange(r,20,52),NdcpSession.hex(journal.values.getProperty("image.sha256"))))
            throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0388,"남아 있는 파일의 전체 무결성 검사가 실패했어요."));
        journal.save("NDCP_VERIFIED");
    }
}
