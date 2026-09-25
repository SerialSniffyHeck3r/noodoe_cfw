package io.opennoodoe.app.maintenance;
import android.content.*;import android.graphics.*;import android.view.*;
import io.opennoodoe.app.*;
import org.junit.*;import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.*;
import java.io.*;import java.lang.reflect.Field;import java.util.*;import static org.junit.Assert.*;
@RunWith(RobolectricTestRunner.class) @Config(sdk=28,qualifiers="w393dp-h852dp-mdpi") @GraphicsMode(GraphicsMode.Mode.NATIVE)
public class CompanionLanguagesTest {
 @Test public void allSixCatalogsResolveAndSettingsSurviveRecreation()throws Exception {
  Context app=RuntimeEnvironment.getApplication();Shadows.shadowOf(RuntimeEnvironment.getApplication()).declareComponentUnbindable(new ComponentName(app,MaintenanceService.class));
  Set<String> titles=new HashSet<>();
  for(String locale:new String[]{"en","ko","zh-CN","zh-TW","ja","es"}){
   AppLanguageSettings.set(app,locale);assertEquals(locale,AppLanguageSettings.resolvedTag(app));
   Context localized=AppLanguageSettings.wrap(app);int count=0;
   for(Field f:R.string.class.getFields())if(f.getName().startsWith("companion_")){String value=localized.getString(f.getInt(null));assertFalse(f.getName(),value.isEmpty());count++;}
   assertEquals(712,count);titles.add(UiText.text(R.string.companion_0308,"fallback"));
   try(org.robolectric.android.controller.ActivityController<InstallerHomeActivity> c=Robolectric.buildActivity(InstallerHomeActivity.class).setup()){
    c.get().workflow(new SetupWorkflow.View("AA:01","product","","",false,false));
    View root=c.get().getWindow().getDecorView();root.measure(View.MeasureSpec.makeMeasureSpec(393,View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(852,View.MeasureSpec.EXACTLY));root.layout(0,0,393,852);
    Bitmap bitmap=Bitmap.createBitmap(393,852,Bitmap.Config.ARGB_8888);root.draw(new Canvas(bitmap));File dir=new File("build/language-preview");assertTrue(dir.isDirectory()||dir.mkdirs());
    try(FileOutputStream out=new FileOutputStream(new File(dir,locale+".png"))){bitmap.compress(Bitmap.CompressFormat.PNG,100,out);}bitmap.recycle();
    c.recreate();assertEquals(locale,AppLanguageSettings.tag(c.get()));assertEquals(localized.getString(R.string.companion_0308),UiText.text(R.string.companion_0308,"fallback"));
   }
  }
  assertEquals(6,titles.size());AppLanguageSettings.set(app,AppLanguageSettings.SYSTEM);
 }
}
