package io.opennoodoe.app.companion;
import android.content.*;import android.media.*;import android.media.session.*;import android.os.*;
import io.opennoodoe.app.protocol.ndcp.NdcpClient;
import org.junit.*;import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.*;import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28) @GraphicsMode(GraphicsMode.Mode.NATIVE)
public class MediaBurstTest {
 @Test public void optionalMediaIdAndBriefNullCannotRestartSameSong()throws Exception{
  Context c=RuntimeEnvironment.getApplication();MediaSession session=new MediaSession(c,"music");MediaController player=session.getController();
  Shadows.shadowOf(player).setPackageName("test.player");Shadows.shadowOf(player).setPlaybackState(new PlaybackState.Builder().setState(PlaybackState.STATE_PLAYING,1000,1).build());
  Shadows.shadowOf(c.getSystemService(MediaSessionManager.class)).addController(player);
  VisualTransferTest.Radio radio=new VisualTransferTest.Radio();radio.ready=true;CompanionWire wire=new CompanionWire(new NdcpClient(radio));wire.connect();
  try(CompanionRuntime runtime=new CompanionRuntime(c,wire)){
   Shadows.shadowOf(Looper.getMainLooper()).idle();
   for(int i=0;i<40;i++){
    MediaMetadata m=new MediaMetadata.Builder().putString(MediaMetadata.METADATA_KEY_TITLE,"Same song").putString(MediaMetadata.METADATA_KEY_ARTIST,"Same artist").putString(MediaMetadata.METADATA_KEY_MEDIA_ID,i%2==0?null:"optional-id").build();
    Shadows.shadowOf(player).setMetadata(i%5==4?null:m);Shadows.shadowOf(player).executeOnMetadataChanged(i%5==4?null:m);Shadows.shadowOf(Looper.getMainLooper()).idle();
    SystemClock.sleep(200);runtime.tick();
   }
   assertEquals("1",runtime.gpsDiagnostics().get("track_changes"));assertEquals(1,radio.begins);assertEquals(1,radio.finishes);
   Shadows.shadowOf(player).setMetadata(new MediaMetadata.Builder().putString(MediaMetadata.METADATA_KEY_TITLE,"Next song").putString(MediaMetadata.METADATA_KEY_ARTIST,"Same artist").build());
   SystemClock.sleep(200);runtime.tick();assertEquals("2",runtime.gpsDiagnostics().get("track_changes"));
  }finally{Shadows.shadowOf(Looper.getMainLooper()).idle();session.release();}
 }
}
