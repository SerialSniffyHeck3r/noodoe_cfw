package io.opennoodoe.app.installer;

import io.opennoodoe.app.protocol.ByteCodec;
import java.io.*;
import java.util.*;
import java.util.zip.CRC32;

/** NDCP is deliberately separate from stock framing/bootstrap; one outstanding request only. */
public final class NdcpSession {
    public static final int STATUS=0x45, RECOVERY=0x48;
    public static final class DeviceRejected extends IOException {
        public final int opcode; public final long result;
        DeviceRejected(int opcode,long result){super("Device rejected opcode "+opcode+" result="+result);this.opcode=opcode;this.result=result;}
    }
    public NdcpSession(InstallerTransport transport) { this(transport,60000); }
    /** Boot health queries have no erase/hash work and must not monopolize the rollback window. */
    public NdcpSession(InstallerTransport transport,long timeoutMs) { this.client=new io.opennoodoe.app.protocol.ndcp.NdcpClient(transport,timeoutMs); }
    private final io.opennoodoe.app.protocol.ndcp.NdcpClient client;
    public static byte[] encode(int opcode,int sequence,byte[] payload,int flags){return io.opennoodoe.app.protocol.ndcp.NdcpClient.encode(opcode,sequence,payload,flags);}
    public byte[] request(int opcode,byte[] payload)throws IOException {
        try{return client.request(opcode,payload);}catch(io.opennoodoe.app.protocol.ndcp.NdcpClient.DeviceRejected e){throw new DeviceRejected(e.opcode,e.result);}
    }
    public byte[] status() throws IOException { byte[] r=request(STATUS,new byte[0]);if(r.length!=84)throw new IOException("Unsupported status ABI");return r; }
    /** Read the running source independently of the candidate ZIP. */
    public byte[] runningIdentity(int role)throws Exception {
        byte[] r;long deadline=System.nanoTime()+15_000_000_000L;
        while(true){try{r=request(0x58,new byte[0]);break;}catch(DeviceRejected waiting){
            if((waiting.result!=2&&waiting.result!=3)||System.nanoTime()>=deadline)throw waiting;Thread.sleep(100);}}
        if(r.length!=88||ByteCodec.u32le(r,0)!=0||ByteCodec.u32le(r,4)!=1||(role!=0&&ByteCodec.u32le(r,8)!=role))
            throw new IOException("Unexpected running firmware role/identity ABI");
        return r;
    }
    /** Exact candidate check belongs after installation, not before an old CFW update. */
    public long[] identity(RecoveryBundle bundle,InstallJournal journal,int role)throws Exception {
        byte[] r=runningIdentity(role);
        if(!Arrays.equals(Arrays.copyOfRange(r,24,56),hex(RecoveryBundle.sha(bundle.paddedImage(role==1?"bootstrap":"cfw")))))
            throw new IOException(role==1
                ?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0554,"실행 중인 Bootstrap과 선택한 ZIP이 달라요. Bootstrap을 올릴 때 쓴 ZIP으로 설치를 이어가세요.")
                :io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0555,"선택한 새 CFW가 아직 실행 중이 아니에요. 현재 버전 확인 후 ‘CFW 업데이트’를 진행하세요."));
        if(role==2)TargetBinding.verifyProduct(this,bundle,journal,r);
        else TargetBinding.verify(this,bundle,journal,r,role);
        long[] uid={ByteCodec.u32le(r,12),ByteCodec.u32le(r,16),ByteCodec.u32le(r,20)};
        String value=uid[0]+","+uid[1]+","+uid[2];
        if(journal.values.containsKey("uid")&&!value.equals(journal.values.getProperty("uid")))throw new IOException("Device UID changed");
        journal.values.setProperty("uid",value);return uid;
    }
    /** Host only accepts a full device-readback SHA; it never equates received bytes with verified bytes. */
    public void stage(RecoveryBundle bundle,InstallJournal journal,StockUpdateSession.Progress progress) throws Exception {
        journal.require("PROVISIONED", "CFW_CONFIRMED");
        byte[] current=status();
        if(ByteCodec.u32le(current,28)==0 || ByteCodec.u32le(current,32)!=TargetBinding.version(journal) || ByteCodec.u32le(current,48)!=0)
            throw new IOException("Bootstrap has not authorized staging, or a boot request is pending");
        byte[] image=bundle.paddedImage("cfw");String sha=RecoveryBundle.sha(image);
        long crc=crc(image,image.length); if(crc==0 || crc==0xffffffffL)throw new IOException("Reserved CRC");
        int tx=new java.security.SecureRandom().nextInt()&0x7fffffff;if(tx==0)tx=1;
        long version=RecoveryBundle.number(bundle.manifest,"cfw.major")|(RecoveryBundle.number(bundle.manifest,"cfw.minor")<<16);
        journal.values.setProperty("transaction",Integer.toString(tx));journal.values.setProperty("image.sha256",sha);
        journal.values.setProperty("version",Long.toString(version));journal.values.setProperty("crc",Long.toString(crc));
        byte[] begin=new byte[56];put(begin,0,tx);put(begin,4,version);put(begin,8,image.length);put(begin,12,crc);
        System.arraycopy(hex(sha),0,begin,16,32);put(begin,48,0x53544147);
        TransferCapabilities caps=TransferCapabilities.query(this);
        journal.save("NDCP_BEGIN_RESULT_UNKNOWN");request(0x40,begin);
        for(int offset=0;offset<image.length;) {
            int count=caps.count(offset,image.length-offset);byte[] chunk=new byte[8+count];put(chunk,0,tx);put(chunk,4,offset);System.arraycopy(image,offset,chunk,8,count);
            journal.values.setProperty("offset",Integer.toString(offset));journal.save("NDCP_DATA_RESULT_UNKNOWN");
            byte[] result=caps.localSources&&"scoped-v2".equals(journal.values.getProperty("backup.mode"))&&offset>=0x10000?request(0x49,words(tx,offset,count)):request(0x41,chunk);if(result.length<20||ByteCodec.u32le(result,12)!=offset+count)throw new IOException("NDCP offset mismatch");
            progress.stage("stage",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0556,"기록할 Gate와 CFW 이미지를 전송하고 있어요."),offset+count,image.length,"B");offset+=count;
        }
        journal.save("NDCP_FINISH_RESULT_UNKNOWN");byte[] result=request(0x42,words(tx));
        if(result.length!=52||ByteCodec.u32le(result,4)!=3||ByteCodec.u32le(result,16)!=image.length
                ||!Arrays.equals(Arrays.copyOfRange(result,20,52),hex(sha)))throw new IOException("Full device readback mismatch");
        journal.save("NDCP_VERIFIED");
    }
    public void commit(InstallJournal journal) throws Exception {
        journal.require("NDCP_VERIFIED");checkCurrent(journal,3);
        byte[] payload=new byte[40];put(payload,0,value(journal,"transaction"));put(payload,4,0x434f4d54);
        System.arraycopy(hex(journal.values.getProperty("image.sha256")),0,payload,8,32);
        journal.save("NDCP_COMMIT_RESULT_UNKNOWN");byte[] result;
        try {result=request(0x43,payload);}
        catch(DeviceRejected rejected){
            // A correlated rejection leaves this socket framed. Query only;
            // on timeout/lost response do not reuse the stream or retry COMMIT.
            BootstrapCommitDiagnostics diagnostic;
            try {diagnostic=new BootstrapCommitDiagnostics(request(0x85,new byte[0]));diagnostic.record(journal);}
            catch(IOException unavailable){rejected.addSuppressed(unavailable);throw rejected;}
            throw new IOException(diagnostic.summary(),rejected);
        }
        if(result.length<20||ByteCodec.u32le(result,4)!=4)throw new IOException("Unexpected commit state");journal.save("NDCP_COMMITTED");
    }
    public void reset(InstallJournal journal) throws Exception {
        journal.require("NDCP_COMMITTED");checkCurrent(journal,4);
        journal.save("NDCP_RESET_RESULT_UNKNOWN");request(0x47,words(value(journal,"transaction"),0x52535421));
        Thread.sleep(2000);journal.save("WAIT_CFW_BOOT");
    }
    public void reconcile(InstallJournal journal) throws Exception {
        byte[] r=status();long tx=value(journal,"transaction");
        long[] expected={TargetBinding.version(journal),value(journal,"version"),0x7f90,RecoveryBundle.APP_BYTES,value(journal,"crc")};
        boolean same=ByteCodec.u32le(r,8)==tx && Arrays.equals(Arrays.copyOfRange(r,52,84),hex(journal.values.getProperty("image.sha256")))
                &&ByteCodec.u32le(r,24)==expected[4]&&ByteCodec.u32le(r,12)==RecoveryBundle.APP_BYTES&&ByteCodec.u32le(r,16)==RecoveryBundle.APP_BYTES;
        if(same&&ByteCodec.u32le(r,4)==3&&ByteCodec.u32le(r,32)==TargetBinding.version(journal)&&ByteCodec.u32le(r,48)==0)journal.save("NDCP_VERIFIED");
        else if(same&&ByteCodec.u32le(r,4)==4&&metadataMatches(r,expected))journal.save("NDCP_COMMITTED");
        else { expected[4]=0; if(ByteCodec.u32le(r,8)==0&&metadataMatches(r,expected))journal.save("BOOT_REQUEST_CLEARED");
            else throw new IOException("Cannot reconcile transaction; no write was retried"); }
    }
    /** Local image recovery has its own explicit confirmation; no firmware bytes are uploaded here. */
    public byte[] recover(int action,InstallJournal journal) throws Exception {
        if(action<0||action>3)throw new IOException("Recovery action out of range");
        if(action==2) journal.require("RECOVERY_CONFIRM_REQUIRED");
        if(action!=0)journal.save(action==2?"RECOVERY_COMMIT_RESULT_UNKNOWN":"RECOVERY_REQUEST_RESULT_UNKNOWN");
        byte[] result=request(RECOVERY,words(action,action==1||action==2?0x53544f43:0));
        if(result.length!=32)throw new IOException("Unknown recovery response ABI");
        if(action==1&&ByteCodec.u32le(result,4)!=2||action==2&&ByteCodec.u32le(result,4)!=6)
            throw new IOException("Recovery confirmation/reset state not acknowledged");
        if(action==1)journal.save("RECOVERY_CONFIRM_REQUIRED");
        else if(action==2)journal.save("RECOVERY_RUNNING");
        else if(action==3)journal.save("RECOVERY_CANCELLED");
        return result;
    }
    private void checkCurrent(InstallJournal journal,int state) throws Exception {
        byte[] r=status();if(ByteCodec.u32le(r,4)!=state||ByteCodec.u32le(r,8)!=value(journal,"transaction")
                ||!Arrays.equals(Arrays.copyOfRange(r,52,84),hex(journal.values.getProperty("image.sha256"))))throw new IOException("Device transaction changed");
    }
    private static boolean metadataMatches(byte[] r,long[] metadata) {for(int i=0;i<5;i++)if(ByteCodec.u32le(r,32+4*i)!=metadata[i])return false;return true;}
    private static long value(InstallJournal j,String key)throws IOException{return RecoveryBundle.number(j.values,key);}
    private static void put(byte[] b,int at,long value){ByteCodec.putU32le(b,at,value);}
    public static byte[] words(long... values){byte[] b=new byte[values.length*4];for(int i=0;i<values.length;i++)put(b,i*4,values[i]);return b;}
    public static long crc(byte[] b,int length){CRC32 crc=new CRC32();crc.update(b,0,length);return crc.getValue();}
    public static byte[] hex(String value){if(value==null||!value.matches("[0-9a-f]{64}"))throw new IllegalArgumentException("Invalid SHA");byte[] b=new byte[32];for(int i=0;i<32;i++)b[i]=(byte)Integer.parseInt(value.substring(i*2,i*2+2),16);return b;}
}
