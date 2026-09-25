package io.opennoodoe.app.installer;
/** User recovery guidance is role-specific. Unknown results never mean cancelled. */
public final class InstallerFailure {
 private InstallerFailure(){}
 public static String rescue(String role){
  if("bootstrap".equals(role))return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0475,"Bootstrap 화면: Back to stock 선택 → O를 놓았다가 새로 2초 유지. 추가 IGN 조작은 필요 없어요.");
  if("product".equals(role))return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0476,"CFW 화면: 키 OFF → O를 약 1초 누른 채 키 ON → 2초 더 유지해 독립 복구에 진입해요.");
  if("stock".equals(role))return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0477,"순정 화면과 업데이트 안내부터 확인해 주세요. 전송 결과가 불명확하면 Bootstrap을 반복 전송하지 마세요.");
  return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0478,"기기 화면을 먼저 확인해 주세요. Bootstrap이면 Back to stock에서 O를 새로 2초 유지. CFW이면 키 OFF → O를 누른 채 키 ON → 2초 유지. 순정 화면이면 순정 업데이트 결과부터 확인해요.");
 }
 public static String explain(Throwable e,String role){
  if(e instanceof StockUpdateSession.ScreenConfirmationTimeout)return e.getMessage();
  if(e instanceof BondSession.Failure||e instanceof BootstrapConnectSession.Unavailable)
   return e.getMessage()+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0479,"\n\n이번 연결 시도에서 펌웨어 전송이나 설치 명령은 보내지 않았어요. 기존 작업 기록은 보존돼요.");
  if(e instanceof InstallJournal.StateBlocked){
   String state=((InstallJournal.StateBlocked)e).state;
   if(state.equals("STOCK_RETURN_CONFIRMED"))
    return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0480,"순정 복귀는 확인됐어요. 요청한 단계는 시작하지 않았어요. 현재 기기를 확인한 뒤 설치 도구 전송으로 새 설치를 시작하세요. 이전 기록과 복구 자료는 보관돼요.");
   if(state.equals("WAIT_CFW_BOOT")||state.equals("TRIAL_HEALTH_WAIT")||state.equals("TRIAL_CONFIRM_RESULT_UNKNOWN"))
    return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0481,"앱에는 CFW 부팅 확인 대기가 남아 있어요. 본체가 순정으로 돌아왔다면 ‘본체에서 순정으로 돌아왔어요 · 확인’을 누르세요. 실제 순정 응답을 확인한 뒤 대기를 종료하며 기록은 보관해요.");
   return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0482,"이전 작업 상태를 먼저 확인해야 해요. 요청한 단계는 시작하지 않았어요.\n기기 확인 → 마지막 작업 확인 → 이어가기 순서로 진행하세요.\n\n기록된 상태: ")+state;
  }
  String detail=e.getMessage()==null?e.getClass().getSimpleName():e.getMessage();String lower=detail.toLowerCase(java.util.Locale.ROOT);
  String cause=e instanceof NdcpSession.DeviceRejected?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0483,"기기가 명령을 거부했어요."):
   lower.contains("timed out")||lower.contains("timeout")?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0484,"정해진 시간 안에 응답을 확인하지 못했어요."):
   lower.contains("hash")||lower.contains("crc")||lower.contains("differs")||lower.contains("mismatch")?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0485,"파일 또는 기기 정보 검증이 맞지 않아요."):
   lower.contains("space")||lower.contains("journal")||lower.contains("sync")?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0486,"저장 공간이나 작업 기록을 확인해야 해요."):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0487,"현재 작업을 계속할 수 없어요.");
  return cause+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0488,"\n설치 성공·취소 여부는 아직 확정하지 않아요. 상시 전원을 유지하고 기기 화면과 작업 기록을 확인해 주세요.\n")+
   io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0489,"응답이 끊긴 작업 확인 / 진단 자료 공유를 사용할 수 있어요. 파일 생성 도중 실패했다면 설치 준비를 반복 실행하지 마세요.\n\n")+rescue(role)+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0490,"\n\n상세 정보: ")+detail;
 }
}
