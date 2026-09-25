package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import java.io.IOException;
/** Read-only snapshot on the SAME serialized connection, never a second socket. */
public final class BootstrapProgress {
 private final NdcpSession session;private final StockUpdateSession.Progress progress;private long sampled;private View latest;
 public BootstrapProgress(NdcpSession s,StockUpdateSession.Progress p){session=s;progress=p;}
 public static final class View {
  public final long state,phase,subphase,kind,position,total,error,heartbeat,uptime,idle,requests,replies,flags,epoch;
  public View(byte[] b)throws IOException{
   if(b.length!=64||u(b,0)!=0||u(b,4)!=2||u(b,8)>10||(u(b,56)&~31L)!=0)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0399,"설치 상태 정보 형식이 맞지 않아요."));
   state=u(b,8);phase=u(b,12);subphase=u(b,16);kind=u(b,20);position=u(b,24);total=u(b,28);error=u(b,32);heartbeat=u(b,36);uptime=u(b,40);idle=u(b,44);requests=u(b,48);replies=u(b,52);flags=u(b,56);epoch=u(b,60);
  }
  private static long u(byte[] b,int at){return ByteCodec.u32le(b,at);}
  public String text(){
   String[] file={io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0400,"폰트·BT 자산"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0401,"사용자 설정"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0125,"주행 기록"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0402,"빈 사진 슬롯"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0403,"순정 복구본"),"CFW A","CFW B",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0404,"부팅 기록"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0405,"진단 기록")};
   String label=phase==12?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0406,"저장소 할당 검사"):phase==13?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0407,"기존 데이터 보존 해시 검사"):phase==7?(subphase>=4?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0408,"파일 할당 정보 확인"):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0409,"기록 후 실제 저장 내용 확인")):phase==4?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0410,"수신 파일 검증"):phase==3?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0411,"파일 수신"):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0412,"기기 상태 확인");
   return label+(phase!=12&&phase!=13&&kind<9?" · "+file[(int)kind]:"")+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0413,"\n기기 응답 정상 · 최근 진행 변화 ")+idle/1000+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0414,"초 전")+((flags&1)!=0?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0415,"\n기기에서도 진행 정체를 감지했어요. 자동 재전송하지 않아요."):"");
  }
 }
 public void poll()throws IOException{poll(null);}
 View poll(String context)throws IOException{return poll(context,System.nanoTime()/1_000_000L);}
 View poll(String context,long now)throws IOException{
  if(latest!=null&&now-sampled<750)return null;sampled=now;
  View v=new View(session.request(0x84,new byte[0]));progress.reply();progress.role("bootstrap");progress.deviceStatus(v);
  if((v.flags&16)!=0)progress.installStatus(new InstallProgressSnapshot(session.request(0x8c,new byte[0])));
  if(v.state==8||v.error!=0)throw new IOException(String.format(java.util.Locale.ROOT,"Bootstrap error %08X (phase %d.%d, file %d)",v.error,v.phase,v.subphase,v.kind));
  latest=v;
  String key=context!=null?context:v.phase==13?"preserve":v.phase==12?"audit":"file-"+v.kind+"-"+v.phase+"-"+v.subphase;
  progress.stage(key,v.text(),v.position,v.total,"B");return v;
 }
}
