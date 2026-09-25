package io.opennoodoe.app.companion;
import android.content.Context;
import android.graphics.Typeface;
import android.text.TextPaint;
import java.util.Locale;
/** Bundled unmodified Noto CJK JP/TC supply region-correct glyphs independent
 * of OEM Han font subsets. Unsupported extended characters use Android fallback. */
public final class CjkFonts {
 private static Typeface jp,tc;private static String region="auto";
 public static synchronized void initialize(Context c){
  if(jp==null){jp=Typeface.createFromAsset(c.getAssets(),"fonts/NotoSansCJKjp-Regular.otf");tc=Typeface.createFromAsset(c.getAssets(),"fonts/NotoSansCJKtc-Regular.otf");}
  region=c.getSharedPreferences("companion",0).getString("cjk.region","auto");
 }
 private static boolean hasKana(String text){for(int i=0;i<text.length();i++){char c=text.charAt(i);if(c>=0x3040&&c<=0x30ff)return true;}return false;}
 public static void apply(TextPaint paint,String text,boolean bold){
  boolean japanese=region.equals("jp")||(!region.equals("tc")&&(Locale.getDefault().getLanguage().equals("ja")||hasKana(text)));
  paint.setTypeface(japanese?jp:tc);paint.setTextLocale(japanese?Locale.JAPAN:Locale.TAIWAN);paint.setFakeBoldText(bold);
 }
}
