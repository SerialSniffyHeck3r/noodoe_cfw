package io.opennoodoe.app.companion;
import android.content.Context;
import org.junit.*;import org.junit.runner.RunWith;
import org.robolectric.*;import org.robolectric.annotation.Config;
import org.robolectric.annotation.GraphicsMode;
import java.util.*;
import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28) @GraphicsMode(GraphicsMode.Mode.NATIVE)
public class PhoneCallsTest {
 @Before public void clear(){RuntimeEnvironment.getApplication().getSharedPreferences("call-favorites",0).edit().clear().commit();}
 @Test public void changedNumberInvalidatesSameNameImage(){
  NoodoeCallService.View a=new NoodoeCallService.View(123,1,0,"Unknown caller","55501");
  NoodoeCallService.View b=new NoodoeCallService.View(123,1,0,"Unknown caller","55502");
  assertNotEquals(CallIdentity.key(1,123,a),CallIdentity.key(1,123,b));
 }
 @Test public void favoritesAreOrderedBoundedAndRetainedOnInvalidSave(){
  Context c=RuntimeEnvironment.getApplication();CallFavorites f=new CallFavorites(c);List<CallFavorites.Entry> entries=new ArrayList<>();
  for(int i=0;i<10;i++)entries.add(new CallFavorites.Entry("Contact "+i,"555000"+i));
  assertTrue(f.save(entries));Collections.swap(entries,0,9);assertTrue(f.save(entries));
  assertEquals("Contact 9",new CallFavorites(c).get().get(0).name);
  entries.add(new CallFavorites.Entry("Overflow","555"));assertFalse(f.save(entries));assertEquals(10,f.get().size());
  entries.clear();entries.add(new CallFavorites.Entry("Invalid",""));assertFalse(f.save(entries));assertEquals(10,f.get().size());
 }
 @Test public void absentCapabilityDoesNotSendAndInvalidCallCannotExecute()throws Exception{
  Context c=RuntimeEnvironment.getApplication();CompanionWire wire=new CompanionWire(null);wire.epoch=42;wire.ign=true;
  PhoneCalls calls=new PhoneCalls(c,wire,()->1,"AA");assertNull(calls.tick(100));
  assertEquals(7,calls.command(CompanionWire.words(41,1,1,0,1)));
  assertEquals(1,calls.command(CompanionWire.words(42,1,1,0,1)));
  assertEquals(7,NoodoeCallService.act(0x80000001L,1));
 }
 @Test public void cjkCallPanelRemainsOneBoundedAlpha4Buffer(){
  CjkFonts.initialize(RuntimeEnvironment.getApplication());
  List<CallFavorites.Entry> entries=Arrays.asList(new CallFavorites.Entry("電話 日本語 臺灣 漢字","5550000"));
  byte[] panel=PhonePanelRenderer.calls(entries,1,0,new NoodoeCallService.View(0,0,0,"",""));assertEquals(288*128/2,panel.length);
  boolean ink=false;for(byte b:panel)ink|=b!=0;assertTrue(ink);
 }
}
