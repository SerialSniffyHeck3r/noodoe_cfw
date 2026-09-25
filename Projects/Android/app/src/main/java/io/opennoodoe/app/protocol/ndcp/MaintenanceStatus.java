package io.opennoodoe.app.protocol.ndcp;
import io.opennoodoe.app.protocol.ByteCodec;
import java.io.IOException;
/** Read-only 0x5A, schema1. Installation policy stays in the installer. */
public final class MaintenanceStatus {
 public final long state,phase,position,total,error,flags,pairingMs;
 public MaintenanceStatus(byte[] b)throws IOException {
  if(b.length!=36||ByteCodec.u32le(b,0)!=0||ByteCodec.u32le(b,4)!=1)throw new IOException("이 설치 도구와 기기의 버전이 맞지 않아요.");
  state=ByteCodec.u32le(b,8);phase=ByteCodec.u32le(b,12);position=ByteCodec.u32le(b,16);total=ByteCodec.u32le(b,20);error=ByteCodec.u32le(b,24);flags=ByteCodec.u32le(b,28);pairingMs=ByteCodec.u32le(b,32);
  if(state>11||(flags&~15L)!=0)throw new IOException("기기의 상태를 읽지 못했어요. 다시 확인해 주세요.");
 }
 public boolean installAllowed(){return state==4&&(flags&4)!=0;}
 public String message(){if(state==11)return "누도에서 조도 센서 값을 표시하고 있어요.";String[] text={"누도를 확인하고 있어요.","설치 준비가 됐어요.","휴대폰 연결을 기다리고 있어요.","파일을 준비하고 있어요.","파일 검증이 끝났어요. 누도에서 Install CFW를 선택해 주세요.","설치 중이에요. 상시 전원을 유지해 주세요.","잠깐 멈췄어요. 연결을 다시 확인해 주세요.","Bootstrap의 Back to stock 화면에서 O를 놓았다가 새로 2초 유지해 주세요. 추가 IGN 조작은 없어요.","문제가 생겼어요. 누도의 안내에 따라 순정으로 돌아갈 수 있어요.","블루투스 테스트 중이에요. 여기서는 상태만 읽어요. 설치하려면 누도에서 Install CFW를 선택해 주세요.","누도에서 UP/DOWN으로 도움말을 보고 O로 돌아가세요."};return text[(int)state]+(total>0?"\n현재 단계: "+position+" / "+total:"")+(error!=0?String.format(java.util.Locale.ROOT,"\n확인 코드: %08X",error):"");}
}
