package io.opennoodoe.app.installer;
import android.annotation.SuppressLint;
import android.bluetooth.*;
import android.content.*;
import android.os.Build;
import java.io.IOException;
import java.util.concurrent.*;

/** Refresh the public SDP service cache without removing the Android bond.
 * Only the selected device's ACTION_UUID ends the bounded discovery wait. */
final class SppDiscovery {
 @SuppressLint("MissingPermission")
 static void refresh(Context context,BluetoothDevice device,StockUpdateSession.Progress progress,int attempt)throws IOException {
  if(context==null)return;
  CountDownLatch received=new CountDownLatch(1);
  BroadcastReceiver receiver=new BroadcastReceiver(){public void onReceive(Context c,Intent intent){
   BluetoothDevice peer=intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
   if(BluetoothDevice.ACTION_UUID.equals(intent.getAction())&&peer!=null&&peer.getAddress().equals(device.getAddress()))received.countDown();
  }};
  if(Build.VERSION.SDK_INT>=33)context.registerReceiver(receiver,new IntentFilter(BluetoothDevice.ACTION_UUID),Context.RECEIVER_EXPORTED);
  else context.registerReceiver(receiver,new IntentFilter(BluetoothDevice.ACTION_UUID));
  try{
   progress.connection("sdp_refresh",attempt,device.getBondState());
   boolean requested=device.fetchUuidsWithSdp();
   boolean finished=requested&&received.await(5,TimeUnit.SECONDS);
   progress.connection(finished?"sdp_reply":requested?"sdp_wait_expired":"sdp_not_started",attempt,device.getBondState());
  }catch(InterruptedException e){Thread.currentThread().interrupt();throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0609,"SPP 서비스 조회 중단"),e);}
  finally{context.unregisterReceiver(receiver);}
 }
}
