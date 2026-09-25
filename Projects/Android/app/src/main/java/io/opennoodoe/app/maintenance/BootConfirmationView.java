package io.opennoodoe.app.maintenance;

import android.app.Activity;
import android.app.AlertDialog;
import android.graphics.Typeface;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import io.opennoodoe.app.R;

/** Required user action stays OUTSIDE the scrolling progress/history. A dialog
 * draws attention once per request; dismissing it never counts as approval.
 * All click callbacks carry the request key, so a stale window cannot approve
 * a replacement connection/candidate. The service owns the operation. */
final class BootConfirmationView extends LinearLayout {
 interface Approval {void accept(String request);}
 private final Activity activity;
 private final Approval approve;
 private final TextView title,countdown;
 private final Button button;
 private AlertDialog dialog;
 private String key="",announced="";
 BootConfirmationView(Activity a,Approval approve){
  super(a);activity=a;this.approve=approve;setOrientation(VERTICAL);
  int pad=(int)(12*a.getResources().getDisplayMetrics().density);setPadding(pad,pad,pad,pad);
  setBackgroundColor(io.opennoodoe.app.UiThemeSettings.isDark(a)?0xff313523:0xffffedbd);
  title=new TextView(a);title.setText(R.string.boot_screen_action);title.setTextSize(20);title.setTypeface(null,Typeface.BOLD);title.setTextColor(InstallerTheme.warning(a));addView(title);
  countdown=new TextView(a);countdown.setTextSize(15);addView(countdown);
  button=new Button(a);button.setText(R.string.boot_screen_yes);button.setTextSize(17);button.setAllCaps(false);addView(button);
  button.setMinHeight((int)(48*a.getResources().getDisplayMetrics().density));button.setTextColor(0xffffffff);
  button.setBackgroundTintList(android.content.res.ColorStateList.valueOf(0xff15654f));
  button.setOnClickListener(v->accept(key));setVisibility(GONE);
 }
 void render(boolean ready,String request,long seconds,boolean foreground){
  if(!ready||request.isEmpty()){
   key="";setVisibility(GONE);dismiss();return;
  }
  if(!request.equals(key)){dismiss();key=request;}
  setVisibility(VISIBLE);button.setEnabled(true);
  countdown.setText(activity.getString(R.string.boot_screen_deadline,seconds));
  if(dialog!=null)dialog.setMessage(message(seconds));
  if(foreground&&!activity.isFinishing()&&!request.equals(announced)){
   announced=request;final String captured=request;
   dialog=new AlertDialog.Builder(activity).setTitle(R.string.boot_screen_action)
    .setMessage(message(seconds)).setPositiveButton(R.string.boot_screen_yes,(d,w)->accept(captured))
    .setNegativeButton(R.string.boot_screen_not_yet,(d,w)->{}).create();
   dialog.setCanceledOnTouchOutside(false);dialog.setOnDismissListener(d->dialog=null);dialog.show();
   dialog.getButton(AlertDialog.BUTTON_POSITIVE).setTextColor(io.opennoodoe.app.UiThemeSettings.isDark(activity)?0xff9ce8d1:0xff075b48);
  }
 }
 private String message(long seconds){return activity.getString(R.string.boot_screen_instructions)+"\n\n"+activity.getString(R.string.boot_screen_deadline,seconds);}
 private void accept(String request){if(request.isEmpty()||!request.equals(key))return;button.setEnabled(false);approve.accept(request);}
 void pause(){dismiss();announced="";}
 void dismiss(){if(dialog!=null){AlertDialog old=dialog;dialog=null;old.setOnDismissListener(null);old.dismiss();}}
}
