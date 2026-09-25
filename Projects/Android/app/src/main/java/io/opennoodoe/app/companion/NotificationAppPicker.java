package io.opennoodoe.app.companion;
import android.app.*;
import android.content.*;
import android.content.pm.*;
import android.graphics.drawable.Drawable;
import android.service.notification.StatusBarNotification;
import android.view.*;
import android.widget.*;
import java.util.*;

/** Package identity remains the saved key; labels/icons are presentation only.
 * Missing/uninstalled packages keep their existing selection and a fallback. */
public final class NotificationAppPicker {
 public static final class Row {
  public final String name,packageName;public final Drawable icon;
  Row(Context c,String pkg){packageName=pkg;String label=pkg;Drawable image=null;
   try{ApplicationInfo app=c.getPackageManager().getApplicationInfo(pkg,0);label=c.getPackageManager().getApplicationLabel(app).toString();image=c.getPackageManager().getApplicationIcon(app);}catch(PackageManager.NameNotFoundException|SecurityException ignored){}
   name=label;icon=image==null?c.getPackageManager().getDefaultActivityIcon():image;
  }
 }
 public static List<Row> rows(Context c,Collection<String> packages){ArrayList<Row> out=new ArrayList<>();for(String pkg:new HashSet<>(packages))out.add(new Row(c,pkg));final java.text.Collator collator=java.text.Collator.getInstance();java.util.Collections.sort(out,(a,b)->{int order=collator.compare(a.name,b.name);return order!=0?order:a.packageName.compareTo(b.packageName);});return out;}
 static View row(Context c,Row item,Set<String> selected){
  int pad=dp(c,12);LinearLayout line=new LinearLayout(c);line.setPadding(pad,pad,pad,pad);line.setGravity(Gravity.CENTER_VERTICAL);
  ImageView icon=new ImageView(c);icon.setImageDrawable(item.icon);icon.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);line.addView(icon,new LinearLayout.LayoutParams(dp(c,40),dp(c,40)));
  LinearLayout text=new LinearLayout(c);text.setOrientation(LinearLayout.VERTICAL);text.setPadding(pad,0,pad,0);
  TextView label=new TextView(c);label.setText(item.name);label.setTextSize(17);text.addView(label);
  TextView pkg=new TextView(c);pkg.setText(item.packageName);pkg.setTextSize(12);pkg.setAlpha(.7f);text.addView(pkg);line.addView(text,new LinearLayout.LayoutParams(0,-2,1));
  CheckBox box=new CheckBox(c);box.setChecked(selected.contains(item.packageName));box.setContentDescription(item.name+", "+item.packageName);line.addView(box);
  box.setOnCheckedChangeListener((b,on)->{if(on)selected.add(item.packageName);else selected.remove(item.packageName);});line.setOnClickListener(v->box.setChecked(!box.isChecked()));return line;
 }
 private static int dp(Context c,int n){return Math.round(n*c.getResources().getDisplayMetrics().density);}
 public static void show(Activity owner){
  Set<String> selected=new HashSet<>(owner.getSharedPreferences("companion",0).getStringSet("notification.apps",Collections.emptySet()));Set<String> candidates=new HashSet<>(selected);
  CompanionNotifications listener=CompanionNotifications.instance;
  if(listener!=null)try{StatusBarNotification[] active=listener.getActiveNotifications();if(active!=null)for(StatusBarNotification n:active)candidates.add(n.getPackageName());}catch(SecurityException ignored){}
  for(ResolveInfo info:owner.getPackageManager().queryIntentActivities(new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER),0))if(info.activityInfo!=null)candidates.add(info.activityInfo.packageName);
  candidates.remove(owner.getPackageName());List<Row> rows=rows(owner,candidates);
  ListView list=new ListView(owner);list.setAdapter(new BaseAdapter(){public int getCount(){return rows.size();}public Row getItem(int p){return rows.get(p);}public long getItemId(int p){return p;}public View getView(int p,View old,ViewGroup parent){return row(owner,getItem(p),selected);}});
  new AlertDialog.Builder(owner).setTitle(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0169,"알림을 보낼 앱")).setView(list)
   .setPositiveButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0170,"저장"),(d,w)->owner.getSharedPreferences("companion",0).edit().putStringSet("notification.apps",selected).apply())
   .setNegativeButton(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0004,"취소"),null).show();
 }
}
