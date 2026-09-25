package io.opennoodoe.app.installer;

import io.opennoodoe.app.protocol.*;
import java.io.*;
import java.util.*;

/** Stock-only session. No preferences, clock sync, generic failure-DONE or automatic reconnect. */
public final class StockUpdateSession {
    public interface ScreenCheck {void poll()throws Exception;}
    public static final class ScreenConfirmationTimeout extends IOException {
        public ScreenConfirmationTimeout(String message){super(message);}
    }
    public interface Progress {
        void update(String text);
        default void stage(String key,String text,long done,long total,String unit){update(text+(total>0?"\n"+done+" / "+total+" "+unit:""));}
        default void reply(){}
        default void screenPrompt(){}
        default void awaitScreen(String candidate,long deadline)throws Exception {
            throw new IOException("Confirm the new CFW screen in the phone app before continuing.");
        }
        default void awaitScreen(String candidate,long deadline,ScreenCheck check)throws Exception {awaitScreen(candidate,deadline);}
        default void installStatus(InstallProgressSnapshot view)throws IOException {}
        default void role(String role){}
        default void deviceStatus(BootstrapProgress.View view)throws IOException {}
        default void connection(String phase,int attempt,int bond)throws IOException {}
    }
    private final InstallerTransport transport;
    private final Progress observer;
    private final SequenceStreamDecoder decoder = new SequenceStreamDecoder();
    private int sendIndex, receiveIndex = -1;
    private byte[] identityReply;
    private byte[] pendingFramed;
    public StockUpdateSession(InstallerTransport transport) { this(transport,text->{}); }
    public StockUpdateSession(InstallerTransport transport,Progress observer) { this.transport = transport;this.observer=observer; }
    /** Exact read-only reply for a later, independently reviewed bundle target. */
    public byte[] identityReply() throws IOException {
        if(identityReply==null)throw new IOException("No stock identity reply");
        return identityReply.clone();
    }

