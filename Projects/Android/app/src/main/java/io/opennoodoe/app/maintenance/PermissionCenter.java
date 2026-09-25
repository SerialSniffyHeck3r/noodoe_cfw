package io.opennoodoe.app.maintenance;

import android.Manifest;
import android.app.*;
import android.content.*;
import android.content.pm.PackageManager;
import android.location.LocationManager;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import android.widget.*;
import io.opennoodoe.app.companion.CompanionNotifications;

/** Android permission requests stay user-mediated. Music needs notification
 * access, not microphone or blanket media-file permission. */
final class PermissionCenter {
 private final Activity host;private AlertDialog dialog;
 PermissionCenter(Activity activity){host=activity;}
 boolean granted(String permission){return host.checkSelfPermission(permission)==PackageManager.PERMISSION_GRANTED;}
 boolean notifications(){String raw=Settings.Secure.getString(host.getContentResolver(),"enabled_notification_listeners");if(raw==null)return false;
  ComponentName expected=new ComponentName(host,CompanionNotifications.class);
  for(String part:raw.split(":"))if(expected.equals(ComponentName.unflattenFromString(part)))return true;return false;}
 String summary(){boolean bt=Build.VERSION.SDK_INT<31||granted(Manifest.permission.BLUETOOTH_CONNECT)&&granted(Manifest.permission.BLUETOOTH_SCAN);
  return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0249,"기기 연결 ")+mark(bt)+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0250," · 음악·알림 ")+mark(notifications())+" · GPS "+mark(granted(Manifest.permission.ACCESS_FINE_LOCATION));}
 private String mark(boolean ok){return ok?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0251,"준비됨"):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0252,"설정 필요");}
 void show(){
  if(dialog!=null)dialog.dismiss();LinearLayout box=new LinearLayout(host);box.setOrientation(LinearLayout.VERTICAL);int p=Math.round(20*host.getResources().getDisplayMetrics().density);box.setPadding(p,p,p,p);
  TextView intro=new TextView(host);intro.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0253,"필요한 항목을 눌러 Android에서 허용해 주세요. 허용한 기능부터 사용할 수 있어요. 음악은 알림 접근으로 읽으며 마이크 권한은 쓰지 않아요."));box.addView(intro);
  row(box,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0254,"근처 기기 · Bluetooth"),Build.VERSION.SDK_INT<31||granted(Manifest.permission.BLUETOOTH_CONNECT)&&granted(Manifest.permission.BLUETOOTH_SCAN),()->{
   if(Build.VERSION.SDK_INT>=31)request(new String[]{Manifest.permission.BLUETOOTH_CONNECT,Manifest.permission.BLUETOOTH_SCAN},8);else open(new Intent(Settings.ACTION_BLUETOOTH_SETTINGS));});
  row(box,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0255,"음악 정보·휴대폰 알림"),notifications(),()->{
   Intent i=new Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS);
   if(Build.VERSION.SDK_INT>=30)i=new Intent(Settings.ACTION_NOTIFICATION_LISTENER_DETAIL_SETTINGS).putExtra(Settings.EXTRA_NOTIFICATION_LISTENER_COMPONENT_NAME,new ComponentName(host,CompanionNotifications.class).flattenToString());open(i);});
  row(box,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0256,"정확한 위치 · GPS 궤적"),granted(Manifest.permission.ACCESS_FINE_LOCATION),()->request(new String[]{Manifest.permission.ACCESS_FINE_LOCATION,Manifest.permission.ACCESS_COARSE_LOCATION},9));
  row(box,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0257,"연락처 · 전화 즐겨찾기"),granted(Manifest.permission.READ_CONTACTS),()->request(new String[]{Manifest.permission.READ_CONTACTS},42));
  row(box,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0258,"최근 통화 기록"),granted(Manifest.permission.READ_CALL_LOG),()->request(new String[]{Manifest.permission.READ_CALL_LOG},43));
  row(box,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0259,"선택 연락처로 전화 걸기"),granted(Manifest.permission.CALL_PHONE),()->request(new String[]{Manifest.permission.CALL_PHONE},44));
  row(box,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0260,"전화 받기·종료 · 동반 기기 등록"),io.opennoodoe.app.companion.CallPermissions.canControl(host,new DeviceSelections(host).address()),()->io.opennoodoe.app.companion.CompanionAssociation.request(host,new DeviceSelections(host).address()));
  TextView calls=new TextView(host);calls.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0261,"기존 전화 앱과 헤드셋은 유지해요. 받기·종료는 동반 기기 등록과 Android 12 이상 지원이 필요해요. Android가 최근 통화 권한을 제한하면 해당 목록에 Permission Denied!가 표시돼요."));box.addView(calls);
  LocationManager location=host.getSystemService(LocationManager.class);boolean enabled=location!=null&&location.isProviderEnabled(LocationManager.GPS_PROVIDER);
  row(box,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0262,"휴대폰 위치 서비스"),enabled,()->open(new Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS)));
  if(Build.VERSION.SDK_INT>=29)row(box,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0263,"백그라운드 자동 연결 GPS · 항상 위치 허용"),granted(Manifest.permission.ACCESS_BACKGROUND_LOCATION),()->{
   if(!granted(Manifest.permission.ACCESS_FINE_LOCATION)){Toast.makeText(host,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0264,"먼저 정확한 위치를 허용해 주세요."),Toast.LENGTH_LONG).show();return;}
   if(Build.VERSION.SDK_INT==29)request(new String[]{Manifest.permission.ACCESS_BACKGROUND_LOCATION},10);else appSettings();});
  android.os.PowerManager power=host.getSystemService(android.os.PowerManager.class);
  row(box,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0265,"화면 OFF 연동 · 배터리 사용 제한 확인"),power!=null&&power.isIgnoringBatteryOptimizations(host.getPackageName()),()->appSettings());
  if(power!=null&&power.isPowerSaveMode())row(box,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0266,"절전 모드가 GPS를 제한할 수 있어요"),false,()->open(new Intent(Settings.ACTION_BATTERY_SAVER_SETTINGS)));
  if(Build.VERSION.SDK_INT>=33)row(box,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0267,"연결·진행 알림 표시"),granted(Manifest.permission.POST_NOTIFICATIONS),()->request(new String[]{Manifest.permission.POST_NOTIFICATIONS},11));
  TextView footer=new TextView(host);footer.setText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0268,"앱을 연 상태에서 주행 연결을 시작하면 화면을 꺼도 시동 ON 동안 위치를 전송해요. 키 OFF·연결 종료 시 위치 수집을 멈춰요. 백그라운드에서 자동 재연결하려면 위치를 ‘항상 허용’으로 설정해 주세요. Galaxy에서는 앱 정보 → 배터리 → 제한 없음도 확인해 주세요. 휴대폰 절전 모드가 화면 OFF GPS를 막으면 절전 모드를 해제해야 해요. 설치에는 위치 권한이 필요하지 않아요."));box.addView(footer);
  HomeSections.styleButtons(box,host);ScrollView scroll=new ScrollView(host);scroll.addView(box);
  dialog=new AlertDialog.Builder(host).setTitle(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0269,"주행 연동 준비")).setView(scroll).setPositiveButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0270,"완료"),null).setNeutralButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0271,"앱 설정"),(d,w)->appSettings()).create();dialog.show();
 }
 private void row(LinearLayout box,String label,boolean ok,Runnable action){Button b=new Button(host);b.setText((ok?"✓ ":"○ ")+label);b.setAllCaps(false);b.setOnClickListener(v->{if(dialog!=null)dialog.dismiss();try{action.run();}catch(RuntimeException failure){
   android.util.Log.w("PermissionCenter","Permission action unavailable: "+failure.getClass().getSimpleName());
   Toast.makeText(host,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0272,"Android에서 요청을 열지 못했어요. 앱 설정에서 권한을 확인해 주세요."),Toast.LENGTH_LONG).show();}});box.addView(b);}
 private void request(String[] values,int code){
  java.util.ArrayList<String> missing=new java.util.ArrayList<>();for(String value:values)if(!granted(value))missing.add(value);
  if(missing.isEmpty()){appSettings();return;}
  // FINE and COARSE must travel together, including an approximate-only grant.
  if(code==9)host.requestPermissions(values,code);else host.requestPermissions(missing.toArray(new String[0]),code);
 }
 private void appSettings(){open(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,Uri.parse("package:"+host.getPackageName())));}
 private void open(Intent intent){try{host.startActivity(intent);}catch(ActivityNotFoundException e){try{host.startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,Uri.parse("package:"+host.getPackageName())));}catch(ActivityNotFoundException unavailable){Toast.makeText(host,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0273,"Android 설정에서 이 앱의 권한을 열어 주세요."),Toast.LENGTH_LONG).show();}}}
 void dismiss(){if(dialog!=null)dialog.dismiss();}
}
