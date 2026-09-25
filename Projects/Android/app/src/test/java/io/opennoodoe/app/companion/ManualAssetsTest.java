package io.opennoodoe.app.companion;
import android.content.*;import android.app.*;import android.service.notification.*;import org.junit.*;import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.*;import java.io.*;import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28) @GraphicsMode(GraphicsMode.Mode.NATIVE)
public class ManualAssetsTest {
 static void save(String n,byte[] b)throws Exception{File d=new File("build/manual-assets");assertTrue(d.isDirectory()||d.mkdirs());try(FileOutputStream f=new FileOutputStream(new File(d,n))){f.write(b);}}
 @Test public void samplesUseProductionPhoneRenderers()throws Exception{
 Context c=RuntimeEnvironment.getApplication();CjkFonts.initialize(c);
 save("calls-list.a4",PhonePanelRenderer.calls(java.util.Arrays.asList(new CallFavorites.Entry("민수","010-0000-0001"),new CallFavorites.Entry("지연","010-0000-0002"),new CallFavorites.Entry("지훈","010-0000-0003")),3,1,new NoodoeCallService.View(0,0,0,"","")));
 save("calls-incoming.a4",PhonePanelRenderer.calls(java.util.Collections.emptyList(),0,0,new NoodoeCallService.View(1,1,0,"지연","010-0000-0002")));
 save("music-rick.a4",PhoneVisualRenderer.music("Never Gonna Give You Up","Rick Astley"));
 save("music-idol-tile.bin",new MusicTextTiles(7,"君のいない世界に","まねきケチャ").tile(1,0));
 save("music-rick-tile.bin",new MusicTextTiles(30,"Never Gonna Give You Up","Rick Astley").tile(1,0));
 save("music.a4",PhoneVisualRenderer.music("君のいない世界に","まねきケチャ"));
 save("phone-idle.a4",PhonePanelRenderer.notification(c,"Galaxy S24 Ultra",87,null,true));
 c.getSharedPreferences("companion",0).edit().putStringSet("notification.apps",java.util.Collections.singleton(c.getPackageName())).commit();
 NotificationHistory h=new NotificationHistory();h.activate(c);Notification n=new Notification.Builder(c,"manual").setSmallIcon(android.R.drawable.ic_dialog_email).setContentTitle("잠깐 쉬었다 가요").setContentText("다음 휴게소에서 만나요. 안전하게 오세요!").build();
 h.posted(c,new StatusBarNotification(c.getPackageName(),c.getPackageName(),7401,"noodoe.notification.test",android.os.Process.myUid(),0,0,n,android.os.Process.myUserHandle(),System.currentTimeMillis()));
 save("phone.a4",PanelCodec.packRoomy(PhonePanelRenderer.notification(c,"Galaxy S24 Ultra",87,h.snapshot(c).get(0),true)));
 }
}
