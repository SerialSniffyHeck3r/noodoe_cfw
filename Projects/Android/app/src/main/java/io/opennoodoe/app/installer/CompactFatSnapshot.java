package io.opennoodoe.app.installer;
import java.io.*;
import java.util.*;

/** Addressed metadata evidence, not a synthetic whole-volume image. Reads of
 * uncaptured ranges fail; unread stock file contents are never represented as
 * zeroes or used as a preservation hash. Payloads remain separate artifacts. */
final class CompactFatSnapshot implements BootstrapFatPlan.ReadAccess {
 private final File root;
 CompactFatSnapshot(File root)throws IOException{this.root=root;if(!root.isDirectory()&&!root.mkdirs())throw new IOException("Cannot create FAT evidence");}
 void put(long address,byte[] bytes)throws IOException{ScopedBackup.save(path(address),bytes);}
 private File path(long address){return new File(root,String.format(Locale.ROOT,"%08x.bin",address));}
 public byte[] read(long address,int bytes)throws IOException{
  File[] files=root.listFiles();if(files==null)throw new IOException("Missing FAT evidence");
  for(File f:files){if(!f.getName().matches("[0-9a-f]{8}\\.bin"))continue;long start=Long.parseLong(f.getName().substring(0,8),16);
   if(address>=start&&bytes>=0&&address-start<=f.length()&&bytes<=f.length()-(address-start)){
    byte[] out=new byte[bytes];try(RandomAccessFile in=new RandomAccessFile(f,"r")){in.seek(address-start);in.readFully(out);}return out;
   }
  }throw new IOException("Uncaptured FAT evidence range: "+Long.toHexString(address));
 }
 public void close(){}
}
