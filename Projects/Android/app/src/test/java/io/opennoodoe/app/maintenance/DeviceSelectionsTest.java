package io.opennoodoe.app.maintenance;
import org.junit.Test;import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.Config;
import static org.junit.Assert.*;import io.opennoodoe.app.UiThemeSettings;import android.content.Context;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28)
public class DeviceSelectionsTest {
 @Test public void resetOnlyCurrentChoiceAndPreservesThemeAndOtherDevice(){
  Context c=RuntimeEnvironment.getApplication();DeviceSelections s=new DeviceSelections(c);
  s.select("AA:01");s.bundle("AA:01","zip-a");s.display("AA:01","old evidence");s.bundle("AA:02","zip-b");
  UiThemeSettings.setMode(c,UiThemeSettings.DARK);s.reset();
  assertNull(s.address());assertNull(s.bundle("AA:01"));assertEquals("zip-b",s.bundle("AA:02"));
  assertEquals("old evidence",s.display("AA:01"));assertEquals(UiThemeSettings.DARK,UiThemeSettings.mode(c));
  s.reset();assertEquals("zip-b",s.bundle("AA:02"));
  s.select("AA:02");assertEquals("zip-b",new DeviceSelections(c).bundle("AA:02"));
 }
 @Test public void themeDefaultsToSystemAndExplicitSelectionSurvivesRecreate(){
  Context c=RuntimeEnvironment.getApplication();UiThemeSettings.setMode(c,UiThemeSettings.AUTO);
  org.robolectric.RuntimeEnvironment.setQualifiers("night");assertTrue(UiThemeSettings.isDark(c));
  UiThemeSettings.setMode(c,UiThemeSettings.LIGHT);assertFalse(UiThemeSettings.isDark(c));
  UiThemeSettings.setMode(c,UiThemeSettings.DARK);org.robolectric.RuntimeEnvironment.setQualifiers("notnight");assertTrue(UiThemeSettings.isDark(c));
  UiThemeSettings.setMode(c,UiThemeSettings.AUTO);assertFalse(UiThemeSettings.isDark(c));
 }
}
