package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import java.io.*;

/** Reads one stable ring generation through the storage-owner mailbox. An
 * interrupted export remains .partial; never advertises it as a complete log. */
public final class DeviceLogDownload {
    public static File run(NdcpSession session,File directory,StockUpdateSession.Progress progress)throws Exception {
        byte[] info=session.request(0x5d,new byte[0]);
        if(info.length!=40||ByteCodec.u32le(info,4)!=1||ByteCodec.u32le(info,12)!=0)throw new IOException("Device log unavailable");
        long generation=ByteCodec.u32le(info,28);
        if(!directory.isDirectory()&&!directory.mkdirs())throw new IOException("Log directory unavailable");
        File partial=new File(directory,"device-log-"+System.currentTimeMillis()+".partial");
        try(FileOutputStream out=new FileOutputStream(partial)){
            for(int offset=0;offset<0x40000;offset+=256){
                long token=offset/256+1;
                session.request(0x5e,NdcpSession.words(token,generation,offset));
                byte[] chunk=null;long until=System.nanoTime()+5_000_000_000L;
                while(System.nanoTime()<until){
                    try{chunk=session.request(0x5f,NdcpSession.words(token));break;}
                    catch(NdcpSession.DeviceRejected pending){if(pending.result!=1)throw pending;Thread.sleep(10);}
                }
                if(chunk==null||chunk.length!=260)throw new IOException("Incomplete device log chunk");
                out.write(chunk,4,256);
                if((offset&4095)==0)progress.update(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0428,"기기 기록 읽는 중 ")+offset+" / 262144");
            }
            byte[] end=session.request(0x5d,new byte[0]);
            if(end.length!=40||ByteCodec.u32le(end,28)!=generation)throw new IOException("Device log rotated; export must be retried");
            out.getFD().sync();
        }
        File complete=new File(directory,partial.getName().replace(".partial",".bin"));
        if(!partial.renameTo(complete))throw new IOException("Cannot publish verified log");
        return complete;
    }
}
