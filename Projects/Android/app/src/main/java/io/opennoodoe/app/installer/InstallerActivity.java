package io.opennoodoe.app.installer;

import android.app.*;
import android.content.*;
import android.net.Uri;
import android.os.*;
import android.view.*;
import android.widget.*;
import io.opennoodoe.app.NoodoeService;

/** Private local installer UI. Each state-changing action is separate from status/readback. */
public final class InstallerActivity extends Activity {
    private NoodoeService service;
    private TextView status;
    private boolean bound;
    private final ServiceConnection connection=new ServiceConnection(){
        public void onServiceConnected(ComponentName n,IBinder b){service=((NoodoeService.LocalBinder)b).getService();bound=true;}
        public void onServiceDisconnected(ComponentName n){service=null;}
    };
    @Override public void onCreate(Bundle state){
        super.onCreate(state);getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        ScrollView scroll=new ScrollView(this);LinearLayout layout=new LinearLayout(this);layout.setOrientation(LinearLayout.VERTICAL);layout.setPadding(24,24,24,24);scroll.addView(layout);
        TextView title=new TextView(this);title.setText("Noodoe CFW 설치 / 순정 복귀");title.setTextSize(22);layout.addView(title);
        status=new TextView(this);status.setText("일반 화면에서 기기를 먼저 선택/페어링하세요. 설치 모드에서는 자동 시간·설정 동기화를 중지합니다.\n전송 수락과 실제 부팅 확인은 별개입니다.\n순정 전송 이후 상시 12V를 유지하고 IGN만 OFF로 전환합니다.");status.setTextIsSelectable(true);layout.addView(status);
        add(layout,"복구 번들 ZIP 가져오기",()->startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("application/zip").addCategory(Intent.CATEGORY_OPENABLE),1));
        add(layout,"순정 식별 읽기 (번들 불필요)",()->action("stock-read-info",null));
        add(layout,"순정과 가져온 번들 호환 확인",()->action("stock-identify",null));
        add(layout,"Bootstrap 전송",()->confirm("순정 APP를 Bootstrap으로 교체할 파일을 전송합니다.","stock-install-bootstrap"));
        add(layout,"Bootstrap 상태 / 해시 읽기",()->action("ndcp-status",null));
        add(layout,"백업 → 복구 준비 → CFW 설치",()->confirm("Bootstrap에서 가운데 버튼을 길게 눌러 유지보수를 허용하세요. 같은 연결에서 전체 독립 백업·파일 생성·읽기 검증 후 CFW를 설치합니다. 폰 여유1GiB 이상이 필요하고 오래 걸립니다. IGN과 상시 전원을 유지하세요.","provision-install"));
        add(layout,"준비된 기기의 CFW 업데이트",()->confirm("보존된 백업과 기기 신원을 대조한 뒤 CFW를 검증·설치하고 재시작합니다.","update-cfw"));
        add(layout,"CFW 부팅 해시 확인",()->action("cfw-verify",null));
        add(layout,"불명 응답 대조 (읽기)",()->action("reconcile",null));
        add(layout,"순정 복구 상태",()->action("recovery-status",null));
        add(layout,"순정 복귀 · CFW 데이터 유지",()->confirm("기기에 미리 보존한 순정 이미지로 복귀합니다. 상시 전원을 유지하세요.","restore-stock"));
        add(layout,"순정 복귀 · CFW 데이터 삭제",()->confirm("삭제 도구로 재시작한 뒤 누도에서 O 2초 승인해야 삭제됩니다. 폰 로그와 순정 파일은 유지해요.","uninstall-stock"));
        add(layout,"순정 복귀 부팅 확인",()->action("stock-return-check",null));
        add(layout,"작업 기록",()->action("journal",null));
        add(layout,"백업·작업 기록 ZIP 내보내기",()->startActivityForResult(new Intent(Intent.ACTION_CREATE_DOCUMENT).setType("application/zip")
                .addCategory(Intent.CATEGORY_OPENABLE).putExtra(Intent.EXTRA_TITLE,"noodoe-recovery-evidence.zip"),2));
        add(layout,"설치 모드 종료",()->action("release",null));setContentView(scroll);
        bound=bindService(new Intent(this,NoodoeService.class),connection,BIND_AUTO_CREATE);
    }
    private void add(LinearLayout layout,String label,Runnable click){Button b=new Button(this);b.setText(label);b.setOnClickListener(v->click.run());layout.addView(b);}
    private void confirm(String text,String action){new AlertDialog.Builder(this).setMessage(text).setNegativeButton("취소",null).setPositiveButton("실행",(d,w)->action(action,null)).show();}
    private void action(String name,Uri uri){if(service==null){status.setText("서비스 연결 대기 중");return;}status.setText("진행 중: "+name);service.installerAction(name,uri,result->runOnUiThread(()->status.setText(result)));}
    @Override protected void onActivityResult(int request,int result,Intent data){super.onActivityResult(request,result,data);if(result==RESULT_OK&&data!=null){if(request==1)action("import",data.getData());else if(request==2)action("export",data.getData());}}
    @Override protected void onDestroy(){if(bound)unbindService(connection);super.onDestroy();}
}
