package io.opennoodoe.app.installer;

import io.opennoodoe.app.protocol.DeviceInfo;
import io.opennoodoe.app.diagnostics.DurableFiles;
import java.io.*;
import java.util.Arrays;

/** A physical Gate restore can happen with no phone connected. Resolve the
 * boot wait only from a fresh stock protocol exchange on the selected peer.
 * This records the running APP, not a NOR cleanup, cancelled FAT transaction,
 * successful CFW boot, or permission to repeat an uncertain stock transfer. */
final class StockReturnSession {
    static boolean accepts(String state) {
        return Arrays.asList("WAIT_CFW_BOOT","TRIAL_CONFIRM_RESULT_UNKNOWN","TRIAL_HEALTH_WAIT",
            "NDCP_LOCAL_CONFIRM_WAIT","NDCP_LOCAL_RESULT_UNKNOWN","NDCP_COMMIT_RESULT_UNKNOWN",
            "NDCP_COMMITTED","NDCP_RESET_RESULT_UNKNOWN","WAIT_DIAGNOSTIC_BOOT","DIAGNOSTIC_RUNNING",
            "CFW_CONFIRMED","CURRENT_CFW_CONFIRMED","CFW_ROLLED_BACK","ROLLBACK_ACKNOWLEDGED",
            "UNINSTALL_DEVICE_RUNNING","UNINSTALL_RESET_RESULT_UNKNOWN","UNINSTALL_COMMITTED",
            "UNINSTALL_COMMIT_RESULT_UNKNOWN","RECOVERY_RUNNING","RECOVERY_COMMIT_RESULT_UNKNOWN",
            "RECOVERY_REQUEST_RESULT_UNKNOWN","RECOVERY_RESULT_UNKNOWN_BOOT_UNVERIFIED",
            "STOCK_RETURN_CONFIRMED").contains(state);
    }
    static String verify(InstallerTransport stream,RecoveryBundle bundle,InstallJournal journal,
                         StockUpdateSession.Progress progress)throws Exception {
        StockUpdateSession stock=new StockUpdateSession(stream,progress);
        DeviceInfo info=stock.identify();
        bundle.checkStock(info);
        if(!info.mac.equalsIgnoreCase(journal.values.getProperty("address","")))
            throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0610,"순정 Bluetooth 주소가 이전 기기와 달라요. 대기 기록은 변경하지 않았어요."));
        if(!accepts(journal.state()))throw new InstallJournal.StateBlocked(journal.state());
        String before=journal.state();byte[] reply=stock.identityReply();
        if(!before.equals("STOCK_RETURN_CONFIRMED")){
            File archived=journal.preserveSnapshot("stock-return-history","State before physically verified stock return");
            File evidence=new File(archived+".stock-info.bin");
            try(FileOutputStream out=new FileOutputStream(evidence)){out.write(reply);out.getFD().sync();}
            if(!Arrays.equals(reply,ScopedBackup.readFile(evidence,reply.length)))
                throw new IOException("Stock identity evidence readback mismatch");
            DurableFiles.directory(evidence.getParentFile());
            journal.values.setProperty("stock.return.previous_state",before);
            journal.values.setProperty("stock.return.previous_record",archived.getCanonicalPath());
            journal.values.setProperty("stock.return.previous_sha256",journal.values.getProperty("journal.sha256",""));
            journal.values.setProperty("stock.return.identity",evidence.getCanonicalPath());
        }
        journal.values.setProperty("stock.return.reply.sha256",RecoveryBundle.sha(reply));
        journal.values.setProperty("stock.return.mac",info.mac);
        journal.values.setProperty("stock.return.vehicle",info.model+" / "+info.pcba);
        if("ERASE_CFW_DATA".equals(journal.values.getProperty("return.mode")))
            journal.values.setProperty("cleanup.result","local-choice-not-observable-over-stock-bt");
        journal.save("STOCK_RETURN_CONFIRMED");
        progress.reply();progress.role("stock");
        progress.stage("stock-return",io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0611,"순정 복귀를 확인했어요. CFW 부팅 대기를 종료했어요."),0,0,"");
        return info+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0612,"\n순정 복귀를 확인했어요. CFW 부팅 대기를 종료했어요. 이전 기록과 복구 자료는 보관됩니다. 선택한 ZIP으로 새 설치를 시작할 수 있어요.");
    }
}
