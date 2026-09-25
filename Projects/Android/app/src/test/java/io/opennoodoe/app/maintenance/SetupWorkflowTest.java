package io.opennoodoe.app.maintenance;
import org.junit.*;import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.Config;
import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28)
public class SetupWorkflowTest {
 @Test public void bootstrapAppearanceDoesNotLaunchCompanionProtocol(){SetupWorkflow w=fresh();w.select("AA");w.begin("stock-install-bootstrap");w.role("bootstrap");w.finish("stock-install-bootstrap",false,"identified");assertFalse(w.automaticDrivingAllowed());w=fresh();w.select("AA");assertFalse(w.automaticDrivingAllowed());w.begin("guided-bootstrap");w.role("product");w.finish("guided-bootstrap",false,"confirmed");assertTrue(w.automaticDrivingAllowed());}
 @Test public void automaticBootstrapConnectionKeepsItsRole(){SetupWorkflow w=fresh();w.select("AA");w.begin("stock-install-bootstrap");w.role("bootstrap");w.finish("stock-install-bootstrap",false,"identified");assertEquals("bootstrap",w.view().role);assertFalse(w.view().recovery);w=fresh();w.select("AA");assertFalse(w.view().recovery);}
 @Test public void explicitBootstrapConnectionResolvesConnectionError(){SetupWorkflow w=fresh();w.select("AA");w.begin("stock-install-bootstrap");w.finish("stock-install-bootstrap",true,"no bond");w.begin("bootstrap-connect");w.role("bootstrap");w.finish("bootstrap-connect",false,"identified");assertEquals("bootstrap",w.view().role);assertFalse(w.view().recovery);}
 @Test public void diagnosticDoesNotBecomeProductOrConfirmStock(){SetupWorkflow w=fresh();w.select("AA");w.role("product");w.begin("diagnostic-cfw");w.finish("diagnostic-cfw",false,"running");assertEquals("diagnostic",w.view().role);assertFalse(w.view().recovery);
  w.begin("diagnostic-return");w.finish("diagnostic-return",false,"confirmation open");assertEquals("diagnostic",w.view().role);assertTrue(w.view().recovery);
  w.begin("diagnostic-status");w.finish("diagnostic-status",false,"ready");assertTrue(w.view().recovery);
  w.begin("stock-return-check");w.finish("stock-return-check",false,"verified");assertEquals("stock",w.view().role);assertFalse(w.view().recovery);}
 private SetupWorkflow fresh(){return new SetupWorkflow(RuntimeEnvironment.getApplication());}
 @Test public void confirmedReturnRecoversLegacyBlockedReinstallWithoutAppReset(){
  SetupWorkflow w=fresh();w.select("AA");w.role("stock");w.begin("stock-return-check");w.finish("stock-return-check",false,"confirmed");
  w.begin("stock-install-bootstrap");w.finish("stock-install-bootstrap",true,"Action blocked in journal state STOCK_RETURN_CONFIRMED");
  w=fresh();w.select("AA");assertTrue(w.view().recovery);
  w.begin("stock-return-check");w.role("stock");w.finish("stock-return-check",false,"confirmed again");
  assertFalse(w.view().recovery);assertEquals("stock",w.view().role);
  w.begin("stock-install-bootstrap");w.finish("stock-install-bootstrap",false,"transferred");
  assertFalse(w.view().recovery);assertEquals("unknown",w.view().role);
 }
 @Test public void unknownCommitSurvivesDeathAndInspection(){SetupWorkflow w=fresh();w.select("AA");w.role("bootstrap");w.begin("guided-bootstrap");w.finish("guided-bootstrap",true,"lost");
  w=fresh();w.select("AA");assertTrue(w.view().recovery);assertEquals("unknown",w.view().role);
  w.begin("inspect-device");w.role("product");w.finish("inspect-device",false,"identified");assertTrue(w.view().recovery);
  w.begin("cfw-verify");w.finish("cfw-verify",false,"confirmed");assertFalse(w.view().recovery);
  w=fresh();w.select("AA");assertFalse(w.view().recovery);
 }
 @Test public void switchingDevicesAndLocalResetDoNotReuseOrEraseAttempts(){SetupWorkflow w=fresh();w.select("AA");w.begin("update-cfw");w.finish("update-cfw",true,"lost");w.select("BB");assertFalse(w.view().recovery);assertEquals("unknown",w.view().role);
  w.select("AA");assertTrue(w.view().recovery);w.reset();assertNull(w.view().address);w.select("AA");assertTrue(w.view().recovery);
 }
 @Test public void stockTransferIsNotCFWSuccess(){SetupWorkflow w=fresh();w.select("AA");w.role("stock");w.begin("stock-install-bootstrap");w.finish("stock-install-bootstrap",false,"sent");assertEquals("unknown",w.view().role);assertFalse(w.view().recovery);assertEquals("stock-install-bootstrap",w.view().operation);
  w=fresh();w.select("AA");assertTrue(w.view().recovery);
 }
 @Test public void ordinaryConnectionFailureCanBeRecheckedWithoutAZip(){SetupWorkflow w=fresh();w.select("AA");w.begin("inspect-device");w.finish("inspect-device",true,"offline");assertTrue(w.view().recovery);w.begin("inspect-device");w.role("product");w.finish("inspect-device",false,"ok");assertFalse(w.view().recovery);}
 @Test public void importsAndDiagnosticExportsDoNotClearInstallationFailure(){SetupWorkflow w=fresh();w.select("AA");w.begin("update-cfw");w.finish("update-cfw",true,"lost");for(String action:new String[]{"import","diagnostics","export","reconcile"}){w.begin(action);w.finish(action,false,"done");assertTrue(w.view().recovery);}}
 @Test public void upgradeFindsPriorDurableUnknownCommit()throws Exception{
  java.io.File root=new java.io.File(RuntimeEnvironment.getApplication().getFilesDir(),"installer");root.mkdirs();
  io.opennoodoe.app.installer.InstallJournal journal=new io.opennoodoe.app.installer.InstallJournal(new java.io.File(root,"bundle-AA.journal"));journal.bind("AA","bundle");journal.save("NDCP_COMMIT_RESULT_UNKNOWN");
  SetupWorkflow w=fresh();w.select("AA");assertTrue(w.view().recovery);assertTrue(w.view().operation.contains("COMMIT_RESULT_UNKNOWN"));
  w.select("BB");assertFalse(w.view().recovery);
 }
}
