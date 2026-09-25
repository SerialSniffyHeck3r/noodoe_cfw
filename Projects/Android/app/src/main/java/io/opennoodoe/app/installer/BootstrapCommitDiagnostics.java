package io.opennoodoe.app.installer;

import io.opennoodoe.app.protocol.ByteCodec;
import java.io.IOException;

/** Read-only latched diagnostics. Never permits retry, reset or a journal-state promotion. */
public final class BootstrapCommitDiagnostics {
    public final long phase,result,detail,destructive,writer,resume,state,failure,uncertain,recovery;
    public BootstrapCommitDiagnostics(byte[] b)throws IOException {
        if(b.length!=48||u(b,0)!=0||u(b,4)!=1||u(b,8)>8||u(b,20)>1||u(b,40)>1)
            throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0363,"설치 오류 상세 정보 형식이 맞지 않아요."));
        phase=u(b,8);result=u(b,12);detail=u(b,16);destructive=u(b,20);
        writer=u(b,24);resume=u(b,28);state=u(b,32);failure=u(b,36);uncertain=u(b,40);recovery=u(b,44);
    }
    private static long u(byte[] b,int p){return ByteCodec.u32le(b,p);}
    public String summary(){
        String[] step={io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0364,"기록 전"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0365,"권한·이미지 검사"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0366,"복구본·CFW 일치 검사"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0367,"Bluetooth 일시 정지"),
            io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0368,"연결 상태 재확인"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0369,"태스크·워치독 확인"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0370,"설치 요청 기록"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0371,"Bluetooth 재개"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0372,"기록 확인 완료")};
        return step[(int)phase]+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0373,"에서 중단됐어요. 단계=")+phase+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0374,", 결과=")+result+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0375,", 상세=")+detail+
            io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0376,", 기록=")+writer+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0377,", BT 재개=")+resume+". "+
            (destructive==0&&uncertain==0?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0378,"설치 요청 기록은 시작되지 않았어요. 기기에서 O를 눌러 순정 복구로 이동할 수 있어요."):
             io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0379,"설치 요청의 기록 여부를 확인해야 해요. 자동 재전송하지 않았어요."));
    }
    public void record(InstallJournal j)throws IOException {
        long[] values={phase,result,detail,destructive,writer,resume,state,failure,uncertain,recovery};
        String[] names={"phase","result","detail","destructive","writer","resume","state","failure","uncertain","recovery"};
        for(int i=0;i<values.length;i++)j.values.setProperty("commit.diagnostic."+names[i],Long.toString(values[i]));
        j.save(j.state());
        j.recordCommitDiagnostic(values);
    }
}
