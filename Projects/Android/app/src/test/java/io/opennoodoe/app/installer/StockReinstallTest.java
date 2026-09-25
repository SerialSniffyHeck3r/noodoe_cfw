package io.opennoodoe.app.installer;

import io.opennoodoe.app.protocol.*;
import org.junit.Test;
import static org.junit.Assert.*;
import java.io.*;
import java.nio.file.Files;
import java.util.*;

/** Exercises the real stock sender and journal across a completed restore.
 * This is a framed-protocol simulation, not a Bluetooth/vehicle test. */
public final class StockReinstallTest {
    private static final String ADDRESS="00:00:00:00:00:00";
    private InstallJournal completed(File root,RecoveryBundle bundle)throws Exception {
        InstallJournal j=new InstallJournal(new File(root,bundle.sha256+"-000000000000.journal"));
        j.bind(ADDRESS,bundle.sha256);j.values.setProperty("uid","1,2,3");
        j.values.setProperty("resident.backup",new File(root,"original.bin").toString());
        j.values.setProperty("resident.sha256","abc");
        j.values.setProperty("stock.return.mac",ADDRESS);j.values.setProperty("transaction","77");
        j.values.setProperty("backup.mode","scoped-v2");j.values.setProperty("backup.directory","previous-evidence");
        j.values.setProperty("create.kind","photo");j.values.setProperty("offset","458752");
        j.values.setProperty("return.mode","KEEP_CFW_DATA");j.save("STOCK_RETURN_CONFIRMED");return j;
    }
    private static final class Radio implements InstallerTransport {
        byte[] pending;int sequence=128,total,identityReads;String fail="";
        final List<Integer> commands=new ArrayList<>();
        public void send(byte[] wire)throws IOException {
            if(wire.length==5&&wire[0]==5){
                if(fail.equals("not-stock")){pending=new byte[]{1,2,3};return;}
                pending=new byte[26];pending[0]=(byte)0x85;ByteCodec.putU32le(pending,1,21);
                pending[6]=5;pending[8]=16;pending[11]=1;return;
            }
            SequenceFrame frame=SequenceFrame.decode(wire);if(frame.getPayload().length==0)return;
            CommandFrame c=CommandFrame.decodeMany(frame.getPayload()).get(0);
            int id=c.getCommandId();commands.add(id);byte[] p=c.getPayload(),r;
            if(id==5){
                identityReads++;r=new byte[82];r[2]=5;r[4]=16;r[7]=1;r[14]=1;
                System.arraycopy("AK550".getBytes(),0,r,40,5);System.arraycopy("sr0601".getBytes(),0,r,75,6);
                if(fail.equals("peer"))r[8]=1;
                if(fail.equals("hardware"))r[7]=2;
                if(fail.equals("version"))r[4]=15;
            }else if(id==12){r=new byte[11];r[2]=(byte)(fail.equals("ign")?0:1);r[10]=(byte)(fail.equals("moving")?1:0);}
            else {
                r=new byte[id==13?16:6];r[2]=p[0];r[3]=p[1];r[4]=1;
                if(id==13){total+=p.length-6;ByteCodec.putU32le(r,12,total);}
                if(id==10&&fail.equals("begin-lost")){pending=null;return;}
            }
            pending=new SequenceFrame(0x40,sequence++,frame.getPacketIndex(),0,new CommandFrame(id,8,r).encode()).encode();
        }
        public byte[] receive(long timeout)throws IOException {if(pending==null||fail.equals("offline"))throw new IOException("lost reply");byte[] r=pending;pending=null;return r;}
        public void close(){}
        boolean wrote(){return commands.stream().anyMatch(c->c==10||c==11||c==13);}
    }
    @Test public void completedReturnStartsNewAttemptAndPreservesEvidence()throws Exception {
        RecoveryBundle b=new InstallerTest().valid();File root=Files.createTempDirectory("stock-reinstall").toFile();
        InstallJournal j=completed(root,b);Properties before=new Properties();before.putAll(j.values);
        File original=new File(j.values.getProperty("resident.backup"));Files.write(original.toPath(),new byte[]{9,8,7});
        Radio radio=new Radio();new StockUpdateSession(radio).installBootstrap(b,j,text->{});
        assertEquals("STOCK_ACCEPTED_WAIT_IGN_OFF",j.state());assertEquals(1,radio.identityReads);assertEquals(0x70000,radio.total);
        InstallJournal archive=new InstallJournal(new File(j.values.getProperty("previous.attempt")));
        assertEquals(before,archive.values);assertEquals("1,2,3",j.values.getProperty("uid"));
        assertEquals(original.toString(),j.values.getProperty("resident.backup"));assertArrayEquals(new byte[]{9,8,7},Files.readAllBytes(original.toPath()));
        for(String key:new String[]{"transaction","backup.directory","backup.mode","create.kind","return.mode"})assertFalse(key,j.values.containsKey(key));
        assertEquals(1,new File(root,"attempt-history").listFiles().length);
        // A double tap after transfer is not a second automatic transfer.
        Radio duplicate=new Radio();try{new StockUpdateSession(duplicate).installBootstrap(b,j,text->{});fail();}catch(InstallJournal.StateBlocked expected){}
        assertFalse(duplicate.wrote());assertEquals(0,duplicate.identityReads);
    }
    @Test public void freshIdentityAndStationaryChecksPrecedeArchiveAndWrites()throws Exception {
        for(String reason:new String[]{"peer","hardware","moving","ign"}){
            RecoveryBundle b=new InstallerTest().valid();File root=Files.createTempDirectory("reinstall-reject").toFile();InstallJournal j=completed(root,b);
            Radio radio=new Radio();radio.fail=reason;
            try{new StockUpdateSession(radio).installBootstrap(b,j,text->{});fail(reason);}catch(IOException expected){}
            assertFalse(reason,radio.wrote());assertEquals("STOCK_RETURN_CONFIRMED",j.state());
            assertFalse(new File(root,"attempt-history").exists());assertEquals("77",j.values.getProperty("transaction"));
        }
    }
    @Test public void unknownMutationCannotUseReinstallTransition()throws Exception {
        for(String state:new String[]{"STOCK_BEGIN_RESULT_UNKNOWN","STOCK_DONE_RESULT_UNKNOWN","NDCP_LOCAL_RESULT_UNKNOWN","PRODUCT_COMMIT_RESULT_UNKNOWN","UNINSTALL_RESET_RESULT_UNKNOWN"}){
            RecoveryBundle b=new InstallerTest().valid();File root=Files.createTempDirectory("reinstall-unknown").toFile();InstallJournal j=completed(root,b);j.save(state);
            Radio radio=new Radio();try{new StockUpdateSession(radio).installBootstrap(b,j,text->{});fail(state);}catch(InstallJournal.StateBlocked expected){}
            assertFalse(radio.wrote());assertEquals(0,radio.identityReads);assertEquals(state,j.state());assertFalse(new File(root,"attempt-history").exists());
        }
    }
    @Test public void archiveFailureStopsBeforeFirstRemoteWrite()throws Exception {
        RecoveryBundle b=new InstallerTest().valid();File root=Files.createTempDirectory("reinstall-no-archive").toFile();InstallJournal j=completed(root,b);
        Files.write(new File(root,"attempt-history").toPath(),new byte[]{1});Radio radio=new Radio();
        try{new StockUpdateSession(radio).installBootstrap(b,j,text->{});fail();}catch(IOException expected){}
        assertFalse(radio.wrote());assertEquals("STOCK_RETURN_CONFIRMED",j.state());
    }
    @Test public void crashAfterArchiveIsResumableButLostBeginIsNotReplayed()throws Exception {
        RecoveryBundle b=new InstallerTest().valid();File root=Files.createTempDirectory("reinstall-restart").toFile();InstallJournal j=completed(root,b);
        j.restartAfterStockReturn();File file=new File(root,b.sha256+"-000000000000.journal");
        j=new InstallJournal(file);assertEquals("STOCK_IDENTIFIED",j.state());Radio first=new Radio();first.fail="begin-lost";
        try{new StockUpdateSession(first).installBootstrap(b,j,text->{});fail();}catch(IOException expected){}
        j=new InstallJournal(file);assertEquals("STOCK_BEGIN_RESULT_UNKNOWN",j.state());
        Radio retry=new Radio();try{new StockUpdateSession(retry).installBootstrap(b,j,text->{});fail();}catch(InstallJournal.StateBlocked expected){}
        assertEquals(0,retry.identityReads);assertFalse(retry.wrote());assertEquals(1,new File(root,"attempt-history").listFiles().length);
    }
    @Test public void confirmedStockCheckCanBeRepeatedBeforeReinstall()throws Exception {
        byte[] archive=new InstallerTest().validArchive();RecoveryBundle b=RecoveryBundle.read(new ByteArrayInputStream(archive));
        File root=Files.createTempDirectory("reinstall-controller").toFile();completed(root,b);
        InstallerController controller=new InstallerController(root,new BootstrapConnectSession(new BootstrapConnectSessionTest.Clock()));
        assertEquals(b.sha256,controller.importBundle(new ByteArrayInputStream(archive)));
        for(int i=0;i<2;i++){
            Radio check=new Radio();String result=controller.run("stock-return-check",ADDRESS,b.sha256,()->check,text->{},data->{});
            assertTrue(result.contains("순정 복귀를 확인"));assertFalse(check.wrote());
        }
        Radio install=new Radio();BootstrapConnectSessionTest.Radio bootstrap=new BootstrapConnectSessionTest.Radio(b);
        controller.run("stock-install-bootstrap",ADDRESS,b.sha256,new InstallerController.Connections(){
            public InstallerTransport open(){return install;}
            public InstallerTransport openBoot(StockUpdateSession.Progress p){assertEquals(0x70000,install.total);return bootstrap;}
        },text->{},data->{});
        assertEquals(0x70000,install.total);
        assertTrue(bootstrap.closed);assertEquals("BOOTSTRAP_IDENTIFIED",new InstallJournal(new File(root,b.sha256+"-000000000000.journal")).state());
    }
    @Test public void blockedStateDoesNotClaimStorageFailure() {
        String message=InstallerFailure.explain(new InstallJournal.StateBlocked("STOCK_RETURN_CONFIRMED"),"stock");
        assertTrue(message.contains("순정 복귀는 확인"));assertFalse(message.contains("저장 공간"));assertFalse(message.contains("성공·취소 여부"));
    }
    @Test public void buttonOnlyReturnResolvesBootWaitWithFreshStockEvidence()throws Exception {
        for(String state:new String[]{"WAIT_CFW_BOOT","TRIAL_CONFIRM_RESULT_UNKNOWN","TRIAL_HEALTH_WAIT","NDCP_LOCAL_RESULT_UNKNOWN","NDCP_RESET_RESULT_UNKNOWN","CFW_CONFIRMED","WAIT_DIAGNOSTIC_BOOT"}){
            byte[] zip=new InstallerTest().validArchive();RecoveryBundle b=RecoveryBundle.read(new ByteArrayInputStream(zip));
            File root=Files.createTempDirectory("manual-stock-return").toFile();InstallJournal j=completed(root,b);j.save(state);
            Properties before=new Properties();before.putAll(j.values);
            InstallerController c=new InstallerController(root);c.importBundle(new ByteArrayInputStream(zip));Radio radio=new Radio();
            String text=c.run("stock-return-check",ADDRESS,b.sha256,()->radio,s->{},data->{});
            j=new InstallJournal(new File(root,b.sha256+"-000000000000.journal"));
            assertEquals("STOCK_RETURN_CONFIRMED",j.state());assertEquals(state,j.values.getProperty("stock.return.previous_state"));
            Properties preserved=new InstallJournal(new File(j.values.getProperty("stock.return.previous_record"))).values;
            for(String key:before.stringPropertyNames())if(!key.equals("updated_unix_ms")&&!key.equals("journal.sha256"))
                assertEquals(key,before.getProperty(key),preserved.getProperty(key));
            assertEquals(j.values.getProperty("stock.return.previous_sha256"),preserved.getProperty("journal.sha256"));
            byte[] info=Files.readAllBytes(new File(j.values.getProperty("stock.return.identity")).toPath());
            assertEquals(82,info.length);assertEquals(RecoveryBundle.sha(info),j.values.getProperty("stock.return.reply.sha256"));
            assertEquals("1,2,3",j.values.getProperty("uid"));assertEquals("77",j.values.getProperty("transaction"));
            assertEquals("previous-evidence",j.values.getProperty("backup.directory"));
            assertTrue(text.contains("CFW 부팅 대기를 종료"));assertFalse(radio.wrote());assertEquals(Collections.singletonList(5),radio.commands);
            int files=new File(root,"stock-return-history").list().length;
            c.run("stock-return-check",ADDRESS,b.sha256,Radio::new,s->{},data->{});
            assertEquals(files,new File(root,"stock-return-history").list().length);
        }
    }
    @Test public void failedStockProofKeepsBootWaitAndCannotClaimRecovery()throws Exception {
        for(String failure:new String[]{"offline","not-stock","peer","hardware","version"}){
            byte[] zip=new InstallerTest().validArchive();RecoveryBundle b=RecoveryBundle.read(new ByteArrayInputStream(zip));
            File root=Files.createTempDirectory("unproven-stock").toFile();InstallJournal j=completed(root,b);j.save("WAIT_CFW_BOOT");
            InstallerController c=new InstallerController(root);c.importBundle(new ByteArrayInputStream(zip));Radio radio=new Radio();radio.fail=failure;
            String[] role={"unknown"};StockUpdateSession.Progress p=new StockUpdateSession.Progress(){public void update(String s){}public void role(String r){role[0]=r;}};
            try{c.run("stock-return-check",ADDRESS,b.sha256,()->radio,p,data->{});fail(failure);}catch(IOException expected){}
            j=new InstallJournal(new File(root,b.sha256+"-000000000000.journal"));
            assertEquals("WAIT_CFW_BOOT",j.state());assertFalse(radio.wrote());assertEquals("unknown",role[0]);
            assertFalse(new File(root,"stock-return-history").exists());
        }
    }
    @Test public void stockBootDoesNotResolveUnknownFatOrPendingStockTransfer()throws Exception {
        for(String state:new String[]{"CREATE_COMMIT_RESULT_UNKNOWN","RESOURCE_COMMIT_RESULT_UNKNOWN","STOCK_DONE_RESULT_UNKNOWN","STOCK_DATA_RESULT_UNKNOWN"}){
            RecoveryBundle b=new InstallerTest().valid();File root=Files.createTempDirectory("unrelated-unknown").toFile();InstallJournal j=completed(root,b);j.save(state);Radio radio=new Radio();
            try{StockReturnSession.verify(radio,b,j,s->{});fail(state);}catch(InstallJournal.StateBlocked expected){}
            assertEquals(state,j.state());assertFalse(radio.wrote());assertFalse(new File(root,"stock-return-history").exists());
        }
    }
    @Test public void manualReturnThenExplicitReinstallPreservesFailedBootRecord()throws Exception {
        RecoveryBundle b=new InstallerTest().valid();File root=Files.createTempDirectory("manual-return-reinstall").toFile();InstallJournal j=completed(root,b);j.save("WAIT_CFW_BOOT");
        Radio proof=new Radio();StockReturnSession.verify(proof,b,j,s->{});assertFalse(proof.wrote());
        File preserved=new File(j.values.getProperty("stock.return.previous_record"));byte[] before=Files.readAllBytes(preserved.toPath());
        Radio next=new Radio();new StockUpdateSession(next).installBootstrap(b,j,s->{});
        assertEquals("STOCK_ACCEPTED_WAIT_IGN_OFF",j.state());assertEquals(0x70000,next.total);
        assertArrayEquals(before,Files.readAllBytes(preserved.toPath()));assertEquals("WAIT_CFW_BOOT",new InstallJournal(preserved).state());
    }
    @Test public void readOnlyReturnCheckDoesNotClearOtherBundlesOrAuthorizeTheirWrites()throws Exception {
        byte[] zip=new InstallerTest().validArchive();RecoveryBundle b=RecoveryBundle.read(new ByteArrayInputStream(zip));
        File root=Files.createTempDirectory("other-pending").toFile();InstallJournal j=completed(root,b);j.save("WAIT_CFW_BOOT");
        String other=String.join("",Collections.nCopies(64,"a"));InstallJournal unrelated=new InstallJournal(new File(root,other+"-000000000000.journal"));
        unrelated.bind(ADDRESS,other);unrelated.save("WAIT_CFW_BOOT");
        InstallerController c=new InstallerController(root);c.importBundle(new ByteArrayInputStream(zip));
        c.run("stock-return-check",ADDRESS,b.sha256,Radio::new,s->{},data->{});
        assertEquals("WAIT_CFW_BOOT",new InstallJournal(new File(root,other+"-000000000000.journal")).state());
        int[] opens={0};try{c.run("stock-install-bootstrap",ADDRESS,b.sha256,()->{opens[0]++;return new Radio();},s->{},data->{});fail();}catch(IOException expected){}
        assertEquals(0,opens[0]);
    }
    @Test public void stockReturnEvidenceFailureCannotClearBootWait()throws Exception {
        RecoveryBundle b=new InstallerTest().valid();File root=Files.createTempDirectory("return-disk-error").toFile();InstallJournal j=completed(root,b);j.save("WAIT_CFW_BOOT");
        Files.write(new File(root,"stock-return-history").toPath(),new byte[]{1});Radio radio=new Radio();
        try{StockReturnSession.verify(radio,b,j,s->{});fail();}catch(IOException expected){}
        assertEquals("WAIT_CFW_BOOT",j.state());assertFalse(radio.wrote());
    }
    @Test public void bootWaitErrorPointsToPhysicalStockVerification(){
        String text=InstallerFailure.explain(new InstallJournal.StateBlocked("WAIT_CFW_BOOT"),"product");
        assertTrue(text.contains("순정으로 돌아왔어요"));assertFalse(text.contains("저장 공간"));
    }
}
