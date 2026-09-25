package io.opennoodoe.app.installer;

import io.opennoodoe.app.protocol.ByteCodec;
import io.opennoodoe.app.protocol.DeviceInfo;
import java.io.*;
import java.util.Arrays;

/** Synchronous core called exclusively by the foreground service worker. UI never owns the socket. */
public final class InstallerController {
    public interface Connections {
        InstallerTransport open() throws Exception;
        default InstallerTransport open(StockUpdateSession.Progress p)throws Exception{return open();}
        default InstallerTransport openBoot(StockUpdateSession.Progress p)throws Exception{return open(p);}
    }
    private final File directory;
    private final BootstrapConnectSession bootstrapConnect;
    public InstallerController(File directory) { this(directory,new BootstrapConnectSession()); }
    InstallerController(File directory,BootstrapConnectSession connect) { this.directory=directory;this.bootstrapConnect=connect; }
    public void exportEvidence(OutputStream target)throws IOException {
        try(java.util.zip.ZipOutputStream zip=new java.util.zip.ZipOutputStream(target)){exportDirectory(directory,"",zip);}
    }
    private void exportDirectory(File dir,String prefix,java.util.zip.ZipOutputStream zip)throws IOException {
        File[] files=dir.listFiles();if(files==null)throw new IOException("Cannot enumerate installer evidence");
        Arrays.sort(files,(a,b)->a.getName().compareTo(b.getName()));
        byte[] buffer=new byte[32768];
        for(File file:files){String name=prefix+file.getName();if(file.isDirectory()){exportDirectory(file,name+"/",zip);continue;}
            zip.putNextEntry(new java.util.zip.ZipEntry(name));try(InputStream in=new FileInputStream(file)){int n;while((n=in.read(buffer))!=-1)zip.write(buffer,0,n);}zip.closeEntry();}
    }
    public String importBundle(InputStream input) throws Exception {
        byte[] data=RecoveryBundle.bounded(input,4*1024*1024);
        RecoveryBundle bundle=RecoveryBundle.read(new ByteArrayInputStream(data));
        if(!directory.isDirectory()&&!directory.mkdirs())throw new IOException("Installer directory unavailable");
        File target=new File(directory,bundle.sha256+".zip");
        if(!target.exists())try(FileOutputStream out=new FileOutputStream(target)){out.write(data);out.getFD().sync();}
        try(FileInputStream in=new FileInputStream(target)){if(!RecoveryBundle.read(in).sha256.equals(bundle.sha256))throw new IOException("Import readback mismatch");}
        return bundle.sha256;
    }
    public String run(String action,String address,String bundleHash,Connections connections,StockUpdateSession.Progress progress,
            BootstrapProvisioner.JpegCheck jpeg) throws Exception {
        try(io.opennoodoe.app.diagnostics.SessionLog log=new io.opennoodoe.app.diagnostics.SessionLog(new File(directory,"logs"))){
            try{
                log.append("action_begin",io.opennoodoe.app.diagnostics.SessionLog.fields("action",action,"device",address==null?"":address,"bundle",bundleHash==null?"":bundleHash));
                LoggedInstallerProgress observed=new LoggedInstallerProgress(progress,log);
                String result;
                try{result=runLogged(action,address,bundleHash,new Connections(){
                    public InstallerTransport open()throws Exception{return new LoggedInstallerTransport(connections.open(observed),log,observed);}
                    public InstallerTransport openBoot(StockUpdateSession.Progress p)throws Exception{return new LoggedInstallerTransport(connections.openBoot(observed),log,observed);}
                },observed,jpeg,log);}
                finally{observed.finishMetrics();}
                // Reset/restore acceptance is deliberately not terminal. Keep
                // this session protected until a later identity/boot check.
                if(Arrays.asList("stock-install-bootstrap","restore-stock","uninstall-stock","reset-cfw","recovery-confirm").contains(action))
                    log.append("action_waiting",io.opennoodoe.app.diagnostics.SessionLog.fields("action",action,"state","BOOT_RESULT_UNCONFIRMED"));
                else log.complete();return result;
            }catch(Exception failure){try{log.failed(failure);}catch(IOException logging){failure.addSuppressed(logging);}throw failure;}
        }
    }
    private String runLogged(String action,String address,String bundleHash,Connections connections,StockUpdateSession.Progress progress,
            BootstrapProvisioner.JpegCheck jpeg,io.opennoodoe.app.diagnostics.SessionLog log) throws Exception {
        if("inspect-device".equals(action)) return DeviceProbe.run(connections,progress);
        if(Arrays.asList("stage-cfw","commit-cfw","reset-cfw").contains(action))
            throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0454,"이번 앱은 설치와 순정 복구 전용이에요. 주행 앱 통신은 아직 연결하지 않았어요."));
        if("bootstrap-view".equals(action)) {
            try(InstallerTransport stream=connections.open()) {
                NdcpSession session=new NdcpSession(stream);
                io.opennoodoe.app.protocol.ndcp.MaintenanceStatus status=new io.opennoodoe.app.protocol.ndcp.MaintenanceStatus(session.request(0x5a,new byte[0]));
                progress.role("bootstrap");
                // Old Bootstrap keeps schema1. Only an explicit unsupported-command
                // rejection permits this read-only fallback; a timeout is not success.
                try {BootstrapProgress.View view=new BootstrapProgress.View(session.request(0x84,new byte[0]));
                    progress.deviceStatus(view);
                    return status.message()+"\n\n"+view.text()+"\nRX "+view.requests+" / TX "+view.replies+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0455," · 세션 ")+view.epoch;
                }catch(NdcpSession.DeviceRejected old){if(old.result!=4)throw old;return status.message();}
            }
        }
        if(action.equals("diagnostic-status")||action.equals("diagnostic-return")||action.equals("diagnostic-log")){
            try(InstallerTransport stream=connections.open()){
                NdcpSession session=new NdcpSession(stream);progress.role("diagnostic");
                if(action.equals("diagnostic-log"))return DiagnosticSession.logs(session,new File(directory,"diagnostic-logs"),progress).getName();
                return DiagnosticSession.view(session,action.equals("diagnostic-return"));
            }
        }
        if("device-log".equals(action)){
            log.reserveArtifact(0x40000);
            try(InstallerTransport stream=connections.open()){
                return DeviceLogDownload.run(new NdcpSession(stream),new File(new File(directory,"logs"),log.id()),progress).getName();
            }
        }
        // Identity must be available BEFORE preparing a target-bound bundle.
        // This branch sends only raw bootstrap + framed DeviceInfo READ; no
        // supplied ZIP, update phase, normal app autosync or target writes.
        if("stock-read-info".equals(action)) {
            try(InstallerTransport stream=connections.open()) {
                StockUpdateSession session=new StockUpdateSession(stream,progress);DeviceInfo info=session.identify();
                progress.reply();progress.role("stock");
                if(info.status!=0)throw new IOException("DeviceInfo rejected: "+info.status);
                File evidence=new File(directory,"stock-identity-"+System.currentTimeMillis());
                if(!evidence.mkdirs())throw new IOException("Cannot create identity evidence");
                byte[] reply=session.identityReply();
                try(FileOutputStream out=new FileOutputStream(new File(evidence,"reply.bin"))){out.write(reply);out.getFD().sync();}
                String json="{\"format\":\"NOODOE_STOCK_IDENTITY_1\",\"payload_file\":\"reply.bin\",\"payload_sha256\":\""+RecoveryBundle.sha(reply)+"\",\"source\":\"android-stock-device-info-read\"}\n";
                try(FileOutputStream out=new FileOutputStream(new File(evidence,"identity.json"))){out.write(json.getBytes(java.nio.charset.StandardCharsets.UTF_8));out.getFD().sync();}
                return info.toString()+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0456,"\n읽기 증거 저장 완료. 번들 생성 전 BL·HW·모델 호환성을 대조해야 합니다.");
            }
        }
        if(bundleHash==null||!bundleHash.matches("[0-9a-f]{64}"))throw new IOException("Import a recovery bundle first");
        RecoveryBundle bundle;
        try(FileInputStream in=new FileInputStream(new File(directory,bundleHash+".zip"))){bundle=RecoveryBundle.read(in);}
        InstallJournal journal=new InstallJournal(new File(directory,bundleHash+"-"+address.replace(":","")+".journal"),log);
        journal.bind(address,bundleHash);
        if("journal".equals(action))return journal.values.toString();
        bundle.requireInstallable();
        // Fresh stock verification is read-only even when another ZIP has an
        // unresolved attempt. It resolves only this selected journal; other
        // attempts still guard subsequent writes and are never silently cleared.
        if(!action.equals("journal")&&!action.equals("reconcile")&&!action.equals("stock-return-check"))DeviceAttempts.check(directory,address,bundleHash);
        if(action.equals("bootstrap-connect"))return bootstrapConnect.run(connections,bundle,journal,progress);
        try(InstallerTransport stream=connections.open()) {
            if("stock-return-check".equals(action))return StockReturnSession.verify(stream,bundle,journal,progress);
            if("stock-identify".equals(action)) {
                DeviceInfo info=new StockUpdateSession(stream,progress).identify();bundle.checkStock(info);progress.reply();progress.role("stock");
                if("IMPORTED".equals(journal.state()))journal.save("STOCK_IDENTIFIED");
                return info.toString()+"\n"+(journal.state().equals("STOCK_RETURN_CONFIRMED")?
                    io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0457,"순정 복귀를 확인했어요. 선택한 ZIP으로 설치 도구를 다시 보낼 수 있어요. 이전 기록과 복구 자료는 보관됩니다."):journal.state());
            }
            if("stock-install-bootstrap".equals(action)) {
                new StockUpdateSession(stream,progress).installBootstrap(bundle,journal,progress);
                // Release the old stock socket before the BL restart; the next
                // operation only connects/reads and cannot retransmit Bootstrap.
                stream.close();return bootstrapConnect.run(connections,bundle,journal,progress);
            }
            NdcpSession ndcp=new NdcpSession(stream);
            switch(action) {
                case "deep-backup": {
                    ndcp.identity(bundle,journal,1);progress.role("bootstrap");BootstrapInstallFlow.awaitLocalInstall(ndcp,progress);
                    byte[] status=ndcp.status();if(ByteCodec.u32le(status,4)!=0)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0458,"전송 중인 작업을 먼저 확인하세요. 진단 백업은 유휴 상태에서만 시작해요."));
                    File evidence=new File(directory,"full-nor-"+System.currentTimeMillis());
                    new BootstrapProvisioner(ndcp,journal,progress).backup(evidence);
                    return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0459,"NOR 독립 A/B와 SHA 대조 완료. 설치에는 사용하지 않았어요. 누도에서 O를 새로 3초 유지해 메뉴로 돌아가세요.");
                }
                case "ndcp-status": {
                    ndcp.identity(bundle,journal,1);progress.role("bootstrap");progress.reply();
                    byte[] reply=ndcp.status();
                    BootstrapInstallFlow.identified(journal);
                    return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0460,"설치 도구의 기기·펌웨어 확인이 끝났어요.\n다음: 화면 위쪽의 ‘3. 설치도구 → CFW 설치 계속’을 눌러 주세요.\n누도에서 Install CFW를 선택하면 파일 준비부터 설치 확인까지 이어집니다.\n기록: ")+journal.state();
                }
                case "rollback-ack": return new ProductBootSession(ndcp).acknowledge(bundle,journal);
                case "cfw-verify": {
                    return new ProductBootSession(ndcp).inspectCurrent(bundle,journal,progress);
                }
                case "guided-bootstrap-restore": case "guided-bootstrap": case "resume-install": case "provision-install": {
                    progress.stage("connect",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0461,"연결된 설치 도구와 선택한 ZIP이 일치하는지 확인하고 있어요."),0,0,"");
                    ndcp.identity(bundle,journal,1);progress.role("bootstrap");BootstrapInstallFlow.identified(journal);
                    boolean autonomous=TransferCapabilities.query(ndcp).localInstall;
                    int continuation=autonomous?BootstrapContinuation.inspect(ndcp.status(),bundle.paddedImage("cfw"),journal):0;
                    if(continuation==BootstrapContinuation.INSTALLING){
                        journal.save("WAIT_CFW_BOOT");stream.close();return reconnectAndConfirm(connections,bundle,journal,progress);
                    }
                    if(continuation==BootstrapContinuation.RECHECK)BootstrapContinuation.reverify(ndcp,journal,progress);
                    else {
                    boolean resume="resume-install".equals(action)||(action.startsWith("guided-bootstrap")&&BootstrapInstallFlow.resume(journal));
                    if(!resume){journal.require("BOOTSTRAP_IDENTIFIED");
                        journal.values.setProperty("reinstall.policy",action.equals("guided-bootstrap-restore")?"restore":"fresh");
                        journal.save("BOOTSTRAP_IDENTIFIED");}
                    if(!resume)journal.require("BOOTSTRAP_IDENTIFIED");
                    BootstrapInstallFlow.awaitLocalInstall(ndcp,progress);
                    BootstrapProvisioner provisioner=new BootstrapProvisioner(ndcp,journal,progress);
                    if(resume)provisioner.resume(bundle,new File(directory,"resume-"+System.currentTimeMillis()));
                    else provisioner.run(bundle,new File(directory,"evidence-"+System.currentTimeMillis()),jpeg);
                    ndcp.stage(bundle,journal,progress);
                    }
                    if(autonomous)journal.save("NDCP_LOCAL_CONFIRM_WAIT");
                    progress.stage("confirm",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0462,"파일 확인 완료. 누도에서 Back 대신 Install CFW를 선택하고 O를 눌러 주세요. 확인 대기 한도는 15분입니다."),0,0,"");
                    long deadline=System.nanoTime()+900_000_000_000L;
                    boolean accepted=false;
                    while(System.nanoTime()<deadline){
                        io.opennoodoe.app.protocol.ndcp.MaintenanceStatus view;
                        try{view=new io.opennoodoe.app.protocol.ndcp.MaintenanceStatus(ndcp.request(0x5a,new byte[0]));}
                        catch(IOException lost){
                            if(!autonomous)throw lost;
                            journal.save("NDCP_LOCAL_RESULT_UNKNOWN");stream.close();
                            return reconnectAndConfirm(connections,bundle,journal,progress);
                        }
                        if(autonomous?view.state==5&&view.phase==101:view.installAllowed()){accepted=true;break;}
                        if(view.state==6||view.state==8)throw new IOException(view.message());
                        if(view.phase!=100){journal.save("INSTALL_CONFIRM_CANCELLED");throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0463,"기기에서 설치를 취소했어요. 다시 하려면 설치 이어가기를 선택해 주세요."));}
                        Thread.sleep(250);
                    }
                    if(!accepted)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0464,"설치 확인 시간이 지났어요. 파일을 설치하지 않았어요."));
                    if(autonomous){
                        journal.save("WAIT_CFW_BOOT");
                        progress.stage("reboot",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0465,"기기에서 승인됐어요. 누도가 자체 검증·설치·재시작하고 있어요. Bluetooth가 끊겨도 작업은 계속됩니다."),0,0,"");
                    }else{
                        progress.stage("reboot",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0466,"O 승인을 확인했어요. 설치 기록을 확정하고 있어요."),0,0,"");ndcp.commit(journal);
                        progress.stage("reboot",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0467,"설치 기록 확정 완료. 재시작을 요청하고 있어요."),0,0,"");ndcp.reset(journal);
                    }
                    progress.role("unknown");stream.close();
                    return reconnectAndConfirm(connections,bundle,journal,progress);
                }
                case "update-cfw": {
                    boolean restarted=new RoutineUpdateSession(ndcp).update(bundle,journal,progress);
                    if(!restarted){new ProductBootSession(ndcp).confirm(bundle,journal,progress);return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0468,"업데이트 정상 확정 완료");}
                    stream.close();
                    return reconnectAndConfirm(connections,bundle,journal,progress);
                }
                case "diagnostic-cfw": {
                    new RoutineUpdateSession(ndcp).diagnostic(bundle,journal,progress);
                    stream.close();return DiagnosticSession.waitForBoot(connections,bundle,journal,progress);
                }
                case "uninstall-stock": {
                    new UninstallSession(ndcp).run(bundle,journal,progress);
                    progress.role("unknown");progress.stage("unconfirmed",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0469,"삭제 도구로 재시작했어요. 누도에서 UP/DOWN으로 Erase CFW data를 고르고 삭제 용량을 확인한 뒤 O를 새로 2초 눌러 주세요. Keep CFW data는 삭제 없이 복귀해요. 이후 기기에서 정리하며 Bluetooth 진행률은 제공하지 않아요."),0,0,"");
                    return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0470,"기기에서 정리 중 / 복귀 확인 필요. 순정 화면이 나온 뒤 순정 복귀 확인을 실행하세요. 재페어링이 필요할 수 있어요.");
                }
                case "restore-stock": {
                    journal.values.setProperty("return.mode","KEEP_CFW_DATA");journal.save(journal.state());
                    byte[] identity=ndcp.request(0x58,new byte[0]);
                    if(identity.length!=88)throw new IOException("Unknown identity ABI");
                    long role=ByteCodec.u32le(identity,8);
                    if(role==2)new RoutineUpdateSession(ndcp).prepareMaintenance(bundle,journal,progress);
                    else if(role==1)ndcp.identity(bundle,journal,1);
                    else throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0471,"이 역할에서는 순정 복구를 요청할 수 없어요."));
                    progress.role(role==1?"bootstrap":"product");progress.stage("reboot",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0472,"순정 복구 요청을 처리 중이에요. 실제 순정 부팅 확인 전에는 완료가 아니에요."),0,0,"");
                    ndcp.recover(1,journal);ndcp.recover(2,journal);Thread.sleep(2000);
                    long deadline=System.nanoTime()+120_000_000_000L;
                    while(System.nanoTime()<deadline){
                        byte[] r;
                        try {r=ndcp.recover(0,journal);}catch(IOException interrupted){journal.save("RECOVERY_RESULT_UNKNOWN_BOOT_UNVERIFIED");progress.stage("unconfirmed",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0473,"순정 복구 뒤 통신이 끊겼어요. 실제 순정 부팅을 확인해야 해요."),0,0,"");return io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0474,"통신 결과를 확인하지 못했습니다. 순정 복귀 부팅 확인을 실행하세요.\n")+interrupted.getMessage();}
                        if(ByteCodec.u32le(r,4)==5||ByteCodec.u32le(r,20)!=0)throw new IOException("Device recovery failed");
                        Thread.sleep(250);
                    }
                    throw new IOException("Recovery did not reset; result remains unconfirmed");
                }
                case "stage-cfw": ndcp.stage(bundle,journal,progress);break;
                case "commit-cfw": ndcp.commit(journal);break;
                case "reset-cfw": ndcp.reset(journal);break;
                case "reconcile":
                    if("ERASE_CFW_DATA".equals(journal.values.getProperty("return.mode")))new UninstallSession(ndcp).reconcile(bundle,journal);
                    else ndcp.reconcile(journal);
                    break;
                case "recovery-status": case "recovery-request": case "recovery-confirm": case "recovery-cancel": {
                    int index=Arrays.asList("recovery-status","recovery-request","recovery-confirm","recovery-cancel").indexOf(action);
                    byte[] r=ndcp.recover(index,journal);
                    return journal.state()+"\nRecovery state="+ByteCodec.u32le(r,4)+" sourceReady="+ByteCodec.u32le(r,8)
                            +" verified="+ByteCodec.u32le(r,12)+" / "+ByteCodec.u32le(r,16)+" error="+ByteCodec.u32le(r,20);
                }
                default:throw new IOException("Unknown installer action");
            }
        }
        return journal.state();
    }
    /** Reset is sent once by the caller. Reconnects only read identity/state
     * and acknowledge the exact trial; a terminal rollback is never retried. */
    private String reconnectAndConfirm(Connections connections,RecoveryBundle bundle,InstallJournal journal,StockUpdateSession.Progress progress)throws Exception {
        return new BootHandoffSession().run(connections,bundle,journal,progress);
    }
}
