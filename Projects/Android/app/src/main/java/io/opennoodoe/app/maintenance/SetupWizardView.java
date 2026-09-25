package io.opennoodoe.app.maintenance;

import android.app.Activity;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.widget.*;
import io.opennoodoe.app.UiThemeSettings;

/** One decision per wizard step. Routes intent only; device and transaction
 * validation remain in InstallerController. Re-rendering cannot send a command. */
final class SetupWizardView extends LinearLayout {
 interface Actions {void action(String action);}
 private final Activity host;private final Actions actions;private String rendered="";
 SetupWizardView(Activity host,Actions actions){super(host);this.host=host;this.actions=actions;setOrientation(VERTICAL);int p=dp(18);setPadding(p,p,p,p);
  GradientDrawable bg=new GradientDrawable();bg.setColor(UiThemeSettings.isDark(host)?0xff1d2823:0xffe5eee8);bg.setCornerRadius(dp(20));setBackground(bg);
 }
 private int dp(int v){return Math.round(v*getResources().getDisplayMetrics().density);}
 void update(SetupWorkflow.View s,String bundle,boolean busy){
  String key=s.address+"|"+s.role+"|"+s.operation+"|"+s.recovery+"|"+s.error+"|"+bundle+"|"+busy;
  if(key.equals(rendered))return;rendered=key;removeAllViews();
  if(busy){text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0277,"진행 중"),13,false);text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0278,"지금 단계가 끝나길 기다려 주세요"),22,true);text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0279,"위의 전체·현재 진행률에 기기에서 확인한 결과가 표시돼요. 연결이 끊겨도 버튼을 반복해서 누르지 않아도 돼요."),16,false);return;}
  if(s.role.equals("diagnostic")){
   text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0280,"진단 모드"),24,true);text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0281,"시험을 마치면 순정으로 돌아가세요"),18,true);
   text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0282,"본체 UP/DOWN으로 항목을 고르고 O로 실행하세요. 진단 펌웨어는 주행 모드가 아니며 설정·사진을 초기화하지 않아요."),16,false);
   if(s.recovery&&!s.error.isEmpty())text(s.error,14,false);
   button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0283,"현재 진단 결과 읽기"),"diagnostic-status",true);
   button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0284,"진단 로그 저장"),"diagnostic-log",false);
   button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0285,"본체에서 순정 복귀 확인"),"diagnostic-return",false);
   button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0286,"순정으로 돌아왔는지 확인"),"stock-return-check",false);
   text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0287,"연결되지 않아도 본체 Back to stock → O를 새로 2초 누르면 복귀할 수 있어요."),14,false);
  }else if(s.recovery){
   text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0288,"문제 해결 마법사"),24,true);text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0289,"먼저 어디까지 끝났는지 확인해요"),18,true);
   text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0290,"설치가 멈췄다고 처음부터 보내지 않아요. 기기 확인 → 마지막 작업 확인 → 이어가기 순서로 진행해요. 기록과 복구 자료는 그대로 보관돼요."),16,false);
   if(!s.error.isEmpty())text(s.error.length()>360?s.error.substring(0,360)+"…":s.error,14,false);
   button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0291,"1. 현재 기기 확인"),s.address==null?"choose":"inspect-device",true);
   if(s.address!=null&&bundle!=null&&!s.role.equals("product"))button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0292,"본체가 Bootstrap이면 · 자동 연결"),"bootstrap-connect",true);
   // A button-only Gate restore happens outside the phone's last known role.
   // Make the read-only stock check reachable even with stale Product/unknown UI.
   if(s.address!=null&&bundle!=null&&!s.role.equals("stock"))
    button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0293,"본체에서 순정으로 돌아왔어요 · 확인"),"stock-return-check",false);
   if(!s.role.equals("unknown")){
    text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0294,"확인된 화면 · ")+role(s.role),15,true);
    if(bundle==null)button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0295,"2. 사용한 설치 ZIP 선택"),"pick",false);
    else {
     if(s.role.equals("product")){
      button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0296,"2. 현재 CFW / 설치 결과 확인"),"cfw-verify",false);
      text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0297,"기존 CFW에 최신 ZIP을 선택해도 괜찮아요. 앱을 구버전으로 내리지 않고 아래에서 업데이트하거나 중단된 전송을 이어가세요."),14,false);
      button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0298,"3. CFW 업데이트 / 이어가기"),"update-cfw",true);
      button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0299,"다른 업데이트 ZIP 선택"),"pick",false);
      button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0300,"복구 계층이 다른 기기: 순정 복귀 후 갱신"),"restore-stock",false);
     }
     else if(s.role.equals("stock"))button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0301,"2. 순정 복귀 확인"),"stock-return-check",false);
     else button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0302,"2. 마지막 작업 상태 조회"),"reconcile",false);
     if(!s.role.equals("product"))button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0303,"3. 검사 후 설치 이어가기"),s.role.equals("stock")?"stock-install-bootstrap":"guided-bootstrap",false);
    }
   }
   button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0304,"진단 자료 저장·공유"),"diagnostics",false);button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0305,"연결·선택 다시 설정"),"reset",false);
   text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0306,"누도에서 복구하려면"),16,true);text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0307,"Bootstrap: Back to stock에서 O를 놓았다가 새로 2초 유지하세요. CFW 긴급 복구: 키 OFF → O 1초 유지 → 누른 채 키 ON → 2초 유지. 기기 화면의 진행 안내를 따라 주세요."),14,false);
  }else if(s.address==null){
   text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0308,"설치 마법사 · 1 / 4"),13,false);text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0309,"누도를 연결해 볼까요?"),24,true);text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0310,"위에서 페어링한 누도를 선택하세요. 설치 여부는 앱이 직접 확인해요."),16,false);button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0311,"누도 페어링하기"),"choose",true);button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0312,"필요한 권한 확인"),"permissions",false);
  }else if(s.role.equals("unknown")){
   text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0308,"설치 마법사 · 1 / 4"),13,false);text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0313,"기기 상태를 확인해요"),24,true);
   text(s.operation.equals("stock-install-bootstrap")?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0314,"Bootstrap이 뜨면 본체에서 Install CFW를 선택하세요. 앱이 페어링을 시작하고 연결을 확인해요. 설치 파일을 다시 보내지 않아요."):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0315,"순정, 설치 도구, CFW 중 무엇이 실행 중인지 읽어서 다음 단계를 안내해요. 이 확인은 펌웨어를 바꾸지 않아요."),16,false);
   if(bundle!=null)button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0316,"Bootstrap 자동 연결"),"bootstrap-connect",true);
   button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0317,"기기 확인하고 계속"),"inspect-device",true);
  }else if(bundle==null){
   text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0318,"설치 마법사 · 2 / 4"),13,false);text(s.role.equals("product")?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0319,"업데이트 파일 선택"):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0320,"설치 파일 선택"),24,true);text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0321,"확인된 기기 · ")+role(s.role),15,true);
   text(s.role.equals("bootstrap")?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0322,"이 설치 도구를 올릴 때 쓴 ZIP을 선택하세요. 다른 버전이면 파일 일치 검사에서 안내해요."):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0323,"다운로드한 installer.zip을 선택하세요. 파일 무결성과 기기 호환성은 전송 전에 검사해요."),16,false);
   button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0324,"ZIP 선택"),"pick",true);button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0325,"기기 다시 확인"),"inspect-device",false);
  }else{
   boolean product=s.role.equals("product"),bootstrap=s.role.equals("bootstrap");
   text(product?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0326,"CFW 업데이트 · 준비"):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0327,"설치 마법사 · 3 / 4"),13,false);
   text(product?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0328,"새 버전으로 업데이트"):bootstrap?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0329,"CFW 설치를 이어가요"):s.operation.equals("stock-return-check")?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0330,"순정에서 다시 설치해요"):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0331,"설치 도구를 준비해요"),24,true);
   text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0332,"기기 · ")+role(s.role)+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0333,"\n파일 · ")+bundle.substring(0,Math.min(12,bundle.length()))+"…",15,false);
   text(product?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0334,"APP 전송 → 재부팅 → 정상 실행 확인. 동일한 자산은 다시 보내지 않아요."):bootstrap?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0335,"누도의 Install CFW를 선택한 뒤 시작하세요. 파일 준비·검증을 진행하고 마지막 승인은 누도에서 받아요."):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0336,"먼저 작은 설치 도구를 보내요. 누도에 Bootstrap이 표시되면 다시 기기를 확인하고 CFW 설치를 이어가요."),16,false);
   text(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0337,"상시 전원을 유지해 주세요. 정상 확정 시 CFW 설정·사진 슬롯이 초기화돼요. 순정 사진과 복구 자료는 보존해요."),14,false);
   button(product?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0338,"업데이트 확인"):bootstrap?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0179,"설치 계속"):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0339,"설치 도구 전송 확인"),product?"update-cfw":bootstrap?"guided-bootstrap":"stock-install-bootstrap",true);
   button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0340,"다른 ZIP 선택"),"pick",false);button(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0325,"기기 다시 확인"),"inspect-device",false);
  }
  HomeSections.styleButtons(this,host);
 }
 private static String role(String r){return r.equals("stock")?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0341,"순정 Noodoe"):r.equals("bootstrap")?"FuckNudo Bootstrap":"FuckNudo CFW";}
 private void text(String value,int size,boolean bold){TextView t=new TextView(host);t.setText(value);t.setTextSize(size);t.setTextColor(UiThemeSettings.isDark(host)?(size>=18?0xffedf5f0:0xffb5c6bc):(size>=18?0xff182d23:0xff475e50));t.setPadding(0,dp(4),0,dp(10));if(bold)t.setTypeface(null,Typeface.BOLD);addView(t);}
 private void button(String label,String action,boolean primary){Button b=new Button(host);b.setText(label);b.setSelected(primary);b.setOnClickListener(v->actions.action(action));addView(b,new LayoutParams(-1,-2));}
}
