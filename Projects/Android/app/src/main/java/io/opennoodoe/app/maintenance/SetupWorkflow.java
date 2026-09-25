package io.opennoodoe.app.maintenance;

import android.content.Context;
import android.content.SharedPreferences;

/** UI routing only. Never authorizes writes or replaces the durable install journal.
 * An unfinished attempt survives process death; inspection cannot mark it successful. */
public final class SetupWorkflow {
 private final SharedPreferences prefs;
 private final java.io.File journalRoot;
 private String address,role="unknown",operation="",error="";private boolean running,recovery;
 public static final class View {
  public final String address,role,operation,error;public final boolean running,recovery;
  public View(String a,String r,String o,String e,boolean busy,boolean failed){address=a;role=r;operation=o;error=e;running=busy;recovery=failed;}
 }
 SetupWorkflow(Context context){prefs=context.getSharedPreferences("setup-workflow-v1",0);journalRoot=new java.io.File(context.getFilesDir(),"installer");}
 synchronized void select(String value){
  if(java.util.Objects.equals(address,value))return;
  address=value;role="unknown";running=false;importPriorAttempt();operation=prefs.getString(key("operation"),"");
  recovery=prefs.getBoolean(key("pending"),false);error=recovery?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0342,"이 기기의 이전 작업 결과를 다시 확인해야 해요."):"";
 }
 private String key(String suffix){return DeviceSelections.key(address)+"."+suffix;}
 private void importPriorAttempt(){
  if(address==null||prefs.contains(key("pending")))return;
  String suffix="-"+DeviceSelections.key(address)+".journal";
  java.io.File[] files=journalRoot.listFiles((dir,name)->name.toUpperCase(java.util.Locale.ROOT).endsWith(suffix.toUpperCase(java.util.Locale.ROOT))||name.toUpperCase(java.util.Locale.ROOT).endsWith((suffix+".previous").toUpperCase(java.util.Locale.ROOT)));
  if(files==null||files.length==0)return;
  java.util.Arrays.sort(files,(a,b)->Long.compare(b.lastModified(),a.lastModified()));
  try {
   java.io.File latest=files[0];if(latest.getName().endsWith(".previous"))latest=new java.io.File(latest.toString().substring(0,latest.toString().length()-9));
   io.opennoodoe.app.installer.InstallJournal journal=new io.opennoodoe.app.installer.InstallJournal(latest);
   String state=journal.state();boolean pending=!java.util.Arrays.asList("EMPTY","IMPORTED","STOCK_IDENTIFIED","BOOTSTRAP_IDENTIFIED","CFW_CONFIRMED","CURRENT_CFW_CONFIRMED","STOCK_RETURN_CONFIRMED").contains(state);
   prefs.edit().putBoolean(key("pending"),pending).putString(key("operation"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0343,"이전 작업: ")+state).commit();
  }catch(java.io.IOException damaged){prefs.edit().putBoolean(key("pending"),true).putString(key("operation"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0344,"이전 작업 기록 확인 필요")).commit();}
 }
 private static boolean install(String op){return java.util.Arrays.asList("stock-install-bootstrap","guided-bootstrap","guided-bootstrap-restore","update-cfw","diagnostic-cfw","diagnostic-return","restore-stock","uninstall-stock","cfw-verify","stock-return-check").contains(op);}
 synchronized void begin(String op){
  running=true;
  if(!op.equals("inspect-device")&&!op.equals("import")&&!op.equals("diagnostics")&&!op.equals("export")&&!op.equals("rides-export"))operation=op;
  if(install(op)){prefs.edit().putBoolean(key("pending"),true).putString(key("operation"),op).commit();}
 }
 synchronized void role(String value){if(value.equals("stock")||value.equals("bootstrap")||value.equals("product")||value.equals("diagnostic")||value.equals("unknown"))role=value;}
 synchronized void finish(String op,boolean failed,String detail){
  running=false;
  if(failed){recovery=true;error=detail;return;}
  if(op.equals("stock-install-bootstrap")){if(role.equals("bootstrap"))clear();else{role="unknown";recovery=false;error="";}}
  else if(op.equals("bootstrap-connect")){role="bootstrap";clear();}
  else if(op.startsWith("guided-bootstrap")||op.equals("update-cfw")||op.equals("cfw-verify")){role="product";clear();}
  else if(op.equals("diagnostic-cfw")){role="diagnostic";clear();}
  else if(op.equals("diagnostic-return")){role="diagnostic";recovery=true;error=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0345,"본체에서 순정 복귀를 확인해 주세요.");}
  else if(op.equals("stock-return-check")){role="stock";clear();}
  else if((op.equals("restore-stock")||op.equals("uninstall-stock"))){role="unknown";recovery=true;error=io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0346,"복원 요청을 보냈어요. 순정으로 돌아왔는지 확인해 주세요.");}
  else if(op.equals("companion")&&!recovery){role="product";}
  else if(op.equals("inspect-device")&&!prefs.getBoolean(key("pending"),false)){recovery=false;error="";}
 }
 private void clear(){recovery=false;error="";prefs.edit().putBoolean(key("pending"),false).apply();}
 /** A CDM appearance is not evidence that the peer is Product. In particular,
  * bonding Bootstrap must not start a competing companion protocol session. */
 synchronized boolean automaticDrivingAllowed(){
  if(recovery||prefs.getBoolean(key("pending"),false)||role.equals("stock")||role.equals("bootstrap")||role.equals("diagnostic"))return false;
  return role.equals("product")||(!operation.equals("stock-install-bootstrap")&&!operation.equals("bootstrap-connect"));
 }
 synchronized View view(){return new View(address,role,operation,error,running,recovery);}
 synchronized void reset(){address=null;role="unknown";operation="";error="";running=false;recovery=false;}
}
