package io.opennoodoe.app.companion;
import android.app.Activity;
import android.companion.*;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.telecom.TelecomManager;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.annotation.*;
import org.robolectric.shadows.ShadowToast;
import java.util.*;
import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
@Config(sdk=34,shadows={CompanionAssociationTest.Cdm.class,CompanionAssociationTest.Telecom.class})
public class CompanionAssociationTest {
 @Implements(CompanionDeviceManager.class) public static class Cdm {
  static boolean fail;static int requested;
  @Implementation protected List<String> getAssociations(){return fail?Collections.emptyList():Arrays.asList("02:00:00:85:26:96");}
  @Implementation protected void associate(AssociationRequest r,CompanionDeviceManager.Callback c,Handler h){requested++;throw new SecurityException("OEM rejection");}
 }
 @Implements(TelecomManager.class) public static class Telecom {
  static boolean allowed;
  @Implementation protected boolean hasManageOngoingCallsPermission(){return allowed;}
 }
 @Before public void setup(){Cdm.fail=false;Cdm.requested=0;Telecom.allowed=false;Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager()).setSystemFeature(PackageManager.FEATURE_COMPANION_DEVICE_SETUP,true);}
 @Test public void actualAppOpRatherThanRuntimePermissionDeterminesControl(){
  android.content.Context c=RuntimeEnvironment.getApplication();assertFalse(CallPermissions.canControl(c,"02:00:00:85:26:96"));
  Telecom.allowed=true;assertTrue(CallPermissions.canControl(c,"02:00:00:85:26:96"));assertFalse(CallPermissions.canControl(c,"00:00:00:00:00:00"));
 }
 @Test public void rejectedRegistrationDoesNotKillActivity(){
  Cdm.fail=true;Shadows.shadowOf(RuntimeEnvironment.getApplication()).grantPermissions(android.Manifest.permission.BLUETOOTH_SCAN);
  try(org.robolectric.android.controller.ActivityController<Activity> a=Robolectric.buildActivity(Activity.class).setup()){
   CompanionAssociation.request(a.get(),"02:00:00:85:26:96");assertFalse(a.get().isFinishing());assertNotNull(ShadowToast.getTextOfLatestToast());
   assertEquals(1,Cdm.requested);
  }
 }
 @Test public void manifestDeclaresCompanionFeature()throws Exception{
  android.content.Context c=RuntimeEnvironment.getApplication();android.content.pm.PackageInfo p=c.getPackageManager().getPackageInfo(c.getPackageName(),PackageManager.GET_CONFIGURATIONS);
  boolean found=false;for(android.content.pm.FeatureInfo f:p.reqFeatures)found|=PackageManager.FEATURE_COMPANION_DEVICE_SETUP.equals(f.name);assertTrue(found);
 }
}
