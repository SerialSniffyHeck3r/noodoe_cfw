package io.opennoodoe.app.installer;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothDevice;
import android.content.*;
import android.os.Build;
import android.os.SystemClock;
import java.io.IOException;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Register before createBond so fast completion cannot be lost. Only the
 * selected address wakes this wait; the real framework state authorizes SPP. */
final class AndroidBondSession {
 @SuppressLint("MissingPermission")
 static void ensure(Context context,BluetoothDevice device,StockUpdateSession.Progress progress)throws IOException {
  if(device.getBondState()==BluetoothDevice.BOND_BONDED)return;
  Semaphore changes=new Semaphore(0);
  BroadcastReceiver receiver=new BroadcastReceiver(){public void onReceive(Context c,Intent intent){
   BluetoothDevice peer=intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
   if(BluetoothDevice.ACTION_BOND_STATE_CHANGED.equals(intent.getAction())&&peer!=null&&device.getAddress().equals(peer.getAddress()))changes.release();
  }};
  if(context!=null){
   IntentFilter filter=new IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
   if(Build.VERSION.SDK_INT>=33)context.registerReceiver(receiver,filter,Context.RECEIVER_EXPORTED);
   else context.registerReceiver(receiver,filter);
  }
  try{BondSession.ensure(new BondSession.Peer(){
   public int state(){return device.getBondState();}
   public boolean request(){return device.createBond();}
   public void waitChange(long ms)throws InterruptedException{changes.tryAcquire(ms,TimeUnit.MILLISECONDS);}
  },SystemClock::elapsedRealtime,progress);}
  finally{if(context!=null)context.unregisterReceiver(receiver);}
 }
}
