package io.opennoodoe.app.companion;

import android.app.Activity;
import android.companion.*;
import android.content.Context;
import android.content.IntentSender;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.widget.Toast;

/** OS-mediated association of the selected MAC only. Pairing secrets stay in
 * Android; this neither replaces the phone app nor silently changes bonds. */
public final class CompanionAssociation {
 public static final int REQUEST=40;
 private CompanionAssociation(){}
 public static boolean supported(Context c){return Build.VERSION.SDK_INT>=26&&c.getPackageManager().hasSystemFeature(PackageManager.FEATURE_COMPANION_DEVICE_SETUP);}
 @SuppressWarnings("deprecation") public static boolean associated(Context c,String address){
  if(Build.VERSION.SDK_INT<26||address==null||!supported(c))return false;
  try{return c.getSystemService(CompanionDeviceManager.class).getAssociations().contains(address);}catch(RuntimeException e){return false;}
 }
 @SuppressWarnings("deprecation") public static void observe(Context c,String address){
  if(Build.VERSION.SDK_INT>=31&&associated(c,address))c.getSystemService(CompanionDeviceManager.class).startObservingDevicePresence(address);
 }
 public static void request(Activity host,String address){
  if(Build.VERSION.SDK_INT<26||!supported(host)||address==null){Toast.makeText(host,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0643,"먼저 기기를 선택해 주세요. 동반 기기 등록은 Android 8 이상에서 지원해요."),Toast.LENGTH_LONG).show();return;}
  // associate() enforces the manifest feature and OEM system availability.
  // A rejected system operation must leave the existing connection intact.
  try{Api26.request(host,address);}catch(RuntimeException failure){failed(host,failure);}
 }
 private static void failed(Activity host,Exception failure){
  android.util.Log.w("CompanionAssociation","Association unavailable: "+failure.getClass().getSimpleName());
  if(!host.isFinishing()&&!host.isDestroyed())Toast.makeText(host,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0644,"동반 기기 등록을 열 수 없어요. Bluetooth를 켜고 선택한 기기를 확인한 뒤 다시 시도해 주세요."),Toast.LENGTH_LONG).show();
 }
 /** Keep framework callback classes out of the API23-25 class-loading path. */
 @android.annotation.TargetApi(26)
 private static final class Api26 {
 static void request(Activity host,String address){
  if(associated(host,address)){try{observe(host,address);Toast.makeText(host,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0645,"동반 기기 등록을 확인했어요."),Toast.LENGTH_LONG).show();}catch(RuntimeException e){Toast.makeText(host,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0646,"자동 연결 권한을 앱 설정에서 확인해 주세요."),Toast.LENGTH_LONG).show();}return;}
  android.bluetooth.BluetoothManager bluetooth=host.getSystemService(android.bluetooth.BluetoothManager.class);
  try{if(bluetooth!=null&&bluetooth.getAdapter()!=null)bluetooth.getAdapter().cancelDiscovery();}
  catch(SecurityException denied){failed(host,denied);return;}
  BluetoothDeviceFilter filter=new BluetoothDeviceFilter.Builder().setAddress(address).build();
  AssociationRequest request=new AssociationRequest.Builder().addDeviceFilter(filter).setSingleDevice(true).build();
  host.getSystemService(CompanionDeviceManager.class).associate(request,new CompanionDeviceManager.Callback(){
   @Override public void onDeviceFound(IntentSender chooser){try{if(host.isFinishing()||host.isDestroyed())return;host.startIntentSenderForResult(chooser,REQUEST,null,0,0,0);}catch(IntentSender.SendIntentException|RuntimeException e){failed(host,e);}}
   @Override public void onFailure(CharSequence error){Toast.makeText(host,io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0647,"동반 기기를 등록하지 못했어요. Bluetooth와 선택한 기기를 확인해 주세요."),Toast.LENGTH_LONG).show();}
  },new Handler(Looper.getMainLooper()));
 }
 }
}
