package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import java.io.IOException;
/** Negotiated payload limits; legacy peers retain their 512-byte contract. */
public final class TransferCapabilities {
 public final int chunk;public final boolean scopedV2,localSources,progress,resources,bootstrapResources,localInstall;
 private TransferCapabilities(int n,long flags){chunk=n;scopedV2=(flags&1)!=0;localSources=(flags&2)!=0;progress=(flags&8)!=0;resources=(flags&16)!=0;bootstrapResources=(flags&32)!=0;localInstall=(flags&64)!=0;}
 public static TransferCapabilities query(NdcpSession s)throws IOException {
  try{byte[] r=s.request(0x86,new byte[0]);
   if(r.length!=20||ByteCodec.u32le(r,4)!=2||ByteCodec.u32le(r,12)!=4096)throw new IOException("Transfer capability ABI mismatch");
   long n=ByteCodec.u32le(r,8);if(n!=512&&n!=960)throw new IOException("Unsupported DATA limit");
   return new TransferCapabilities((int)n,ByteCodec.u32le(r,16));
  }catch(NdcpSession.DeviceRejected e){if(e.result!=4)throw e;return new TransferCapabilities(512,0);}
 }
 public int count(int offset,int remaining){return Math.min(Math.min(chunk,remaining),4096-(offset&4095));}
}
