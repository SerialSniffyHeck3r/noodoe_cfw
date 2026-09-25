package io.opennoodoe.app.companion;

import android.content.Context;
import io.opennoodoe.app.diagnostics.SessionLog;
import io.opennoodoe.app.protocol.ndcp.NdcpClient;
import java.io.*;
import java.nio.file.Files;
import java.util.*;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.annotation.Config;
import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class) @Config(sdk=28)
public class CompanionDiagnosticsTest {
 @Test public void actualRuntimeSnapshotSurvivesFirstConnectionAndPeriodicLogs()throws Exception {
  Context context=RuntimeEnvironment.getApplication();
  CompanionWire wire=new CompanionWire(new NdcpClient(new CompanionTest.Radio()));wire.connect();
  File directory=Files.createTempDirectory("companion-diagnostics").toFile();
  try(CompanionRuntime runtime=new CompanionRuntime(context,wire);SessionLog log=new SessionLog(directory)){
   Map<String,String> snapshot=runtime.gpsDiagnostics();
   assertTrue(snapshot.containsKey("callbacks"));assertTrue(snapshot.containsKey("art_requests"));
   for(int i=0;i<12;i++)log.append("phone_location_status",snapshot);
   log.complete();
  }
 }
 @Test public void personalContentRemainsRejected()throws Exception {
  try(SessionLog log=new SessionLog(Files.createTempDirectory("private-log-rejected").toFile())){
   for(String key:new String[]{"latitude","longitude","title","artist","phone_number","link_key"}){
    try{log.append("phone_location_status",SessionLog.fields(key,"secret"));fail(key);}
    catch(IllegalArgumentException expected){}
   }
   log.complete();
  }
 }
}
