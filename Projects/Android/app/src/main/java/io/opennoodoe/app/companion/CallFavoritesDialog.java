package io.opennoodoe.app.companion;
import android.Manifest;
import android.app.*;
import android.content.*;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.provider.ContactsContract.CommonDataKinds.Phone;
import android.widget.Toast;
import java.util.*;
/** Ordered ten-contact picker. Numbers are never shown in diagnostic logs. */
public final class CallFavoritesDialog {
 public static final int REQUEST=41;
 public static void show(Activity a){
  CallFavorites store=new CallFavorites(a);List<CallFavorites.Entry> entries=store.get();String[] labels=new String[entries.size()];
  for(int i=0;i<labels.length;i++)labels[i]=(i+1)+". "+(entries.get(i).name.isEmpty()?entries.get(i).number:entries.get(i).name);
  new AlertDialog.Builder(a).setTitle(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0633,"전화 즐겨찾기 · 최대 10개")).setItems(labels,(d,which)->{
   new AlertDialog.Builder(a).setTitle(labels[which]).setItems(new String[]{io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0634,"위로 이동"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0635,"아래로 이동"),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0636,"즐겨찾기에서 제거")},(x,action)->{
    if(action==0&&which>0)Collections.swap(entries,which,which-1);else if(action==1&&which+1<entries.size())Collections.swap(entries,which,which+1);else if(action==2)entries.remove(which);
    if(!store.save(entries))Toast.makeText(a,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0637,"연락처 순서를 저장하지 못했어요."),Toast.LENGTH_LONG).show();show(a);
   }).setNegativeButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0004,"취소"),null).show();
  }).setPositiveButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0638,"연락처 추가"),(d,w)->{
   if(entries.size()>=10){Toast.makeText(a,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0639,"최대 10개예요. 먼저 하나를 지워 주세요."),Toast.LENGTH_LONG).show();return;}
   if(a.checkSelfPermission(Manifest.permission.READ_CONTACTS)!=PackageManager.PERMISSION_GRANTED){a.requestPermissions(new String[]{Manifest.permission.READ_CONTACTS},42);return;}
   try{a.startActivityForResult(new Intent(Intent.ACTION_PICK,Phone.CONTENT_URI),REQUEST);}catch(ActivityNotFoundException e){Toast.makeText(a,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0640,"연락처 앱을 찾을 수 없어요."),Toast.LENGTH_LONG).show();}
  }).setNegativeButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0009,"닫기"),null).show();
 }
 public static void picked(Activity a,Uri uri){
  if(uri==null)return;try(Cursor c=a.getContentResolver().query(uri,new String[]{Phone.DISPLAY_NAME,Phone.NUMBER},null,null,null)){
   if(c!=null&&c.moveToFirst()){CallFavorites store=new CallFavorites(a);List<CallFavorites.Entry> list=store.get();String number=c.getString(1);
    if(number==null||number.isEmpty())return;for(CallFavorites.Entry e:list)if(e.number.equals(number)){show(a);return;}
    if(list.size()<10){list.add(new CallFavorites.Entry(c.getString(0),number));if(!store.save(list))Toast.makeText(a,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0641,"연락처를 저장하지 못했어요."),Toast.LENGTH_LONG).show();}}
  }catch(SecurityException e){Toast.makeText(a,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0642,"연락처 권한을 확인해 주세요."),Toast.LENGTH_LONG).show();}show(a);
 }
}
