package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import org.junit.Test;
import static org.junit.Assert.*;
import java.io.*;
import java.util.*;

/** Wire-level read/query test. No radio or physical recovery is simulated. */
public class DiagnosticSessionTest {
 static class Radio implements InstallerTransport {
  byte[] pending;int mutations,role=3;long epoch=91;
  public void send(byte[] f)throws IOException {
   int op=f[5]&255,seq=(int)ByteCodec.u32le(f,8),n=ByteCodec.u16le(f,12);byte[] r;
   if(op==0x58)r=Arrays.copyOf(NdcpSession.words(0,1,role,1,2,3),88);
   else if(op==0x96&&n==0){r=new byte[76];ByteCodec.putU32le(r,4,1);ByteCodec.putU32le(r,8,3);ByteCodec.putU32le(r,72,epoch);}
   else if(op==0x96&&n==8){assertEquals(epoch,ByteCodec.u32le(f,16));assertEquals(7,ByteCodec.u32le(f,20));mutations++;r=NdcpSession.words(0,1);}
   else throw new IOException("Unexpected command "+op+" bytes "+n);
   pending=NdcpSession.encode(op,seq,r,1);
  }
  public byte[] receive(long timeout){byte[] r=pending;pending=null;return r;}
  public void close(){}
 }
 @Test public void statusIsReadOnly()throws Exception {Radio r=new Radio();assertTrue(DiagnosticSession.view(new NdcpSession(r),false).contains("진단 모드"));assertEquals(0,r.mutations);}
 @Test public void returnOnlyOpensPhysicalConfirmation()throws Exception {Radio r=new Radio();String hint=DiagnosticSession.view(new NdcpSession(r),true);assertTrue(hint.contains("2초"));assertEquals(1,r.mutations);}
 @Test public void productCannotBeMistakenForDiagnostic()throws Exception {Radio r=new Radio();r.role=2;try{DiagnosticSession.view(new NdcpSession(r),true);fail();}catch(IOException expected){}assertEquals(0,r.mutations);}
}
