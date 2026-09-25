package io.opennoodoe.app.maintenance;

import android.Manifest;
import android.app.*;
import android.bluetooth.*;
import android.content.*;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.*;
import android.provider.Settings;
import android.widget.*;
import java.util.*;

/** Companion home observes the single connection service. Tab/fold/theme
 * changes only affect presentation, never sockets or installation state. */
public final class InstallerHomeActivity extends Activity implements MaintenanceService.Listener {
 private SetupWizardView setupWizard,updateWizard;private LinearLayout sectionsRoot;private PermissionCenter permissionCenter;private Button permissionButton;private SetupWorkflow.View setupState;
 private BootConfirmationView bootConfirmation;private boolean resumed;
 private HomeSections sections;private LinearLayout progressBox;private Button steps,stop,record;
 private MaintenanceService service;private boolean bound,busy,confirmationShowing;
 private TextView status,phase,metrics,attention,bundleInfo,overall,reason,outline;private int normalText;private ProgressBar progressBar,overallBar;private Spinner devices;private final ArrayList<String> addresses=new ArrayList<>();
 private final ArrayList<Button> actions=new ArrayList<>();
 private final ServiceConnection connection=new ServiceConnection(){
  public void onServiceConnected(ComponentName n,IBinder b){service=((MaintenanceService.LocalBinder)b).service();service.subscribe(InstallerHomeActivity.this);restoreDeviceSelection();service.resumeAutomatic();}
  public void onServiceDisconnected(ComponentName n){service=null;renderScreenConfirmation();}
 };
 @Override protected void attachBaseContext(Context base){super.attachBaseContext(io.opennoodoe.app.AppLanguageSettings.wrap(base));}
 @Override public void onCreate(Bundle state){
  InstallerTheme.apply(this);super.onCreate(state);permissionCenter=new PermissionCenter(this);SetupWorkflow initial=new SetupWorkflow(this);initial.select(new DeviceSelections(this).address());setupState=initial.view();
  ScrollView scroll=new ScrollView(this);scroll.setBackgroundColor(InstallerTheme.background(this));LinearLayout body=new LinearLayout(this);body.setOrientation(LinearLayout.VERTICAL);
  int pad=(int)(20*getResources().getDisplayMetrics().density);body.setPadding(pad,pad,pad,pad);scroll.addView(body);
  heading(body,"FuckNudo",28);
  note(body,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0107,"연결하고, 달리고, 내 취향대로."));
  Button manual=new Button(this);manual.setText(io.opennoodoe.app.R.string.manual_title);manual.setOnClickListener(v->UserManual.open(this));body.addView(manual);
  devices=new Spinner(this);body.addView(devices);
  devices.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){
   public void onNothingSelected(AdapterView<?> p){}
   public void onItemSelected(AdapterView<?> p,android.view.View v,int at,long id){
    if(at!=devices.getSelectedItemPosition())return;
    String address=at>0&&at-1<addresses.size()?addresses.get(at-1):null;
    if(service!=null&&!service.isBusy())service.selectDevice(address);refreshBundle();
   }
  });
  permissionButton=new Button(this);permissionButton.setOnClickListener(v->permissionCenter.show());body.addView(permissionButton);
  LinearLayout connectionTools=HomeSections.fold(this,body,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0108,"기기 연결 · 앱 테마"));
  Button theme=new Button(this);theme.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0109,"앱 테마"));theme.setOnClickListener(v->theme());connectionTools.addView(theme);
  Button language=new Button(this);language.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0110,"앱 언어 · Language"));language.setOnClickListener(v->language());body.addView(language);
  add(connectionTools,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0111,"새 기기 페어링"),()->startActivity(new Intent(Settings.ACTION_BLUETOOTH_SETTINGS)));
  add(connectionTools,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0112,"기기 목록 새로고침"),this::paired);
  add(connectionTools,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0113,"동반 기기 등록 · 자동 연결"),()->io.opennoodoe.app.companion.CompanionAssociation.request(this,selectedAddress()));
  progressBox=new LinearLayout(this);progressBox.setOrientation(LinearLayout.VERTICAL);body.addView(progressBox);
  overall=new TextView(this);overall.setTextSize(19);progressBox.addView(overall);
  overallBar=new ProgressBar(this,null,android.R.attr.progressBarStyleHorizontal);overallBar.setMax(100);progressBox.addView(overallBar);
  phase=new TextView(this);phase.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0114,"설치 준비"));phase.setTextSize(21);progressBox.addView(phase);
  status=new TextView(this);status.setTextIsSelectable(true);status.setTextSize(17);status.setPadding(0,pad/2,0,pad/2);progressBox.addView(status);normalText=status.getCurrentTextColor();
  progressBar=new ProgressBar(this,null,android.R.attr.progressBarStyleHorizontal);progressBar.setMax(100);progressBox.addView(progressBar);
  metrics=new TextView(this);metrics.setTextSize(15);progressBox.addView(metrics);
  attention=new TextView(this);attention.setTextSize(17);attention.setTextIsSelectable(true);progressBox.addView(attention);
  reason=new TextView(this);reason.setTextSize(15);reason.setPadding(0,pad/2,0,pad/2);progressBox.addView(reason);
  steps=new Button(this);steps.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0115,"전체 과정·현재 위치 보기"));progressBox.addView(steps);
  outline=new TextView(this);outline.setTextSize(15);outline.setVisibility(android.view.View.GONE);progressBox.addView(outline);
  steps.setOnClickListener(v->outline.setVisibility(outline.getVisibility()==android.view.View.VISIBLE?android.view.View.GONE:android.view.View.VISIBLE));
  setupWizard=new SetupWizardView(this,this::wizardAction);body.addView(setupWizard);
  sectionsRoot=new LinearLayout(this);sectionsRoot.setOrientation(LinearLayout.VERTICAL);body.addView(sectionsRoot);
  sections=new HomeSections(this,sectionsRoot,new String[]{io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0119,"주행"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0120,"꾸미기"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0121,"설치")});
  LinearLayout ride=sections.page(0),custom=sections.page(1),install=sections.page(2);
  updateWizard=new SetupWizardView(this,this::wizardAction);install.addView(updateWizard);
  bundleInfo=new TextView(this);bundleInfo.setVisibility(android.view.View.GONE);install.addView(bundleInfo);
  add(ride,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0122,"누도와 연결"),()->run("companion",null));
  stop=new Button(this);stop.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0123,"주행 연동 중지 · 업데이트 준비"));stop.setEnabled(false);stop.setOnClickListener(v->{if(service!=null)service.disconnect();});ride.addView(stop);
  note(ride,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0124,"음악 · 알림 · GPS 궤적을 계기판과 동기화해요. 위치는 시동이 켜져 있는 동안만 전송해요."));
  heading(ride,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0125,"주행 기록"),22);
  note(ride,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0126,"폰에 기록하려면 직접 시작해 주세요. 기록을 꺼도 음악·알림·GPS 전송과 누도의 트립·정비 기록은 계속 동작해요. 연결이 끊긴 구간은 미확인으로 남겨요."));
  record=new Button(this);record.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0127,"주행 기록 시작"));record.setEnabled(false);record.setOnClickListener(v->{if(service!=null)service.recording(!service.isRecording());});ride.addView(record);
  Button history=new Button(this);history.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0128,"저장된 주행 보기"));history.setOnClickListener(v->rideHistory());ride.addView(history);
  Button replies=new Button(this);replies.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0129,"빠른 답장 5개 · 서명 · 문자 지역"));replies.setOnClickListener(v->io.opennoodoe.app.companion.ReplySettingsDialog.show(this));custom.addView(replies);
  Button contacts=new Button(this);contacts.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0130,"전화 즐겨찾기 10개 · 순서 편집"));contacts.setOnClickListener(v->io.opennoodoe.app.companion.CallFavoritesDialog.show(this));custom.addView(contacts);
  Button apps=new Button(this);apps.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0131,"전송할 알림 앱 선택"));apps.setOnClickListener(v->notificationApps());custom.addView(apps);
  Button testNotification=new Button(this);testNotification.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0132,"테스트 알림 보내기"));
  testNotification.setOnClickListener(v->new AlertDialog.Builder(this).setMessage(io.opennoodoe.app.companion.NotificationTest.send(this)).setPositiveButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0001,"확인"),null).show());custom.addView(testNotification);
  Button settings=new Button(this);settings.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0133,"기기의 현재 설정 보기 / 변경"));settings.setOnClickListener(v->settings());custom.addView(settings);
  note(custom,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0134,"사진 교체·펌웨어 업데이트 전에는 주행 연결을 종료하세요. 사진은 가운데를 정사각형으로 잘라 저장해요."));
  for(int i=0;i<3;i++){final int slot=i;add(custom,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0135,"배경 사진 ")+i+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0136," 교체"),()->document(Intent.ACTION_OPEN_DOCUMENT,"image/*",10+slot));}
  LinearLayout help=HomeSections.fold(this,body,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0137,"문제가 생겼나요? · 복구와 진단"));
  Button reset=new Button(this);reset.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0138,"연결·설치 상태 초기화"));reset.setOnClickListener(v->resetLocal());help.addView(reset);
  add(help,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0139,"순정으로 돌아가기"),this::chooseStockReturn);
  add(help,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0140,"진단 자료 공유"),()->document(Intent.ACTION_CREATE_DOCUMENT,"application/zip",3));
  note(help,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0141,"폰 연결이 없어도 Bootstrap의 Back to stock에서 O를 놓았다가 새로 2초 유지하면 복구를 확인할 수 있어요. 추가 IGN 조작은 필요 없어요."));
  LinearLayout details=new LinearLayout(this);details.setOrientation(LinearLayout.VERTICAL);details.setVisibility(android.view.View.GONE);
  Button more=new Button(this);more.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0142,"진단·복구 세부 도구 펼치기"));more.setOnClickListener(v->{boolean show=details.getVisibility()!=android.view.View.VISIBLE;details.setVisibility(show?android.view.View.VISIBLE:android.view.View.GONE);more.setText(show?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0143,"세부 도구 접기"):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0142,"진단·복구 세부 도구 펼치기"));});help.addView(more);help.addView(details);
  add(details,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0144,"순정 기기 정보 확인"),()->run("stock-read-info",null));
  add(details,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0145,"설치 파일과 순정 기기 대조"),()->run("stock-identify",null));
  add(details,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0146,"Bluetooth 8KiB 왕복 셀프 테스트"),()->run("radio-test",null));
  add(details,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0147,"Bootstrap 연결 확인만 하기"),()->run("ndcp-status",null));
  add(details,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0148,"누도 진행 상황 읽기"),()->run("bootstrap-view",null));
  add(details,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0149,"정밀 진단: 전체 NOR 이중 백업"),()->confirm(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0150,"일반 설치에는 필요하지 않아요. Bootstrap에서 128MiB를 두 번 읽어 보관·비교합니다. 시간이 오래 걸리고 폰 여유 공간 300MiB 이상이 필요해요. 상시 전원을 유지하세요."),"deep-backup"));
  add(details,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0151,"순정 복귀 확인"),()->run("stock-return-check",null));
  add(details,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0152,"별도 진단 펌웨어 설치"),()->confirm(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0153,"주행 연동을 중지한 뒤 실행하세요. 진단이 끝나면 누도에서 Back to stock → O 2초로 순정 복귀합니다. 설정과 사진은 유지하며, 지원되는 Gate가 필요해요."),"diagnostic-cfw"));
  add(details,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0154,"진단 상태 보기"),()->run("diagnostic-status",null));
  add(details,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0155,"진단 로그 가져오기"),()->run("diagnostic-log",null));
  add(details,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0156,"진단 종료 · 순정 복귀 안내"),()->run("diagnostic-return",null));
  add(details,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0157,"CFW 정상 부팅 확인"),()->run("cfw-verify",null));
  add(details,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0158,"응답이 끊긴 작업 확인"),()->run("reconcile",null));
  add(details,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0159,"기기 진단 기록 가져오기"),()->run("device-log",null));
  add(details,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0160,"복귀 결과 확인"),()->confirm(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0161,"실패한 업데이트와 이전 버전 복귀 기록을 확인했나요? 기록은 보존하고 확인 표시만 저장해요."),"rollback-ack"));
  add(details,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0162,"작업 기록 보기"),()->run("journal",null));
  add(details,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0163,"백업·작업 기록 저장"),()->document(Intent.ACTION_CREATE_DOCUMENT,"application/zip",2));
  note(details,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0164,"최초 파일 생성 도중 실패한 기록은 자동 반복하지 않습니다. 실행 중인 CFW에서 독립 Gate에 들어가려면 키 OFF → O를 약 1초 누른 채 키 ON → 2초 더 유지하세요."));
  sections.select(state==null?0:state.getInt("home.section",0));
  HomeSections.styleButtons(body,this);
  overall.setVisibility(android.view.View.GONE);overallBar.setVisibility(android.view.View.GONE);
  phase.setVisibility(android.view.View.GONE);progressBar.setVisibility(android.view.View.GONE);steps.setVisibility(android.view.View.GONE);
  status.setVisibility(android.view.View.GONE);metrics.setVisibility(android.view.View.GONE);attention.setVisibility(android.view.View.GONE);reason.setVisibility(android.view.View.GONE);
  workflow(setupState);permissionButton.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0165,"권한·연동 준비  ›"));
  LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);
  bootConfirmation=new BootConfirmationView(this,key->{if(service!=null)service.confirmScreen(key);renderScreenConfirmation();});
  root.addView(bootConfirmation);root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
  setContentView(root);bound=bindService(new Intent(this,MaintenanceService.class),connection,BIND_AUTO_CREATE);permissions();
 }
 @Override protected void onSaveInstanceState(Bundle out){super.onSaveInstanceState(out);if(sections!=null)out.putInt("home.section",sections.selected());}
 private void heading(LinearLayout box,String text,int size){TextView v=new TextView(this);v.setText(text);v.setTextSize(size);v.setPadding(0,20,0,8);box.addView(v);}
 private void note(LinearLayout box,String text){TextView v=new TextView(this);v.setText(text);v.setTextSize(15);v.setPadding(0,6,0,12);box.addView(v);}
 private void refreshBundle(){if(bundleInfo==null)return;String hash=service==null?new DeviceSelections(this).bundle(new DeviceSelections(this).address()):service.selectedBundle();if(setupWizard!=null&&setupState!=null){setupWizard.update(setupState,hash,busy);updateWizard.update(setupState,hash,busy);}
 bundleInfo.setText(hash==null?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0166,"선택한 펌웨어가 없어요."):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0167,"선택된 펌웨어: ")+hash.substring(0,Math.min(12,hash.length()))+"…");}
 private void notificationApps(){io.opennoodoe.app.companion.NotificationAppPicker.show(this);}
 private void settings(){DeviceSettingsDialog.show(this,service);}
 private void add(LinearLayout parent,String label,Runnable r){Button b=new Button(this);b.setText(label);b.setOnClickListener(v->{if(!busy)r.run();});parent.addView(b);actions.add(b);}
 private void document(String action,String type,int code){Intent i=new Intent(action).setType(type).addCategory(Intent.CATEGORY_OPENABLE);if(code>=2&&code<=4)i.putExtra(Intent.EXTRA_TITLE,code==4?"noodoe-rides.csv":code==3?"noodoe-diagnostics.zip":"noodoe-install-backup.zip");startActivityForResult(i,code);}
 private void chooseStockReturn(){
  if(busy||confirmationShowing)return;confirmationShowing=true;final int[] choice={0};final boolean[] next={false};
  AlertDialog d=new AlertDialog.Builder(this).setTitle(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0139,"순정으로 돌아가기"))
   .setSingleChoiceItems(new String[]{io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0171,"CFW 데이터 유지 (기본값)"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0172,"CFW 데이터까지 완전 삭제")},0,(v,n)->choice[0]=n)
   .setNegativeButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0004,"취소"),null).setPositiveButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0173,"다음"),(v,n)->next[0]=true).create();
  d.setOnDismissListener(v->{
    confirmationShowing=false;
    if(!next[0])return;
    confirm(choice[0]==0?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0174,"Gate에서 순정 APP만 복원해요. CFW 설정·사진·기록과 FAT는 유지합니다."):
      io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0175,"삭제 전용 펌웨어를 먼저 전송해요. 누도에서 UP/DOWN으로 Erase CFW data를 선택하고 O를 새로 2초 누르면 CFW 파일 내용을 지우고 공간을 반환합니다. 순정 사진·공장 정보와 폰 로그는 보존해요. 이 경로의 실제 전원 중단 시험은 아직 완료되지 않았어요."),choice[0]==0?"restore-stock":"uninstall-stock");
   });d.show();
 }
 private void chooseReinstall(){
  if(busy||confirmationShowing)return;confirmationShowing=true;final int[] choice={0};
  AlertDialog d=new AlertDialog.Builder(this).setTitle(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0176,"CFW 설치 · 보관 데이터"))
   .setSingleChoiceItems(new String[]{io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0177,"새로 시작 (설정·CFW 사진 초기화)"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0178,"보관 데이터 복원 (호환 기록만)")},0,(v,n)->choice[0]=n)
   .setNegativeButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0004,"취소"),null).setPositiveButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0179,"설치 계속"),(v,n)->run(choice[0]==0?"guided-bootstrap":"guided-bootstrap-restore",null)).create();
  d.setOnDismissListener(v->confirmationShowing=false);d.show();
 }
 private void confirm(String text,String action){
  if(busy||confirmationShowing||(service!=null&&service.isBusy()))return;
  confirmationShowing=true;
  AlertDialog dialog=new AlertDialog.Builder(this).setMessage(text).setNegativeButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0180,"돌아가기"),null).setPositiveButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0181,"계속"),(d,w)->run(action,null)).create();
  dialog.setOnDismissListener(d->confirmationShowing=false);dialog.show();
 }
 private void run(String action,Uri uri){
  if(service==null){showStatus(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0182,"잠깐만요. 연결 준비 중이에요."));return;}
  if(busy||service.isBusy())return;
  if(sections!=null&&(action.equals("import")||action.equals("guided-bootstrap")||action.equals("update-cfw")||action.equals("stock-install-bootstrap")))sections.select(2);
  service.run(action,selectedAddress(),uri);
  // Lock locally before the next tap, not after an asynchronous status callback.
  if(service.isBusy())changed(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0183,"작업을 시작했어요. 진행 상황을 확인하고 있어요."),true);
 }
 private String selectedAddress(){int at=devices.getSelectedItemPosition()-1;return at>=0&&at<addresses.size()?addresses.get(at):null;}
 private void permissions(){
  if(Build.VERSION.SDK_INT>=31&&(checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)!=PackageManager.PERMISSION_GRANTED||checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN)!=PackageManager.PERMISSION_GRANTED))
   requestPermissions(new String[]{Manifest.permission.BLUETOOTH_CONNECT,Manifest.permission.BLUETOOTH_SCAN},8);
  else paired();
 }
 @SuppressWarnings("deprecation") private void paired(){
  try{BluetoothAdapter bt=BluetoothAdapter.getDefaultAdapter();ArrayList<String> names=new ArrayList<>();names.add(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0184,"연결할 누도를 선택하세요"));addresses.clear();
   if(bt!=null&&bt.isEnabled()){ArrayList<BluetoothDevice> list=new ArrayList<>(bt.getBondedDevices());Collections.sort(list,(a,b)->a.getAddress().compareTo(b.getAddress()));for(BluetoothDevice d:list){addresses.add(d.getAddress());names.add((d.getName()==null?"Bluetooth":d.getName())+" · "+d.getAddress());}}
   // A removed bond must not erase the selected installation target or ZIP.
   // Keep that exact address available so createBond can resume its connection.
   String saved=service==null?new DeviceSelections(this).address():service.selectedDevice();
   if(saved!=null&&BluetoothAdapter.checkBluetoothAddress(saved)&&!addresses.contains(saved)){addresses.add(saved);names.add(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0185,"선택한 누도 · 페어링 다시 연결 · ")+saved);}
   devices.setAdapter(new ArrayAdapter<>(this,android.R.layout.simple_spinner_dropdown_item,names));restoreDeviceSelection();
  }catch(SecurityException e){showStatus(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0186,"근처 기기 권한을 허용하면 누도를 고를 수 있어요."));}
 }
 @Override public void onRequestPermissionsResult(int code,String[] permissions,int[] results){super.onRequestPermissionsResult(code,permissions,results);paired();refreshPermissions();if(results.length>0){boolean denied=false;for(int r:results)denied|=r!=PackageManager.PERMISSION_GRANTED;if(denied)Toast.makeText(this,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0187,"허용하지 않은 기능은 꺼져 있어요. 권한·연동 준비에서 다시 설정할 수 있어요."),Toast.LENGTH_LONG).show();}}
 @Override protected void onActivityResult(int request,int result,Intent data){super.onActivityResult(request,result,data);
  if(request==io.opennoodoe.app.companion.CallFavoritesDialog.REQUEST){if(result==RESULT_OK&&data!=null)io.opennoodoe.app.companion.CallFavoritesDialog.picked(this,data.getData());return;}
  if(request==io.opennoodoe.app.companion.CompanionAssociation.REQUEST){if(result==RESULT_OK)try{io.opennoodoe.app.companion.CompanionAssociation.observe(this,selectedAddress());if(service!=null)service.resumeAutomatic();}catch(RuntimeException e){showStatus(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0188,"동반 기기 등록은 완료됐지만 자동 연결 권한을 확인해야 해요."));}return;}if(result==RESULT_OK&&data!=null){if(request==4){if(service!=null)service.exportRides(data.getData());}else if(request>=10&&request<=12)run("photo"+(request-10),data.getData());else if(request>=1&&request<=3)run(request==1?"import":request==3?"diagnostics":"export",data.getData());}}
 private void showStatus(String text){status.setText(text);status.setVisibility(text==null||text.isEmpty()?android.view.View.GONE:android.view.View.VISIBLE);}
 @Override public void changed(String text,boolean running){busy=running;showStatus(text);refreshBundle();devices.setEnabled(!running);for(Button b:actions)b.setEnabled(!running);if(stop!=null)stop.setEnabled(service!=null&&(!running||service.isCompanionRunning()));if(record!=null){record.setEnabled(service!=null&&service.isCompanionRunning());record.setText(service!=null&&service.isRecording()?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0189,"주행 기록 종료"):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0127,"주행 기록 시작"));}renderWorkflow();renderScreenConfirmation();}
 @Override public void progress(io.opennoodoe.app.installer.InstallerPresentation.Snapshot s){
  renderScreenConfirmation();
  changed(s.text,s.running);phase.setText(s.title);
  boolean transfer=s.overallPercent>=0;progressBar.setVisibility(transfer?android.view.View.VISIBLE:android.view.View.GONE);
  steps.setVisibility(transfer?android.view.View.VISIBLE:android.view.View.GONE);if(!transfer)outline.setVisibility(android.view.View.GONE);
  phase.setVisibility(transfer?android.view.View.VISIBLE:android.view.View.GONE);metrics.setVisibility(transfer?android.view.View.VISIBLE:android.view.View.GONE);reason.setVisibility(transfer?android.view.View.VISIBLE:android.view.View.GONE);progressBar.setIndeterminate(s.running&&s.percent<0);progressBar.setProgress(Math.max(0,s.percent));
  overall.setText(s.overall);overallBar.setProgress(Math.max(0,s.overallPercent));
  overall.setVisibility(s.overallPercent<0?android.view.View.GONE:android.view.View.VISIBLE);overallBar.setVisibility(overall.getVisibility());
  reason.setText(s.why);outline.setText(s.outline);
  metrics.setText(s.counts+"\n"+s.timing+(s.rate.isEmpty()?"":"\n"+s.rate));attention.setText(s.notice);
  attention.setVisibility(s.notice.isEmpty()?android.view.View.GONE:android.view.View.VISIBLE);
  status.setTextColor(s.error?InstallerTheme.error(this):normalText);attention.setTextColor(InstallerTheme.warning(this));
 }
 @Override public void workflow(SetupWorkflow.View view){setupState=view;renderWorkflow();}
 private void renderWorkflow(){
  if(setupState==null||sectionsRoot==null)return;
  boolean normal=setupState.role.equals("product")&&!setupState.recovery&&(!busy||service!=null&&service.isCompanionRunning());
  sectionsRoot.setVisibility(normal?android.view.View.VISIBLE:android.view.View.GONE);setupWizard.setVisibility(normal?android.view.View.GONE:android.view.View.VISIBLE);
  refreshBundle();
 }
 private void wizardAction(String action){
  if(busy)return;
  switch(action){
   case "choose":startActivity(new Intent(Settings.ACTION_BLUETOOTH_SETTINGS));break;
   case "permissions":permissionCenter.show();break;
   case "pick":document(Intent.ACTION_OPEN_DOCUMENT,"application/zip",1);break;
   case "diagnostics":document(Intent.ACTION_CREATE_DOCUMENT,"application/zip",3);break;
   case "reset":resetLocal();break;
   case "stock-install-bootstrap":confirm(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0193,"순정에 설치 도구를 전송할까요? 누도와 ZIP의 호환성을 먼저 검사해요. 전송 후에는 누도 화면의 키 조작 안내를 따라 주세요."),action);break;
   case "guided-bootstrap":chooseReinstall();break;
   case "update-cfw":confirm(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0194,"새 CFW를 전송할까요? 정상 실행 확인 후 설정·CFW 사진을 초기화해요. 실패 롤백 시 이전 상태를 유지해요."),action);break;
   case "restore-stock":confirm(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0195,"CFW 데이터는 유지한 채 순정으로 돌아갈까요? 복구 계층이 다른 구버전은 순정 복귀 후 최신 ZIP의 Bootstrap으로 갱신할 수 있어요. 일반 CFW 업데이트가 가능한 기기는 이 과정이 필요 없어요."),action);break;
   default:run(action,null);
  }
 }
 private void rideHistory(){
  try(io.opennoodoe.app.companion.RideHistory db=new io.opennoodoe.app.companion.RideHistory(this)){
   List<String> rows=db.recent();String text=rows.isEmpty()?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0196,"아직 저장된 기록이 없어요. 주행 연결 후 ‘주행 기록 시작’을 누르면 저장해요."):android.text.TextUtils.join("\n\n",rows);
   TextView view=new TextView(this);view.setText(text);view.setTextSize(16);view.setPadding(24,16,24,16);ScrollView scroll=new ScrollView(this);scroll.addView(view);
   new AlertDialog.Builder(this).setTitle(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0197,"주행 기록 · 최근 50개 구간")).setView(scroll).setPositiveButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0009,"닫기"),null).setNeutralButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0198,"전체 CSV 내보내기"),(d,w)->document(Intent.ACTION_CREATE_DOCUMENT,"text/csv",4)).show();
  }catch(RuntimeException e){showStatus(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0199,"주행 기록을 열지 못했어요. 폰의 저장 공간을 확인해 주세요."));}
 }
 private void refreshPermissions(){if(permissionButton!=null)permissionButton.setContentDescription(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0200,"권한·연동 준비. ")+permissionCenter.summary());if(service!=null)service.refreshPermissions();}
 private void renderScreenConfirmation(){
  if(bootConfirmation==null)return;boolean ready=service!=null&&service.canConfirmScreen();
  bootConfirmation.render(ready,ready?service.screenConfirmationKey():"",ready?service.screenSeconds():0,resumed);
  if(ready)getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
  else getWindow().clearFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
 }
 @Override protected void onResume(){super.onResume();resumed=true;if(devices!=null&&!busy)paired();refreshPermissions();renderScreenConfirmation();}
 @Override protected void onPause(){resumed=false;if(bootConfirmation!=null)bootConfirmation.pause();super.onPause();}
 private void restoreDeviceSelection(){
  String address=service==null?new DeviceSelections(this).address():service.selectedDevice();
  int i=address==null?-1:addresses.indexOf(address);devices.setSelection(i+1);
 }
 private void theme(){
  new AlertDialog.Builder(this).setTitle(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0201,"화면 테마")).setSingleChoiceItems(new String[]{io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0202,"시스템 설정 따르기"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0087,"라이트"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0086,"다크")},io.opennoodoe.app.UiThemeSettings.mode(this),(d,index)->{
   io.opennoodoe.app.UiThemeSettings.setMode(this,index);d.dismiss();recreate();
  }).setNegativeButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0009,"닫기"),null).show();
 }
 private void language(){
  new AlertDialog.Builder(this).setTitle(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0110,"앱 언어 · Language")).setSingleChoiceItems(new String[]{io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0202,"시스템 설정 따르기"),"English","繁體中文（台灣）","简体中文",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0203,"한국어"),"日本語","Español"},io.opennoodoe.app.AppLanguageSettings.position(this),(d,index)->{
   io.opennoodoe.app.AppLanguageSettings.set(this,io.opennoodoe.app.AppLanguageSettings.tagAt(index));d.dismiss();recreate();
  }).setNegativeButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0009,"닫기"),null).show();
 }
 private void resetLocal(){
  new AlertDialog.Builder(this).setTitle(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0138,"연결·설치 상태 초기화")).setMessage(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0204,"앱의 연결과 현재 작업 선택을 초기화합니다. 누도의 펌웨어·설정과 기존 로그·복구 자료는 삭제하지 않습니다.\n\n기기의 설치 취소 명령은 보내지 않습니다. 결과를 받지 못한 작업은 다시 연결해 먼저 조회합니다."))
   .setNegativeButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0180,"돌아가기"),null).setPositiveButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0205,"앱 상태 초기화"),(d,w)->{if(service!=null)service.resetLocal();}).show();
 }
 @Override public void selectionCleared(){devices.setSelection(0);refreshBundle();phase.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0206,"기기를 다시 선택해 주세요"));overall.setText("");reason.setText("");outline.setText("");metrics.setText("");attention.setText("");progressBar.setProgress(0);overallBar.setProgress(0);}
 @Override protected void onDestroy(){if(bootConfirmation!=null)bootConfirmation.dismiss();if(permissionCenter!=null)permissionCenter.dismiss();if(service!=null)service.unsubscribe(this);if(bound)unbindService(connection);super.onDestroy();}
}
