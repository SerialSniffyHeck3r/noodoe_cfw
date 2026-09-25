package io.opennoodoe.app.diagnostics;
import java.io.*;
/** fsync of the file alone does not persist a new name/rename. Android's
 * installation path must sync its containing directory before issuing writes.
 * JVM unit tests do not emulate Android filesystem power-loss guarantees. */
public final class DurableFiles {
 private DurableFiles(){}
 public static void directory(File directory)throws IOException {
  if(!System.getProperty("java.vm.name","").equals("Dalvik"))return;
  FileDescriptor fd=null;
  if(!directory.isDirectory())throw new IOException("Not a journal directory");
  try{fd=android.system.Os.open(directory.getAbsolutePath(),android.system.OsConstants.O_RDONLY,0);android.system.Os.fsync(fd);}
  catch(android.system.ErrnoException e){throw new IOException("Directory sync failed",e);}
  finally{if(fd!=null)try{android.system.Os.close(fd);}catch(android.system.ErrnoException e){throw new IOException("Directory close failed",e);}}
 }
}
