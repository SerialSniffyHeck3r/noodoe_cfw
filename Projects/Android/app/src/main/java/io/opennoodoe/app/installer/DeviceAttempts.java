package io.opennoodoe.app.installer;
import java.io.*;
/** Evidence is indexed by actual peer address until UID binding completes.
 * Switching peers never consumes another peer's journal. Switching a ZIP on
 * the SAME peer cannot hide an ambiguous mutation behind fresh UI state. */
final class DeviceAttempts {
 static void checkUid(InstallJournal current,String uid)throws IOException {
  File[] files=current.evidenceRoot().listFiles((d,n)->n.endsWith(".journal"));if(files==null)return;
  for(File f:files){
   // A plain read only selects candidates; a matching record is checked with
   // the durable journal checksum before any of its claims are accepted.
   java.util.Properties hint=new java.util.Properties();try(InputStream in=new FileInputStream(f)){hint.load(in);}
   if(!uid.equals(hint.getProperty("uid")))continue;
   InstallJournal old=new InstallJournal(f);
   if(current.values.getProperty("address","").equalsIgnoreCase(old.values.getProperty("address",""))&&
      current.values.getProperty("bundle","").equals(old.values.getProperty("bundle","")))continue;
   String state=old.state();
   if(state.startsWith("NDCP_LOCAL_")||state.contains("COMMIT_RESULT_UNKNOWN")||state.contains("RESET_RESULT_UNKNOWN")||state.equals("WAIT_CFW_BOOT")||state.equals("UNINSTALL_DEVICE_RUNNING")||state.equals("UNINSTALL_COMMITTED"))
    throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0425,"동일 UID의 이전 연결에 결과 미확인 작업이 남아 있어요. 이전 기기/ZIP 선택으로 돌아가 결과부터 조회해 주세요. 기록은 보존됩니다."));
  }
 }
 static void check(File root,String address,String bundle)throws IOException {
  String suffix="-"+address.replace(":","")+".journal";
  File[] files=root.listFiles((dir,name)->name.toLowerCase(java.util.Locale.ROOT).endsWith(suffix.toLowerCase(java.util.Locale.ROOT)));if(files==null)return;
  for(File f:files){InstallJournal j=new InstallJournal(f);
   if(!address.equalsIgnoreCase(j.values.getProperty("address",""))||bundle.equals(j.values.getProperty("bundle")))continue;
   String state=j.state();
   if(state.startsWith("NDCP_LOCAL_")||state.contains("COMMIT_RESULT_UNKNOWN")||state.contains("RESET_RESULT_UNKNOWN")||state.equals("WAIT_CFW_BOOT")||state.equals("UNINSTALL_DEVICE_RUNNING")||state.equals("UNINSTALL_COMMITTED")||state.equals("CREATE_COMMIT_RESULT_UNKNOWN"))
    throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0426,"이 기기에 결과 미확인 작업이 남아 있어요. 이전 ZIP ")+j.values.getProperty("bundle","")+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0427,"을 선택해 상태 조회부터 해 주세요. 앱 초기화로 설치 확정 여부를 지우지 않습니다."));
  }
 }
}
