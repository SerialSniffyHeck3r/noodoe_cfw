package io.opennoodoe.app.installer;

import android.annotation.SuppressLint;
import android.bluetooth.*;
import android.os.SystemClock;
import android.content.Context;
import java.io.*;
import java.util.UUID;
import java.util.concurrent.*;

/** No bootstrap and no autosync: protocol selection belongs to the caller. */
public final class AndroidInstallerTransport implements InstallerTransport {
    private final BluetoothSocket socket;
    private final InputStream input;
    private final OutputStream output;
    private final ScheduledExecutorService deadlines;
    public AndroidInstallerTransport(String address)throws IOException{this(null,address,text->{});}
    @SuppressLint("MissingPermission")
    public AndroidInstallerTransport(Context context,String address,StockUpdateSession.Progress progress) throws IOException {
        this(context,address,progress,false);
    }
    @SuppressLint("MissingPermission")
    public AndroidInstallerTransport(Context context,String address,StockUpdateSession.Progress progress,boolean bootHandoff) throws IOException {
        BluetoothAdapter adapter=BluetoothAdapter.getDefaultAdapter();
        if(adapter==null||!adapter.isEnabled())throw new IOException("Enable Bluetooth first");
        BluetoothDevice device=adapter.getRemoteDevice(address);
        adapter.cancelDiscovery();
        // Bonding is a separate bounded operation. A 15s RFCOMM deadline must
        // not close the socket while the rider is still approving Android UI.
        AndroidBondSession.ensure(context,device,progress);
        if(bootHandoff){
         SppDiscovery.refresh(context,device,progress,1);
        }
        deadlines=Executors.newSingleThreadScheduledExecutor();
        try{
         socket=SppConnectPolicy.connect(new SppConnectPolicy.Attempt<BluetoothSocket>(){
          public void prepareRetry(int attempt)throws IOException{
           try{Thread.sleep(1000);}catch(InterruptedException e){Thread.currentThread().interrupt();throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0347,"SPP 연결 중단"),e);}
           adapter.cancelDiscovery();AndroidBondSession.ensure(context,device,progress);SppDiscovery.refresh(context,device,progress,attempt);
          }
          public BluetoothSocket open(int attempt)throws IOException{
           progress.connection("secure_connect_start",attempt,device.getBondState());
           if(!bootHandoff)progress.update(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0348,"Bluetooth 보안 연결 중 · ")+attempt+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0349," / 3회. 기존 페어링은 유지해요."));
           BluetoothSocket candidate=device.createRfcommSocketToServiceRecord(UUID.fromString("00001101-0000-1000-8000-00805F9B34FB"));
           TransportDeadline guard=new TransportDeadline(deadlines,15000,()->{try{candidate.close();}catch(IOException ignored){}});
           try{
            candidate.connect();guard.close();if(guard.expired())throw new IOException("SPP connect timeout");
            candidate.getInputStream();candidate.getOutputStream();
            progress.connection("secure_connect_ready",attempt,device.getBondState());return candidate;
           }catch(IOException|RuntimeException e){
            try{candidate.close();}catch(IOException close){e.addSuppressed(close);}
            progress.connection(guard.expired()?"secure_connect_timeout":"secure_connect_failed",attempt,device.getBondState());throw e;
           }finally{guard.close();}
          }
         },bootHandoff?1:3);
        }catch(IOException|RuntimeException e){deadlines.shutdownNow();throw e;}
        try{input=socket.getInputStream();output=socket.getOutputStream();}
        catch(IOException|RuntimeException e){deadlines.shutdownNow();try{socket.close();}catch(IOException close){e.addSuppressed(close);}throw e;}
    }
    @Override public void send(byte[] bytes)throws IOException {
        TransportDeadline deadline=new TransportDeadline(deadlines,15000,()->{try{socket.close();}catch(IOException ignored){}});
        try{output.write(bytes);output.flush();}finally{deadline.close();}
        if(deadline.expired())throw new IOException("SPP write timeout; result unknown, query before retry");
    }
    @Override public byte[] receive(long timeoutMs)throws IOException {
        long until=SystemClock.elapsedRealtime()+timeoutMs;
        while(SystemClock.elapsedRealtime()<until) {
            if(Thread.currentThread().isInterrupted())throw new IOException("Installer interrupted; result unknown");
            int available=input.available();
            if(available>0){byte[] b=new byte[Math.min(4096,available)];int n=input.read(b);if(n<0)throw new EOFException();return java.util.Arrays.copyOf(b,n);}
            SystemClock.sleep(5);
        }
        throw new IOException("SPP receive timeout; result unknown");
    }
    @Override public void close()throws IOException {deadlines.shutdownNow();socket.close();}
}
