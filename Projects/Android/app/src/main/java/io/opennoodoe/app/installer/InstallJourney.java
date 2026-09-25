package io.opennoodoe.app.installer;

/** Overall progress counts completed workflow stages, not estimated time or
 * bytes. Repeated files/subphases cannot move this bar backwards or finish it. */
final class InstallJourney {
 private static final String[] FIRST={io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0523,"연결·설치 선택"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0514,"저장소 배치 검사"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0524,"할당·쓰기 범위 검사"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0525,"변경 메타데이터 보관"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0526,"CFW 파일 기록·검증"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0527,"최종 FAT·파일 대조"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0517,"설치 이미지 전송"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0528,"누도에서 설치 확정"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0529,"설치·재시작"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0530,"정상 실행 확인")};
 private static final String[] STOCK={io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0531,"설치 도구 전송"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0532,"키 조작·설치 도구 부팅 확인")};
 private static final String[] UPDATE={io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0533,"업데이트 사전 검사"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0534,"APP 전송·검증"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0529,"설치·재시작"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0530,"정상 실행 확인")};
 private static final String[] UNINSTALL={io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0535,"삭제 지원·기기 확인"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0536,"삭제 도구 전송·검증"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0537,"기기 승인·정리 중 (Bluetooth 없음)"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0151,"순정 복귀 확인")};
 private String[] steps=new String[0];private int at;
 void begin(String action){at=0;steps=action.startsWith("guided-bootstrap")||action.equals("resume-install")||action.equals("provision-install")?FIRST:action.equals("stock-install-bootstrap")?STOCK:action.equals("update-cfw")?UPDATE:action.equals("uninstall-stock")?UNINSTALL:new String[0];}
 void stage(String key){
  if(key.equals("screen-confirm"))key="health";
  if(steps.length==0)return;
  int next=0;
  if(key.equals("done"))next=steps.length;
  else if(steps==UNINSTALL)next=key.equals("unconfirmed")?2:key.equals("stage")?1:0;
  else if(steps==STOCK)next=key.equals("stock-reboot")||key.equals("bootstrap-connect")?1:0;
  else if(steps==UPDATE)next=key.equals("health")?3:key.equals("reboot")?2:key.equals("stage")?1:0;
  else if(key.equals("health"))next=9;
  else if(key.equals("reboot"))next=8;
  else if(key.equals("confirm"))next=7;
  else if(key.equals("stage"))next=6;
  else if(key.equals("preserve"))next=5;
  else if(key.startsWith("file"))next=4;
  else if(key.startsWith("backup"))next=3;
  else if(key.equals("baseline"))next=2;
  else if(key.equals("audit"))next=1;
  at=Math.max(at,next);
 }
 int percent(){return steps.length==0?-1:at*100/steps.length;}
 String label(){return steps.length==0?"":String.format(java.util.Locale.ROOT,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0538,"전체 진행 · %d / %d단계 완료 (%d%%)"),at,steps.length,percent());}
 String outline(){
  if(steps.length==0)return "";
  StringBuilder b=new StringBuilder(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0539,"단계 기준이며 소요 시간 비율은 아니에요.\n"));
  for(int i=0;i<steps.length;i++){if(i>0)b.append('\n');b.append(i<at?"✓ ":i==at?"▶ ":"· ").append(i+1).append(". ").append(steps[i]);}
  return b.toString();
 }
 static String why(String key){
  if(key.equals("screen-confirm"))return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.boot_screen_waiting,"Bluetooth 연결 완료. 화면 위의 확인 버튼을 눌러야 검사가 계속돼요.");
  if(key.equals("baseline"))return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0540,"설치 전 기준값: 누도 내부에서 128MiB를 읽어 변경하지 않을 영역의 SHA-256을 계산해요. 폰으로 사진을 보내거나 덮어쓰지 않아요. 설치 후 같은 검사를 다시 대조해요.");
  if(key.equals("preserve"))return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0541,"최종 FAT와 파일 해시를 독립 계획과 대조해요. 새 설치 경로는 기존 파일·예약 영역에 쓰기 권한을 주지 않아요. 구형 Bootstrap만 전체 영역 해시를 사용해요.");
  if(key.startsWith("backup"))return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0542,"변경할 FAT·디렉터리의 원본과 계획을 보관해요. 새 Bootstrap에서는 미사용 공간이나 128MiB 전체를 내려받지 않아요.");
  if(key.startsWith("file"))return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0543,"순정 복구본·CFW 자산·설정·빈 사진 슬롯·A/B·로그 파일을 새 공간에 준비하고 실제 기록 내용을 검증해요. 기존 순정 사진은 유지해요.");
  if(key.equals("audit"))return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0544,"FAT와 디렉터리를 읽어 기존 파일·예약 영역과 충돌하지 않는 빈 공간을 확인해요.");
  if(key.equals("stage"))return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0545,"검증된 설치 이미지를 전송해요. 전송 100%만으로 설치 완료는 아니에요.");
  if(key.equals("health"))return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0546,"30초 동안 잘 동작하고 휴대폰에 다시 연결되면 완료예요.");
  return "";
 }
}
