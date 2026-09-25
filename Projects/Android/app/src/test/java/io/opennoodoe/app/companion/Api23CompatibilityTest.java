package io.opennoodoe.app.companion;
import org.junit.Test;import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.Config;
import androidx.exifinterface.media.ExifInterface;import java.io.*;import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=23)
public class Api23CompatibilityTest {
 @Test public void photoOrientationReadsFromStreamOnAndroid6()throws Exception {
  String hex="ffd8ffe1002245786966000049492a0008000000010012010300010000000600000000000000ffd9";
  byte[] jpeg=new byte[hex.length()/2];for(int i=0;i<jpeg.length;i++)jpeg[i]=(byte)Integer.parseInt(hex.substring(i*2,i*2+2),16);
  ExifInterface exif=new ExifInterface(new ByteArrayInputStream(jpeg));assertEquals(6,exif.getAttributeInt(ExifInterface.TAG_ORIENTATION,1));
 }
}
