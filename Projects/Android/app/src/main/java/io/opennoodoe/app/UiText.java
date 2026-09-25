package io.opennoodoe.app;
import android.content.Context;
/** Resource lookup for worker-owned presentation as well as Activities.
 * Protocol identifiers and raw diagnostic details never pass through here. */
public final class UiText {
 private static volatile Context application;
 private static String language;private static android.content.res.Resources resources;
 private UiText(){}
 public static synchronized void initialize(Context context){application=context.getApplicationContext();language=null;resources=null;}
 public static synchronized String text(int resource,String fallback){
  Context context=application;if(context==null)return fallback;
  String next=AppLanguageSettings.resolvedTag(context);
  if(resources==null||!next.equals(language)){resources=AppLanguageSettings.wrap(context).getResources();language=next;}
  return resources.getString(resource);
 }
}
