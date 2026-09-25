package io.opennoodoe.app.maintenance;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.drawable.GradientDrawable;
import android.view.*;
import android.widget.*;
import io.opennoodoe.app.UiThemeSettings;

/** Screen-local navigation. Owns no device, package, transaction or service. */
final class HomeSections {
 private final Activity host;private final LinearLayout[] pages;private final Button[] tabs;private int selected;
 HomeSections(Activity host,LinearLayout parent,String[] labels){
  this.host=host;pages=new LinearLayout[labels.length];tabs=new Button[labels.length];
  LinearLayout bar=new LinearLayout(host);parent.addView(bar);
  for(int i=0;i<labels.length;i++){final int index=i;Button b=new Button(host);b.setText(labels[i]);b.setTextSize(15);b.setAllCaps(false);b.setOnClickListener(v->select(index));tabs[i]=b;bar.addView(b,new LinearLayout.LayoutParams(0,dp(host,48),1));}
  for(int i=0;i<labels.length;i++){pages[i]=new LinearLayout(host);pages[i].setOrientation(LinearLayout.VERTICAL);pages[i].setPadding(0,dp(host,12),0,dp(host,16));parent.addView(pages[i]);}
  select(0);
 }
 LinearLayout page(int index){return pages[index];}int selected(){return selected;}
 void select(int index){if(index<0||index>=pages.length)index=0;selected=index;
  for(int i=0;i<pages.length;i++){pages[i].setVisibility(i==index?View.VISIBLE:View.GONE);tabs[i].setSelected(i==index);tabs[i].setTypeface(null,i==index?android.graphics.Typeface.BOLD:android.graphics.Typeface.NORMAL);}
 }
 static LinearLayout fold(Activity host,LinearLayout parent,String title){
  Button toggle=new Button(host);toggle.setText(title+"  +");parent.addView(toggle);
  LinearLayout content=new LinearLayout(host);content.setOrientation(LinearLayout.VERTICAL);content.setVisibility(View.GONE);parent.addView(content);
  toggle.setOnClickListener(v->{boolean show=content.getVisibility()!=View.VISIBLE;content.setVisibility(show?View.VISIBLE:View.GONE);toggle.setText(title+(show?"  −":"  +"));});return content;
 }
 private static int dp(Activity a,int n){return Math.round(n*a.getResources().getDisplayMetrics().density);}
 static void styleButtons(View view,Activity host){
  if(view instanceof Button){Button b=(Button)view;boolean dark=UiThemeSettings.isDark(host);
   b.setAllCaps(false);b.setTextSize(15);b.setMinHeight(dp(host,48));
   int[][] states={{-android.R.attr.state_enabled},{android.R.attr.state_selected},{}};
   b.setTextColor(new ColorStateList(states,new int[]{dark?0xff68716e:0xff89918d,dark?0xff9ce8d1:0xff075b48,dark?0xffedf4f0:0xff192b24}));
   b.setBackgroundTintList(new ColorStateList(states,new int[]{dark?0xff1b211f:0xffe8eeea,dark?0xff25463b:0xffcbe8db,dark?0xff25302b:0xffe1eae4}));
  }else if(view instanceof ViewGroup){ViewGroup group=(ViewGroup)view;for(int i=0;i<group.getChildCount();i++)styleButtons(group.getChildAt(i),host);}
 }
}
