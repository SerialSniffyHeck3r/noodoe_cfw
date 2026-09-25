package io.opennoodoe.app.installer;

import io.opennoodoe.app.protocol.ByteCodec;
import io.opennoodoe.app.protocol.DeviceInfo;
import io.opennoodoe.app.protocol.ndcp.NdcpClient;
import java.io.IOException;

/** Read-only discovery. A failed NDCP probe is never evidence of stock firmware:
 * stock must answer its own identity exchange on a fresh connection. */
public final class DeviceProbe {
 public static int role(byte[] reply)throws IOException {
  if(reply.length!=88||ByteCodec.u32le(reply,0)!=0||ByteCodec.u32le(reply,4)!=1)
   throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0429,"기기 식별 응답 형식이 맞지 않아요."));
  int role=(int)ByteCodec.u32le(reply,8);
  if(role!=1&&role!=2&&role!=3)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0430,"지원하지 않는 기기 역할이에요: ")+role);
  return role;
 }
 public static String run(InstallerController.Connections connections,StockUpdateSession.Progress progress)throws Exception {
  IOException ndcpFailure;
  boolean spokeNdcp=false;
  try(InstallerTransport stream=connections.open()) {
   NdcpClient client=new NdcpClient(stream,3000);long end=System.nanoTime()+15_000_000_000L;
   for(;;) {
    try {
     byte[] identity=client.request(0x58,new byte[0]);spokeNdcp=true;
     int role=role(identity);progress.role(role==1?"bootstrap":role==3?"diagnostic":"product");progress.reply();
     return role==3?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0431,"진단 펌웨어가 연결됐어요. 본체에서 시험하고, 끝나면 Back to stock을 선택하세요."):role==1?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0432,"설치 도구가 연결됐어요. 사용했던 설치 ZIP을 선택하면 이어갈 수 있어요."):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0433,"CFW가 연결됐어요. 주행 연동을 시작할 수 있어요.");
    }catch(NdcpClient.DeviceRejected e) {
     spokeNdcp=true;if((e.result!=2&&e.result!=3)||System.nanoTime()>=end)throw e;
     progress.update(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0434,"기기가 시작 중이에요. 식별값을 기다리고 있어요."));Thread.sleep(200);
    }
   }
  }catch(IOException e){if(spokeNdcp)throw e;ndcpFailure=e;}
  progress.update(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0435,"순정 응답도 확인하고 있어요. 펌웨어에는 쓰지 않아요."));
  try(InstallerTransport stream=connections.open()) {
   DeviceInfo info=new StockUpdateSession(stream,progress).identify();
   if(info.status!=0)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0436,"순정 기기 정보 요청이 거절됐어요."));
   progress.role("stock");progress.reply();return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0437,"순정 Noodoe가 연결됐어요. 설치 마법사를 시작할 수 있어요.");
  }catch(Exception stockFailure){stockFailure.addSuppressed(ndcpFailure);throw stockFailure;}
 }
}