    public DeviceInfo identify() throws IOException {
        observer.connection("stock_raw_start",0,-1);
        transport.send(new byte[]{5,0,0,0,0});
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        long deadline = System.nanoTime() + 5_000_000_000L;
        while (System.nanoTime() < deadline) {
            byte[] chunk = transport.receive(remaining(deadline)); bytes.write(chunk,0,chunk.length);
            byte[] data = bytes.toByteArray();
            if (data.length > 8192 || (data.length > 0 && (data[0] & 255) != 0x85))
                throw new IOException("Invalid stock bootstrap");
            if (data.length >= 5) {
                long length = ByteCodec.u32le(data,1);
                if (length < 13 || length > 4096) throw new IOException("Invalid stock bootstrap length");
                if (data.length >= length + 5) {
                    // RFCOMM is a stream: the first framed notification may
                    // share the read containing the end of this raw reply.
                    // Preserve it for the framed parser instead of requiring
                    // Android's read boundary to be a protocol boundary.
                    pendingFramed=Arrays.copyOfRange(data,(int)length+5,data.length);
                    DeviceInfo raw = DeviceInfo.fromRawBootstrap(Arrays.copyOfRange(data,6,(int)length+5));
                    if (!raw.supportsFramedProtocol()) throw new IOException("Unsupported stock protocol");
                    observer.connection("stock_raw_ready",0,-1);
                    observer.reply();
                    identityReply=request(5, CommandFrame.READ, new byte[0],0,5000);
                    observer.connection("stock_framed_ready",0,-1);
                    observer.reply();
                    return DeviceInfo.fromFramedReply(identityReply);
                }
            }
        }
        throw new IOException("Stock bootstrap timed out");
    }
    /** Same packet is never resent at application level; ambiguous phases remain in the journal. */
    public byte[] request(int command, int attribute, byte[] payload, int session, int timeoutMs) throws IOException {
        ReplyExpectation expectation = ReplyExpectation.fromRequest(command,payload);
        byte[] wire = new SequenceFrame(receiveIndex < 0 ? 0 : SequenceFrame.CONTROL_ACK,
                sendIndex,Math.max(0,receiveIndex),session,new CommandFrame(command,attribute,payload).encode()).encode();
        int requestIndex = sendIndex;
        transport.send(wire);
        long deadline = System.nanoTime() + timeoutMs * 1_000_000L;
        boolean acknowledged = false;
        byte[] matched = null;
        while (System.nanoTime() < deadline) {
            byte[] chunk;
            if(pendingFramed!=null&&pendingFramed.length>0){chunk=pendingFramed;pendingFramed=null;}
            else chunk = transport.receive(remaining(deadline));
            for (SequenceFrame frame : decoder.feed(chunk,chunk.length)) {
                if ((frame.getControl() & SequenceFrame.CONTROL_RESET) != 0) throw new IOException("Stock sequence reset; reconnect read-only");
                // Stock also piggybacks an ACK index on data without setting the pure-ACK bit.
                if ((frame.isAck() || frame.getPayload().length > 0) && frame.getAckIndex() == requestIndex) acknowledged = true;
                if (frame.getPayload().length == 0) continue;
                int received = frame.getPacketIndex();
                transport.send(new SequenceFrame(SequenceFrame.CONTROL_ACK,
                        acknowledged ? (requestIndex + 1) % 128 : requestIndex, received,0,new byte[0]).encode());
                if (received == receiveIndex) continue;
                if (received < 128 || (receiveIndex >= 0 && received != (receiveIndex == 255 ? 128 : receiveIndex+1)))
                    throw new IOException("Out-of-order stock response");
                receiveIndex = received;
                for (CommandFrame reply : CommandFrame.decodeMany(frame.getPayload())) {
                    if ((reply.getAttribute() & CommandFrame.REPLY) != 0 && expectation.matches(reply)) matched=reply.getPayload();
                }
            }
            if (acknowledged && matched != null) {
                sendIndex = (requestIndex + 1) % 128;
                return matched;
            }
        }
        throw new IOException("Stock reply timed out; operation result may be unknown");
    }
    public void installBootstrap(RecoveryBundle bundle, InstallJournal journal, Progress progress) throws Exception {
        journal.require("IMPORTED", "STOCK_IDENTIFIED", "STOCK_RETURN_CONFIRMED");
        progress.stage("stock-handshake",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0613,"Bluetooth는 연결됐어요. 순정 프로토콜 응답을 확인해요."),0,0,"");
        DeviceInfo info=identify(); bundle.checkStock(info);progress.reply();progress.role("stock");
        String address=journal.values.getProperty("address");
        if(address!=null&&!address.equalsIgnoreCase(info.mac))
            throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0614,"선택한 누도와 순정 Bluetooth 주소가 달라요. 전송하지 않았어요."));
        int major=(int)RecoveryBundle.number(bundle.manifest,"bootstrap.major");
        int minor=(int)RecoveryBundle.number(bundle.manifest,"bootstrap.minor");
        if (major < info.firmwareMajor || major == info.firmwareMajor && minor <= info.firmwareMinor)
            throw new IOException("Stock requires a strictly newer firmware version");
        byte[] riding=request(0x0c,CommandFrame.READ,new byte[0],0,5000);
        RidingStatus status=RidingStatus.fromReply(riding);
        if (status.status != 0 || status.currentSpeed != 0 || !status.keyStateKnown || !status.keyOn)
            throw new IOException("Fresh stationary IGN ON reading required");
        if (journal.state().equals("STOCK_RETURN_CONFIRMED")) {
            progress.stage("stock-handshake",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0615,"순정 복귀와 현재 기기를 확인했어요. 이전 기록을 보관하고 새 설치를 준비해요."),0,0,"");
            journal.restartAfterStockReturn();
        }
        byte[] image=bundle.paddedImage("bootstrap");
        int task=1; byte[] id=new byte[4]; ByteCodec.putU16le(id,0,major); ByteCodec.putU16le(id,2,minor);
        long crc=FileTransferPayloads.paddedCrc32(image);
        journal.values.setProperty("stock.device",info.toString()); journal.values.setProperty("image.sha256",RecoveryBundle.sha(image));
        journal.save("STOCK_BEGIN_RESULT_UNKNOWN");
        success(request(0x0a,2,FileTransferPayloads.negotiate(task,2,0x0800,image.length,1,id),0,10000));
        journal.save("STOCK_START_RESULT_UNKNOWN");
        success(request(0x0b,2,FileTransferPayloads.controlByFileId(task,1,1,1,crc,image.length),0,10000));
        for(int offset=0;offset<image.length;) {
            int count=Math.min(11816,image.length-offset);
            journal.values.setProperty("offset",Integer.toString(offset)); journal.save("STOCK_DATA_RESULT_UNKNOWN");
            byte[] reply=request(0x0d,2,FileTransferPayloads.data(task,1,image,offset,count),1,15000);
            success(reply); FileTransferPayloads.DataReply accepted=FileTransferPayloads.DataReply.parse(reply);
            if(accepted.taskId!=task || accepted.transferId!=1 || accepted.cumulativeSize!=offset+count)
                throw new IOException("Stock data offset mismatch");
            offset+=count;progress.reply();progress.stage("stock-transfer",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0616,"Bootstrap을 순정 업데이트 경로로 보내고 있어요."),offset,image.length,"B");
        }
        journal.save("STOCK_TERMINATE_RESULT_UNKNOWN");
        success(request(0x0b,2,FileTransferPayloads.controlByFileId(task,2,1,1,crc,image.length),0,15000));
        journal.save("STOCK_DONE_RESULT_UNKNOWN");
        success(request(0x0a,2,FileTransferPayloads.negotiate(task,2,0x0800,image.length,3,id),0,15000));
        journal.save("STOCK_ACCEPTED_WAIT_IGN_OFF");
        progress.stage("stock-reboot",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0617,"전송 검증 응답 완료. 상시 전원은 유지하고 IGN만 OFF → Bootstrap 화면 확인 후 ON으로 바꿔 주세요. 설치 도구 연결 확인이 필요해요."),0,0,"");
    }
    private static long remaining(long deadline) { return Math.max(1,(deadline-System.nanoTime())/1_000_000); }
    private static void success(byte[] reply) throws IOException {
        if(reply.length<2 || ByteCodec.u16le(reply,0)!=0) throw new IOException("Stock rejected transfer: "+(reply.length<2?-1:ByteCodec.u16le(reply,0)));
    }
}
