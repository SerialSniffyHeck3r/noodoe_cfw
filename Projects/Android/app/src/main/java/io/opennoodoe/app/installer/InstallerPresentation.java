package io.opennoodoe.app.installer;
import java.util.Locale;
/** Thread-safe presentation only: never authorizes or retries a device command.
 * Counters are current-stage measurements; no fabricated whole-install ETA. */
public final class InstallerPresentation implements StockUpdateSession.Progress {
 public interface Clock {long now();}
 private final Clock clock;
 private final InstallJourney journey=new InstallJourney();
 private long started,stageStarted,changed,replyAt=-1,done,total,baseDone,baseTime,ended;
 private String key="prepare",text=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0491,"준비 중"),unit="",role="unknown";
 private boolean running,error;
 private InstallProgressSnapshot device;
 public synchronized void installStatus(InstallProgressSnapshot s){device=s;}
 public InstallerPresentation(Clock c){clock=c;}
 public synchronized void begin(String action){long now=clock.now();started=stageStarted=changed=baseTime=now;replyAt=-1;done=total=baseDone=0;key="prepare";text=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0492,"연결과 작업 기록을 준비하고 있어요.");unit="";role="unknown";running=true;error=false;device=null;journey.begin(action);}
 public synchronized void role(String r){role=r;}
 public synchronized String role(){return role;}
 public synchronized void reply(){replyAt=clock.now();}
 public synchronized void update(String s){text=s;}
 public synchronized void stage(String k,String s,long d,long t,String u){
  long now=clock.now();d=Math.max(0,d);t=Math.max(0,t);
  if(!k.equals(key)||!u.equals(unit)||t!=total||d<done){stageStarted=baseTime=now;baseDone=d;changed=now;}
  if(d!=done||!k.equals(key))changed=now;
  key=k;text=s;done=d;total=t;unit=u;
  journey.stage(k);
 }
 public synchronized void finish(String message,boolean failed){ended=clock.now();running=false;error=failed;text=message;}
 public synchronized Snapshot snapshot(){
  long now=running?clock.now():ended,age=replyAt<0?-1:Math.max(0,now-replyAt),idle=Math.max(0,now-changed);
  boolean waiting=key.equals("authorize")||key.equals("confirm")||key.equals("reboot")||key.equals("stock-reboot")||key.equals("bootstrap-connect")||key.equals("health")||key.equals("screen-confirm");
  String notice="";
  if(key.equals("unconfirmed"))notice=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0493,"완료 여부를 확인하지 못했어요. 기기 화면과 부팅 결과를 먼저 조회해 주세요.");
  if(running&&!waiting&&now-started>15000&&(age<0||age>15000))notice=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0494,"응답을 기다리는 중이에요. 작업 결과는 아직 미확인입니다. 자동 재전송하지 않아요.");
  else if(running&&!waiting&&total>0&&idle>30000)notice=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0495,"응답은 있지만 진행량이 30초 이상 그대로예요. 현재 단계 확인이 필요해요.");
  int percent=total>0&&(running||key.equals("done"))&&!error?(int)Math.min(100,(double)done*100/total):-1;
  String rate="";
  if(running&&"B".equals(unit)&&done>baseDone&&now-baseTime>=2000){
   double bytesPerSecond=(done-baseDone)*1000.0/(now-baseTime);
   long seconds=(long)Math.ceil(Math.max(0,total-done)/bytesPerSecond);
   rate=String.format(Locale.ROOT,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0496,"%.1f KiB/s · 이 단계 약 %d초 남음"),bytesPerSecond/1024,seconds);
  }
  String counts=percent>=0?String.format(Locale.ROOT,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0497,"현재 작업 · %,d / %,d %s · %d%%"),done,total,unit,percent):error?String.format(Locale.ROOT,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0498,"중단 위치 · %,d / %,d %s — 결과 확인 필요"),done,total,unit):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0499,"현재 작업 · 진행량 확인 대기");
  if(device!=null&&running)counts+="\n"+device.detail();
  String timing=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0500,"경과 ")+duration(now-started)+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0501," · 현재 단계 ")+duration(now-stageStarted)+"\n"+(age<0?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0502,"검증된 응답을 아직 받지 못했어요."):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0503,"마지막 응답 ")+(age/1000)+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0414,"초 전"));
  int overall=journey.percent();String overallLabel=journey.label();
  if(device!=null){int stage=key.equals("done")?8:(key.equals("health")||key.equals("screen-confirm"))?7:key.equals("reboot")?6:(int)device.stage;
   overall=stage==8?100:Math.min(99,stage*100/8);overallLabel=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0504,"전체 진행 · 단계 기준 ")+stage+" / 8 ("+overall+"%)";
  }
  String outline=device==null?journey.outline():io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0505,"1 연결 · 2 검사 · 3 준비/전송/기록 · 4 최종 확인\n5 설치 확정 · 6 재시작 · 7 정상 실행 확인 · 8 완료");
  return new Snapshot(key,title(key),text,counts,timing,rate,notice,role,percent,running,error,overall,overallLabel,outline,InstallJourney.why(key));
 }
 private static String duration(long ms){long sec=Math.max(0,ms/1000);return String.format(Locale.ROOT,"%d:%02d",sec/60,sec%60);}
 public static String title(String k){
  if(k.equals("screen-confirm"))return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.boot_screen_action,"지금 확인 버튼을 눌러 주세요");
  if(k.equals("bootstrap-connect"))return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0506,"Bootstrap 자동 페어링·연결");
  if(k.startsWith("backup"))return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0507,"변경 전 복구 자료 보관");
  if(k.equals("unconfirmed"))return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0508,"작업 결과 미확인");
  if(k.equals("authorize"))return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0509,"누도에서 설치 메뉴 선택");
  if(k.startsWith("file"))return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0510,"CFW 파일 준비·기록·검증");
  switch(k){case "stock-transfer":return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0511,"Bootstrap 보내기");case "stock-reboot":return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0512,"키 조작 후 Bootstrap 확인");case "connect":return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0513,"기기 연결 확인");case "audit":return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0514,"저장소 배치 검사");case "baseline":return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0515,"변경 전 원본 해시 계산");case "preserve":return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0516,"설치 후 기존 파일 보존 확인");case "stage":return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0517,"설치 이미지 전송");case "confirm":return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0518,"기기에서 설치 확인 대기");case "reboot":return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0519,"설치·재시작 결과 확인");case "health":return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0520,"정상 실행·영구 확정 확인");case "done":return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0521,"설치 확인 완료");default:return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0522,"작업 준비·상태 확인");}
 }
 public static final class Snapshot {
  public final String key,title,text,counts,timing,rate,notice,role,overall,outline,why;public final int percent,overallPercent;public final boolean running,error;
  Snapshot(String k,String a,String b,String c,String d,String e,String f,String r,int p,boolean run,boolean err,int op,String ol,String steps,String reason){key=k;title=a;text=b;counts=c;timing=d;rate=e;notice=f;role=r;percent=p;running=run;error=err;overallPercent=op;overall=ol;outline=steps;why=reason;}
  public String display(){return overall+"\n"+title+"\n"+text+"\n\n"+counts+"\n"+timing+(rate.isEmpty()?"":"\n"+rate)+(notice.isEmpty()?"":"\n\n"+notice);}
 }
}
