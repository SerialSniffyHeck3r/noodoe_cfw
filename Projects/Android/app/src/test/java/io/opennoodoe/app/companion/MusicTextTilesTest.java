package io.opennoodoe.app.companion;
import android.graphics.*;import android.text.*;
import org.junit.*;import org.junit.runner.RunWith;
import org.robolectric.*;import org.robolectric.annotation.*;
import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28) @GraphicsMode(GraphicsMode.Mode.NATIVE)
public class MusicTextTilesTest {
 @Before public void fonts(){CjkFonts.initialize(RuntimeEnvironment.getApplication());}
 @Test public void fixedFontAndBoundedTileForJapaneseTraditionalChinese(){
  String title="臺灣 台北 日本語 音楽 働く騎士 飛行機 世界中から長い音楽のタイトル";
  TextPaint paint=new TextPaint(Paint.ANTI_ALIAS_FLAG|Paint.SUBPIXEL_TEXT_FLAG);CjkFonts.apply(paint,title,true);paint.setTextSize(28);
  MusicTextTiles tiles=new MusicTextTiles(7,title,"歌手 藝術家");assertEquals(Math.max(304,(int)Math.ceil(Layout.getDesiredWidth(title,paint))+8),tiles.width);
  byte[] first=tiles.tile(9,0),next=tiles.tile(9,1);assertEquals(11184,first.length);assertEquals(7,CompanionWire.u(first,0));assertEquals(9,CompanionWire.u(first,4));assertEquals(1,CompanionWire.u(next,8));
  boolean ink=false;for(int i=16;i<6928;i++)ink|=first[i]!=0;assertTrue(ink);
  for(int i=6928;i<first.length;i++)assertEquals(first[i],next[i]);
 }
 @Test public void shortTextCenteredAndHugeInputCannotGrowTileBuffer(){
  MusicTextTiles shortText=new MusicTextTiles(1,"Ride","Artist");assertEquals(304,shortText.width);byte[] b=shortText.tile(2,0);
  int min=384,max=0;for(int y=0;y<36;y++)for(int x=0;x<384;x++){int v=b[16+y*192+x/2]&255;v=(x&1)==0?v>>4:v&15;if(v!=0){min=Math.min(min,x);max=Math.max(max,x);}}
  assertTrue(Math.abs((min+max)/2f-151.5f)<3);
  String huge=new String(new char[10000]).replace('\0','漢');MusicTextTiles large=new MusicTextTiles(2,huge,huge);assertTrue(large.width<=32768);assertEquals(11184,large.tile(1,(large.width-1)/384).length);
 }
}
