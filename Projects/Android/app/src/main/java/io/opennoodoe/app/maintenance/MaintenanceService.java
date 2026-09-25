package io.opennoodoe.app.maintenance;

import android.app.*;
import android.content.*;
import android.content.pm.ServiceInfo;
import android.content.pm.PackageManager;
import android.Manifest;
import android.net.Uri;
import android.os.*;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import io.opennoodoe.app.installer.*;
import io.opennoodoe.app.companion.*;
import io.opennoodoe.app.diagnostics.SessionLog;
import io.opennoodoe.app.protocol.ndcp.NdcpClient;
import java.io.*;
import java.util.concurrent.*;

/** One transport owner for maintenance and the explicitly selected companion
 * session. Stock autosync and the original NoodoeService are never started. */
public final class MaintenanceService extends Service {
 public interface Listener { default void workflow(SetupWorkflow.View v){} default void selectionCleared(){} void changed(String message,boolean busy); default void progress(InstallerPresentation.Snapshot s){changed(s.display(),s.running);} }
 public final class LocalBinder extends Binder { public MaintenanceService service(){return MaintenanceService.this;} }
 private final SessionEpoch epochs=new SessionEpoch();
 private final ThreadLocal<Long> workerEpoch=new ThreadLocal<>();
 private DeviceSelections selections;private SetupWorkflow workflow;
 public SetupWorkflow.View workflow(){return workflow.view();}
 private void notifyWorkflow(){long token=epochs.current();main.post(()->{if(epochs.accepts(token))for(Listener l:listeners)l.workflow(workflow.view());});}
 public void refreshPermissions(){if(companionRunning&&foregroundReady)try{foreground(true);}catch(RuntimeException e){publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0207,"위치 서비스 권한을 확인해 주세요. 주행 연결은 유지해요."));}}
 private volatile boolean resetting;
 private volatile String screenCandidate="";
 private volatile long screenEpoch,screenDeadline;
 private volatile boolean screenAccepted,screenVisible;
 public boolean screenVisible(){return screenVisible&&busy;}
 public boolean canConfirmScreen(){return screenVisible()&&!screenAccepted&&!screenCandidate.isEmpty()&&epochs.accepts(screenEpoch)&&System.nanoTime()<screenDeadline;}
 public String screenConfirmationKey(){return canConfirmScreen()?screenEpoch+":"+screenDeadline+":"+screenCandidate:"";}
 public synchronized void confirmScreen(String key){if(canConfirmScreen()&&screenConfirmationKey().equals(key))screenAccepted=true;}
 public long screenSeconds(){return Math.max(0,(screenDeadline-System.nanoTime())/1000000000L);}

 private volatile Future<?> currentWork;
 private final ExecutorService control=Executors.newSingleThreadExecutor();
 private final ExecutorService worker=Executors.newSingleThreadExecutor();
 private final CopyOnWriteArrayList<Listener> listeners=new CopyOnWriteArrayList<>();
 private final Handler main=new Handler(Looper.getMainLooper());
 private final InstallerPresentation presentation=new InstallerPresentation(SystemClock::elapsedRealtime);
 private volatile InstallerPresentation.Snapshot completedPresentation;
 private volatile boolean presenting,foregroundReady;private long cacheAt,notificationAt;private String cacheKey="",cacheText="";
 private volatile InstallerTransport maintenanceStream;
 private final Runnable ticker=new Runnable(){public void run(){
  if(presenting){InstallerPresentation.Snapshot s=presentation.snapshot();for(Listener l:listeners)l.progress(s);
   if((s.running&&SystemClock.elapsedRealtime()-cacheAt>=5000)||!cacheKey.equals(s.key)||(!s.running&&!cacheText.equals(s.display()))){
    cacheAt=SystemClock.elapsedRealtime();cacheKey=s.key;cacheText=s.display();
    selections.display(selections.address(),s.display());
   }
   if(s.running&&foregroundReady&&SystemClock.elapsedRealtime()-notificationAt>=1000){notificationAt=SystemClock.elapsedRealtime();Notification.Builder b=Build.VERSION.SDK_INT>=26?new Notification.Builder(MaintenanceService.this,"install"):new Notification.Builder(MaintenanceService.this);
    b.setSmallIcon(android.R.drawable.stat_sys_upload).setContentTitle(canConfirmScreen()?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.boot_screen_action,"지금 확인 버튼을 눌러 주세요"):s.title).setContentText(canConfirmScreen()?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.boot_screen_open_app,"앱을 열어 CFW UPDATE 화면 확인 버튼을 눌러 주세요."):(s.notice.isEmpty()?s.text:s.notice)).setOngoing(true)
     .setContentIntent(PendingIntent.getActivity(MaintenanceService.this,0,new Intent(MaintenanceService.this,InstallerHomeActivity.class),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE))
     .setSubText(s.overall).setProgress(100,Math.max(0,s.overallPercent>=0?s.overallPercent:s.percent),s.overallPercent<0&&s.percent<0);
    getSystemService(NotificationManager.class).notify(17,b.build());
   }
  }main.postDelayed(this,200);
 }};
 @Override public void onCreate(){super.onCreate();selections=new DeviceSelections(this);workflow=new SetupWorkflow(this);workflow.select(selections.address());String old=selections.display(selections.address());
  if(old!=null)message=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0208,"이전 작업의 마지막 표시입니다. 자동 재개하지 않았어요. 기기 상태·작업 기록을 확인해 주세요.\n\n")+old;
  main.post(ticker);
 }
 private volatile boolean busy,companionRunning,recording;
 public boolean isBusy(){return busy;}
 public boolean isCompanionRunning(){return companionRunning;}
 public boolean isRecording(){return recording;}
 public void recording(boolean enabled){recording=companionRunning&&enabled;publish(recording?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0209,"폰 주행 기록을 시작해요."):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0210,"폰 주행 기록을 종료해요. 음악·알림·GPS 연결은 유지해요."));}
 private volatile InstallerTransport companionStream;
 private volatile DeviceSettingsSession deviceSettings;
 public DeviceSettingsSession deviceSettings(){return deviceSettings;}
 private volatile java.util.List<long[]> settingRows=java.util.Collections.emptyList();
 public java.util.List<long[]> settings(){java.util.List<long[]> copy=new java.util.ArrayList<>();for(long[] r:settingRows)copy.add(r.clone());return copy;}
 public boolean change(long field,int value){DeviceSettingsSession s=deviceSettings;if(!companionRunning||s==null)return false;for(long[] r:s.rows())if(r[0]==field)return s.edit(field,value,r[1]);return false;}
 private void closeCompanion(){recording=false;companionRunning=false;InstallerTransport s=companionStream;if(s!=null)try{s.close();}catch(IOException ignored){}}
 public void disconnect(){
  if(!new DrivingMode(this).set(selections.address(),DrivingMode.MANUAL)){publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0211,"주행 중지 상태를 저장하지 못했어요. 폰 저장 공간을 확인해 주세요."));return;}
  new RideServiceRestart(this).clear();closeCompanion();publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0212,"주행 연동을 수동 중지했어요. 이제 업데이트할 수 있어요."));
 }
 public void resumeAutomatic(){
  String address=selections.address();if(!busy&&!resetting&&workflow.view().role.equals("product")&&!workflow.view().recovery&&new DrivingMode(this).automatic(address))run("companion",address,null);
 }

 private volatile String message=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0213,"설치할 누도를 골라 주세요. 먼저 Android 설정에서 페어링하면 돼요.");
 public void subscribe(Listener l){listeners.add(l);l.workflow(workflow.view());if(presenting)l.progress(presentation.snapshot());else {if(completedPresentation!=null)l.progress(completedPresentation);l.changed(message,busy);}}
 public void exportRides(Uri uri){control.submit(()->{try(RideHistory history=new RideHistory(this);OutputStream out=getContentResolver().openOutputStream(uri)){if(out==null)throw new IOException("Output unavailable");history.export(out);main.post(()->android.widget.Toast.makeText(this,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0214,"주행 기록 CSV를 저장했어요."),android.widget.Toast.LENGTH_LONG).show());}catch(Exception e){main.post(()->android.widget.Toast.makeText(this,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0215,"주행 기록 내보내기에 실패했어요. 저장 위치를 확인해 주세요."),android.widget.Toast.LENGTH_LONG).show());}});}
 public void unsubscribe(Listener l){listeners.remove(l);}
 private void publish(String s){
  final long token;
  synchronized(this){Long owner=workerEpoch.get();token=owner==null?epochs.current():owner;if(!epochs.accepts(token))return;message=s;if(presenting)presentation.update(s);}
  main.post(()->{if(epochs.accepts(token))for(Listener l:listeners)l.changed(s,busy);});
 }
 public String selectedDevice(){return selections.address();}
 public String selectedBundle(){return selections.bundle(selections.address());}
 /** A device switch discards only its active presentation, never journal evidence. */
 public synchronized void selectDevice(String address){
  if(busy||java.util.Objects.equals(address,selections.address()))return;
  try(SessionLog log=new SessionLog(new File(getFilesDir(),"installer/logs"))){
   log.append("device_selected",SessionLog.fields("device",address==null?"":address,"epoch",Long.toString(epochs.current()+1)));log.complete();
  }catch(IOException e){publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0216,"기기 전환 기록을 저장하지 못했어요. 저장 공간을 확인해 주세요."));return;}
  epochs.invalidate();screenVisible=false;screenCandidate="";selections.select(address);workflow.select(address);notifyWorkflow();presenting=false;completedPresentation=null;
  String previous=selections.display(address);publish(previous==null?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0217,"기기를 선택했어요. 이 기기에 사용할 ZIP을 선택해 주세요."):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0218,"이 기기의 이전 기록입니다. 새 연결에서 상태를 다시 확인합니다.\n")+previous);
 }
 /** Bounded local reset. Closing a socket does not prove that a remote write
  * stopped. Pre-command journals remain intact and reconciliation is mandatory. */
 public synchronized void resetLocal(){
  if(resetting)return;resetting=true;new RideServiceRestart(this).clear();
  control.submit(()->{
   boolean invalidated=false;
   try(SessionLog log=new SessionLog(new File(getFilesDir(),"installer/logs"))){
    log.append("app_reset_requested",SessionLog.fields("device",selections.address()==null?"":selections.address(),"epoch",Long.toString(epochs.current())));
    long token;Future<?> pending;
    synchronized(this){token=epochs.invalidate();screenVisible=false;screenCandidate="";invalidated=true;busy=true;recording=false;companionRunning=false;presenting=false;completedPresentation=null;pending=currentWork;}
    // Invalidation immediately fences new sends. Close breaks any pending read;
    // its owner journals UNKNOWN rather than pretending that the device aborted.
    InstallerTransport a=maintenanceStream,b=companionStream;
    if(a!=null)try{a.close();}catch(IOException ignored){}if(b!=null&&b!=a)try{b.close();}catch(IOException ignored){}
    boolean drained=true;
    if(pending!=null)try{pending.get(2000,TimeUnit.MILLISECONDS);}catch(TimeoutException e){drained=false;pending.cancel(true);}catch(Exception ignored){}
    log.append("app_session_invalidated",SessionLog.fields("epoch",Long.toString(token),"state",drained?"DRAINED_QUERY_REQUIRED":"RESULT_UNKNOWN_QUERY_REQUIRED"));
    log.complete();
    synchronized(this){selections.reset();workflow.reset();notifyWorkflow();maintenanceStream=null;companionStream=null;deviceSettings=null;settingRows=java.util.Collections.emptyList();busy=false;resetting=false;}
    main.post(()->{if(!epochs.accepts(token))return;foregroundReady=false;stopForeground(true);stopSelf();for(Listener l:listeners)l.selectionCleared();publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0219,"앱 연결·선택을 초기화했어요. 기기 작업은 취소하지 않았어요. 누도를 선택해 상태부터 확인하세요."));});
   }catch(IOException e){synchronized(this){resetting=false;if(invalidated)busy=false;}publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0220,"초기화 기록을 저장하지 못했어요. 기존 증거를 보존한 채 중단했어요."));}
  });
 }
 private StockUpdateSession.Progress observer(long token){return new StockUpdateSession.Progress(){
  public void update(String text){if(epochs.accepts(token))presentation.update(text);}
  public void stage(String k,String t,long d,long n,String u){if(epochs.accepts(token))presentation.stage(k,t,d,n,u);}
  public void reply(){if(epochs.accepts(token))presentation.reply();}
  public void screenPrompt(){if(epochs.accepts(token)){screenVisible=true;screenCandidate="";screenAccepted=false;}}
  public void awaitScreen(String candidate,long deadline)throws Exception {
   awaitScreen(candidate,deadline,()->{});
  }
  public void awaitScreen(String candidate,long deadline,StockUpdateSession.ScreenCheck check)throws Exception {
   epochs.check(token);screenEpoch=token;screenCandidate=candidate;screenDeadline=deadline;screenAccepted=false;screenVisible=true;
   long nextCheck=0;
   try{while(!screenAccepted){epochs.check(token);long now=System.nanoTime();if(now>=deadline)throw new StockUpdateSession.ScreenConfirmationTimeout(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.boot_screen_expired,"화면 확인 시간이 끝났어요. 기기의 부팅 결과를 확인해 주세요."));
     if(now>=nextCheck){check.poll();presentation.reply();nextCheck=System.nanoTime()+1_000_000_000L;}
     Thread.sleep(100);}
    epochs.check(token);
   }finally{if(epochs.accepts(token)){screenVisible=false;screenCandidate="";screenAccepted=false;}}
  }
  public void installStatus(InstallProgressSnapshot s)throws IOException{epochs.check(token);presentation.installStatus(s);}
  public void role(String r){synchronized(MaintenanceService.this){if(epochs.accepts(token)){presentation.role(r);workflow.role(r);notifyWorkflow();}}}
  public void deviceStatus(BootstrapProgress.View v)throws IOException{epochs.check(token);presentation.deviceStatus(v);}
  public void connection(String p,int a,int b)throws IOException{epochs.check(token);}
 };}
 private InstallerTransport connect(long token,String address,StockUpdateSession.Progress progress)throws IOException{
  return connect(token,address,progress,false);
 }
 private InstallerTransport connect(long token,String address,StockUpdateSession.Progress progress,boolean bootHandoff)throws IOException{
  epochs.check(token);InstallerTransport stream=epochs.bind(token,new AndroidInstallerTransport(this,address,progress,bootHandoff));
  synchronized(this){if(!epochs.accepts(token)){stream.close();epochs.check(token);}maintenanceStream=stream;}
  return stream;
 }
 @Override public IBinder onBind(Intent i){return new LocalBinder();}
 @Override public int onStartCommand(Intent i,int flags,int id){
  if(i==null){
   String address=selections.address();
   if(!busy&&!resetting&&new RideServiceRestart(this).allowed(address,workflow.automaticDrivingAllowed()))run("companion",address,null);
   if(!busy){stopForeground(true);stopSelf(id);}return companionRunning?START_STICKY:START_NOT_STICKY;
  }
  if(i!=null&&i.hasExtra("auto-device")){
   String address=i.getStringExtra("auto-device");
   if(!busy&&!resetting&&workflow.automaticDrivingAllowed()&&java.util.Objects.equals(address,selections.address())&&CompanionAssociation.associated(this,address)&&new DrivingMode(this).automatic(address))run("companion",address,null);
   if(!busy){stopForeground(true);stopSelf(id);}return companionRunning?START_STICKY:START_NOT_STICKY;
  }
  if(i!=null&&epochs.accepts(i.getLongExtra("epoch",-1))&&busy&&!resetting){if(!foregroundReady)try{foreground(i.getBooleanExtra("radio",false));}catch(RuntimeException e){disconnect();publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0221,"Android가 백그라운드 서비스를 허용하지 않았어요. 앱을 열고 권한을 확인해 주세요."));}}
  else if(!busy||resetting){stopForeground(true);stopSelf(id);}return companionRunning?START_STICKY:START_NOT_STICKY;
 }
 private boolean locationForegroundReady(){return foregroundReady&&(Build.VERSION.SDK_INT<29||(getForegroundServiceType()&ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION)!=0);}
 private void foreground(boolean radio){
  NotificationManager manager=getSystemService(NotificationManager.class);
  if(Build.VERSION.SDK_INT>=26)manager.createNotificationChannel(new NotificationChannel("install",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0222,"누도 연결·진행"),NotificationManager.IMPORTANCE_LOW));
  Intent home=new Intent(this,InstallerHomeActivity.class);
  PendingIntent pi=PendingIntent.getActivity(this,0,home,PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
  Notification.Builder b=Build.VERSION.SDK_INT>=26?new Notification.Builder(this,"install"):new Notification.Builder(this);
  Notification notification=b.setSmallIcon(android.R.drawable.stat_sys_upload).setContentTitle(companionRunning?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0223,"누도 주행 연결"):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0224,"누도를 준비하고 있어요"))
    .setContentText(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0225,"진행 상황을 보려면 눌러 주세요.")).setContentIntent(pi).setOngoing(true).build();
  if(Build.VERSION.SDK_INT>=29){
   int type=radio?ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE:ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC;
   boolean gps=radio&&companionRunning&&checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)==PackageManager.PERMISSION_GRANTED;
   try{startForeground(17,notification,type|(gps?ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION:0));}
   catch(SecurityException restricted){
    if(!gps)throw restricted;
    // Background auto-connect may have while-in-use permission but no right
    // to START a location FGS. Keep SPP/music and report GPS unavailable;
    // reopening the Activity or granting background access retries promotion.
    startForeground(17,notification,type);
    publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0226,"주행 연결은 유지해요. GPS 자동 연결은 위치 ‘항상 허용’을 확인하거나 앱을 열어 주세요."));
   }
  }
  else startForeground(17,notification);
  foregroundReady=true;
 }
 /** Starting an action does not claim that a remote write completed. The
  * controller journals uncertain replies before any destructive command. */
 public synchronized void run(String action,String address,Uri uri){
  if(busy||resetting)return;
  if(!"companion".equals(action))new RideServiceRestart(this).clear();
  boolean radio=!"import".equals(action)&&!"export".equals(action)&&!"diagnostics".equals(action)&&!"journal".equals(action)&&!"rides-export".equals(action);
  if(radio&&Build.VERSION.SDK_INT>=31&&(checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)!=PackageManager.PERMISSION_GRANTED||checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN)!=PackageManager.PERMISSION_GRANTED)){publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0227,"근처 기기 권한을 허용한 뒤 다시 눌러 주세요."));return;}
  if(address!=null&&!java.util.Objects.equals(address,selections.address())){selectDevice(address);if(!address.equals(selections.address()))return;}
  DrivingMode mode=new DrivingMode(this);
  if("companion".equals(action)){
   if(mode.get(address)==DrivingMode.UPDATE){publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0228,"업데이트 결과부터 확인해 주세요. 확인 전에는 자동 주행 연동을 시작하지 않아요."));return;}
   if(!mode.set(address,DrivingMode.AUTO)){publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0229,"연결 상태를 저장하지 못했어요."));return;}
  }
  if(("update-cfw".equals(action)||"diagnostic-cfw".equals(action))&&!mode.beginUpdate(address)){publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0230,"먼저 ‘주행 연동 중지 · 업데이트 준비’를 눌러 주세요."));return;}
  screenVisible=false;screenCandidate="";screenAccepted=false;final long token=epochs.invalidate();final String chosenAddress=selections.address();final StockUpdateSession.Progress observed=observer(token);
  companionRunning="companion".equals(action);busy=true;presenting=!companionRunning;if(presenting){completedPresentation=null;presentation.begin(action);}publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0231,"잠깐만요. 준비하고 있어요."));
  Intent keep=new Intent(this,MaintenanceService.class).putExtra("radio",radio).putExtra("epoch",token);
  try{if(Build.VERSION.SDK_INT>=26)startForegroundService(keep);else startService(keep);foreground(radio);workflow.begin(action);notifyWorkflow();}
  catch(RuntimeException e){busy=false;companionRunning=false;foregroundReady=false;stopForeground(true);stopSelf();publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0232,"백그라운드 작업을 시작하지 못했어요. 앱을 연 상태에서 Bluetooth 권한을 확인해 주세요."));presentation.finish(message,true);workflow.finish(action,true,message);notifyWorkflow();return;}
  currentWork=worker.submit(()->{
   workerEpoch.set(token);
   PowerManager.WakeLock wake=((PowerManager)getSystemService(POWER_SERVICE)).newWakeLock(PowerManager.PARTIAL_WAKE_LOCK,"NoodoeInstaller:Transfer");

   File root=new File(getFilesDir(),"installer");root.mkdirs();
   InstallerController controller=new InstallerController(root);
   boolean failed=false;
   try{
    epochs.check(token);wake.acquire();
    android.content.SharedPreferences prefs=getSharedPreferences("installer",MODE_PRIVATE);
    if("companion".equals(action)){
     if(address==null||address.isEmpty())throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0233,"먼저 누도를 골라 주세요."));
     runCompanion(address,new File(root,"logs"),token);
    }else if(action.matches("photo[0-2]")){
     if(address==null||address.isEmpty()||uri==null)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0234,"기기와 사진을 먼저 골라 주세요."));
     byte[] jpeg=PhotoUpload.prepare(this,uri);
     try(SessionLog log=new SessionLog(new File(root,"logs"));InstallerTransport stream=connect(token,address,observed)){
      maintenanceStream=stream;CompanionWire wire=new CompanionWire(new NdcpClient(stream,3000));wire.connect();
      PhotoUpload.send(wire,action.charAt(5)-'0',jpeg,log,this::publish);log.complete();
      publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0235,"사진 슬롯 ")+action.charAt(5)+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0236," 저장을 확인했어요. 기기 설정에서 배경 사진을 선택하세요."));
     }
    }else if("radio-test".equals(action)){
     if(address==null||address.isEmpty())throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0233,"먼저 누도를 골라 주세요."));
     try(SessionLog log=new SessionLog(new File(root,"logs"));InstallerTransport stream=connect(token,address,observed)){
      log.append("radio_test_start",SessionLog.fields("bytes","8192"));
      RadioSelfTest.run(new NdcpClient(stream,3000),this::publish,log);log.complete();
     }
    }else if("import".equals(action)){
     try(InputStream in=getContentResolver().openInputStream(uri)){
      String hash=controller.importBundle(in);epochs.check(token);selections.bundle(chosenAddress,hash);publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0237,"설치 파일을 확인했어요. 이제 기기와 맞는 파일인지 확인해 주세요."));
     }
    }else if("rides-export".equals(action)){
     try(RideHistory history=new RideHistory(this);OutputStream out=getContentResolver().openOutputStream(uri)){if(out==null)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0238,"기록 파일을 열 수 없어요."));history.export(out);publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0214,"주행 기록 CSV를 저장했어요."));}
    }else if("diagnostics".equals(action)){
     try(OutputStream out=getContentResolver().openOutputStream(uri)){
      io.opennoodoe.app.diagnostics.SessionLog.export(new File(root,"logs"),out);publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0239,"진단 기록을 저장했어요. 사진·알림 내용·전체 백업은 포함하지 않았어요."));
     }
    }else if("export".equals(action)){
     try(OutputStream out=getContentResolver().openOutputStream(uri)){controller.exportEvidence(out);publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0240,"백업과 작업 기록을 저장했어요."));}
    }else{
     if(address==null||address.isEmpty())throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0233,"먼저 누도를 골라 주세요."));
     String result=controller.run(action,address,selections.bundle(chosenAddress),
       new InstallerController.Connections(){
        public InstallerTransport open()throws Exception{return open(observed);}
        public InstallerTransport open(StockUpdateSession.Progress p)throws Exception{return connect(token,address,p);}
        public InstallerTransport openBoot(StockUpdateSession.Progress p)throws Exception{return connect(token,address,p,true);}
       },observed,MaintenanceService::checkPhoto);
     publish(result);
    }
   }catch(Exception e){
    failed=true;
    try(FileWriter out=new FileWriter(new File(root,"last-error-"+DeviceSelections.key(chosenAddress)+"-"+token+".txt"))){e.printStackTrace(new PrintWriter(out));}catch(IOException ignored){}
    publish("companion".equals(action)?(e.getMessage()==null?e.getClass().getSimpleName():e.getMessage()):InstallerFailure.explain(e,presentation.role()));
   }finally{
    if(wake.isHeld())wake.release();if(epochs.accepts(token)){companionRunning=false;companionStream=null;maintenanceStream=null;if(deviceSettings!=null)deviceSettings.disconnected();deviceSettings=null;busy=false;presentation.finish(message,failed);workflow.finish(action,failed,message);notifyWorkflow();publish(message);
    if(presenting)selections.display(chosenAddress,presentation.snapshot().display());
    new RideServiceRestart(this).clear();recording=false;
    // Keep the foreground service alive through the update -> riding handoff.
    // stopSelf before the posted reconnect destroys an unbound service and
    // shuts down its socket executor while that reconnect is being started.
    boolean resume=!failed&&"update-cfw".equals(action)&&new DrivingMode(this).set(chosenAddress,DrivingMode.AUTO);
    finishConnectionService(token,resume);
    }workerEpoch.remove();
   }
  });
 }
 /** Main-thread, generation-scoped handoff. Never stop a newly started owner. */
 void finishConnectionService(long token,boolean resume){
  final InstallerPresentation.Snapshot finished=presenting?presentation.snapshot():null;
  main.post(()->{
   if(!epochs.accepts(token))return;
   if(finished!=null){
    completedPresentation=finished;
    for(Listener listener:listeners)listener.progress(finished);
   }
   if(resume)resumeAutomatic();
   if(!busy&&epochs.accepts(token)){foregroundReady=false;stopForeground(true);stopSelf();}
  });
 }
 /** This worker owns both companion and installation sockets. Ending this
  * loop closes location subscriptions before a maintenance task can run. */
 private void runCompanion(String address,File logs,long token)throws Exception {
  StockUpdateSession.Progress observed=observer(token);
  try(SessionLog log=new SessionLog(logs)){
   CompanionReconnectPolicy retry=new CompanionReconnectPolicy();
   while(epochs.accepts(token)&&companionRunning&&retry.allowed()){
    NdcpClient client=null;String phase="connect";long opened=SystemClock.elapsedRealtime();
    try(InstallerTransport stream=connect(token,address,observed,true)){
     companionStream=stream;phase="initialize";log.append("connection_open",SessionLog.fields("action","companion","epoch",java.util.UUID.randomUUID().toString()));
     client=new NdcpClient(stream,3000);CompanionWire wire=new CompanionWire(client);wire.initialize();epochs.check(token);observed.role("product");new RideServiceRestart(this).productConnected(address);DeviceSettingsSession settings=new DeviceSettingsSession(wire,(action,field,result)->log.append(action,SessionLog.fields("offset",Long.toString(field),"result",Long.toString(result))));deviceSettings=settings;long settingsRevision=-1;
     int stopThreshold=5;for(long[] row:settingRows)if(row[0]==0x1014)stopThreshold=(int)row[1];
     try(CompanionRuntime runtime=new CompanionRuntime(this,wire,address,this::locationForegroundReady);RideSync rides=new RideSync(this,wire,address,stopThreshold)){
      long lastUi=-1000,lastGpsLog=-30000;String logNotice="";
      while(epochs.accepts(token)&&companionRunning){long signal=runtime.signalVersion();long started=SystemClock.elapsedRealtime();phase="content";runtime.tick();for(long[] row:settingRows)if(row[0]==0x1014)rides.threshold((int)row[1]);rides.enabled(recording);rides.tick();
       phase="settings";settings.tick(started);if(settingsRevision!=settings.revision()){settingRows=settings.rows();settingsRevision=settings.revision();}
       if(started-lastGpsLog>=30000){lastGpsLog=started;
        try{log.append("phone_location_status",runtime.gpsDiagnostics());logNotice="";}
        catch(IOException|IllegalArgumentException localLogFailure){logNotice=" · "+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_log_failed,"Phone log unavailable; riding stays connected");}
       }
       retry.healthy(started);
       if(started-lastUi>=1000){lastUi=started;publish((wire.ign?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0241,"주행 중"):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0242,"시동 대기"))+" · "+rides.status+runtime.gpsNotice()+logNotice);}
       long wait=(runtime.hasPendingVisuals()?20:200)-(SystemClock.elapsedRealtime()-started);if(wait>0)runtime.awaitChange(signal,wait);
      }
     }
    }catch(Exception e){
     if(!epochs.accepts(token)||!companionRunning)break;
     boolean reconnect=retry.failed(e);String opcode=client==null?"none":String.format(java.util.Locale.ROOT,"0x%02X",client.lastOpcode());
     log.append(reconnect?"connection_error":"companion_local_error",SessionLog.fields("action","companion","state",phase,
       "sequence",Integer.toString(client==null?0:client.lastSequence()),"opcode",opcode,"error_class",e.getClass().getSimpleName(),
       "result",e instanceof NdcpClient.DeviceRejected?Long.toString(((NdcpClient.DeviceRejected)e).result):"unknown",
       "elapsed_ms",Long.toString(SystemClock.elapsedRealtime()-opened),"offset",Integer.toString(retry.failures())));
     if(!reconnect)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_local_failed,"Phone processing failed. Radio retries stopped; share diagnostics.")+" ("+phase+", "+opcode+", "+e.getClass().getSimpleName()+")",e);
     publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0244,"연결이 끊겼어요. 재연결 ")+retry.failures()+" / 8 · "+phase+" / "+opcode);
     long until=SystemClock.elapsedRealtime()+retry.delayMs();while(epochs.accepts(token)&&companionRunning&&SystemClock.elapsedRealtime()<until)Thread.sleep(200);
    }
    finally{if(epochs.accepts(token)){companionStream=null;if(deviceSettings!=null)deviceSettings.disconnected();deviceSettings=null;settingRows=java.util.Collections.emptyList();}}
   }
   if(companionRunning)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0245,"재연결 한도에 도달했어요. 기기와 Bluetooth 상태를 확인해 주세요."));
   log.complete();publish(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0246,"주행 연동을 종료했어요. 위치 수집도 중지했어요."));
  }
 }
 private static void checkPhoto(byte[] data)throws IOException{
  BitmapFactory.Options o=new BitmapFactory.Options();o.inJustDecodeBounds=true;BitmapFactory.decodeByteArray(data,0,data.length,o);
  if(o.outWidth<1||o.outHeight<1||o.outWidth>480||o.outHeight>480||data.length>131072)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0247,"사진 크기를 확인해 주세요."));
  Bitmap b=BitmapFactory.decodeByteArray(data,0,data.length);if(b==null)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0248,"사진을 끝까지 읽을 수 없어요."));b.recycle();
 }
 @Override public void onDestroy(){main.removeCallbacks(ticker);closeCompanion();InstallerTransport t=maintenanceStream;if(t!=null)try{t.close();}catch(IOException ignored){}epochs.invalidate();worker.shutdownNow();control.shutdownNow();super.onDestroy();}
}
