package io.opennoodoe.app.installer;
import io.opennoodoe.app.protocol.ByteCodec;
import org.junit.Test;
import static org.junit.Assert.*;
import java.io.*;
import java.nio.file.Files;
import java.util.*;
public class BootHandoffSessionTest {
 static final class Clock implements BootHandoffSession.Clock {
  long time=System.nanoTime();int sleeps;public long now(){return time;}
  public void sleep(long ms){time+=ms*1000000;sleeps++;}
 }
 static final class Radio implements InstallerTransport {
  final RecoveryBundle b;final List<Integer> ops=new ArrayList<>();byte[] reply;int role,queries;boolean wrongUid,acked,closed;int loseOpcode,bootQueries,confirmAtQuery;
  Radio(RecoveryBundle b,int role){this.b=b;this.role=role;}
  public void send(byte[] f)throws IOException {
   int op=f[5]&255,seq=(int)ByteCodec.u32le(f,8);ops.add(op);byte[] r;
   try {switch(op){
    case 0x58:
     if(role==1&&++queries>12)throw new IOException("Expected installer restart");
     r=Arrays.copyOf(NdcpSession.words(0,1,role,wrongUid?9:1,2,3),88);
     System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(b.paddedImage(role==1?"bootstrap":"cfw"))),0,r,24,32);break;
    case 0x45:r=Arrays.copyOf(NdcpSession.words(0,5),84);break;
    case 0x64:r=Arrays.copyOf(NdcpSession.words(0,2),40);System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(Arrays.copyOf(b.paddedImage("cfw"),0x10000))),0,r,8,32);break;
    case 0x5b:if(++bootQueries==confirmAtQuery)acked=true;r=Arrays.copyOf(NdcpSession.words(0,2,acked?8:7,acked?4:3,acked?0:1),80);System.arraycopy(NdcpSession.hex(RecoveryBundle.sha(GateContainers.product(b.paddedImage("cfw")))),0,r,48,32);break;
    case 0x0d:r=NdcpSession.words(0,2,2,65536);break;
    case 0x9c:r=f.length==20?NdcpSession.words(0,170000):NdcpSession.words(0);break;
    case 0x5c:assertEquals(7,ByteCodec.u32le(f,16));assertTrue(ops.contains(0x9c));acked=true;r=NdcpSession.words(0);break;
    default:throw new AssertionError("Unexpected command "+op);
   }}catch(IOException e){throw e;}catch(Exception e){throw new IOException(e);}
   if(op==loseOpcode)throw new IOException("lost response for "+op);
   reply=NdcpSession.encode(op,seq,r,1);
  }
  public byte[] receive(long ms){assertTrue("boot query must not wait 60s",ms<=5000);byte[] r=reply;reply=null;return r;}public void close(){closed=true;}
 }
 private InstallJournal journal()throws Exception {
  InstallJournal j=new InstallJournal(new File(Files.createTempDirectory("handoff").toFile(),"journal"));j.values.setProperty("uid","1,2,3");j.save("WAIT_CFW_BOOT");return j;
 }
 @Test public void waitsForLiveBootstrapThenConfirmsExactProduct()throws Exception {
  RecoveryBundle b=new InstallerTest().valid();Radio boot=new Radio(b,1),product=new Radio(b,2);int[] opens={0};InstallJournal j=journal();Clock clock=new Clock();
  InstallerController.Connections c=new InstallerController.Connections(){public InstallerTransport open(){fail("must use boot connection budget");return null;}public InstallerTransport openBoot(StockUpdateSession.Progress p){return opens[0]++==0?boot:product;}};
  int[] screens={0};new BootHandoffSession(clock).run(c,b,j,new StockUpdateSession.Progress(){public void update(String s){}public void awaitScreen(String binding,long deadline){screens[0]++;assertFalse(product.acked);}});
  assertEquals(1,screens[0]);assertNotNull(j.values.getProperty("visual.confirmed"));
  assertEquals(2,opens[0]);assertEquals(13,boot.queries);assertTrue(boot.closed&&product.closed);assertEquals("CFW_CONFIRMED",j.state());
  assertEquals(1,Collections.frequency(product.ops,0x5c));for(Radio r:Arrays.asList(boot,product))for(int op:new int[]{0x40,0x41,0x43,0x47})assertFalse(r.ops.contains(op));
 }
 @Test public void wrongUidIsTerminalAndDoesNotAcknowledge()throws Exception {
  RecoveryBundle b=new InstallerTest().valid();Radio radio=new Radio(b,2);radio.wrongUid=true;int[] opens={0};
  try{new BootHandoffSession(new Clock()).run(()->{opens[0]++;return radio;},b,journal(),m->{});fail();}catch(ProductBootSession.TerminalFailure expected){}
  assertEquals(1,opens[0]);assertEquals(Arrays.asList(0x58),radio.ops);
 }
 @Test public void radioAbsenceStopsAtDeadlineAndKeepsUnknownJournal()throws Exception {
  RecoveryBundle b=new InstallerTest().valid();int[] opens={0};InstallJournal j=journal();
  try{new BootHandoffSession(new Clock()).run(()->{opens[0]++;throw new IOException("offline");},b,j,m->{});fail();}catch(IOException expected){}
  assertTrue(opens[0]>5);assertTrue(opens[0]<60);assertEquals("WAIT_CFW_BOOT",j.state());
 }
 @Test public void interruptedWaitDoesNotOpenAnotherSocket()throws Exception {
  BootHandoffSession.Clock c=new BootHandoffSession.Clock(){public long now(){return 1;}public void sleep(long ms)throws InterruptedException{throw new InterruptedException();}};
  try{new BootHandoffSession(c).run(()->{fail();return null;},new InstallerTest().valid(),journal(),m->{});fail();}catch(InterruptedException expected){}
 }
 @Test public void singleSocketBudgetDoesNotNestThreeRetries()throws Exception {
  int[] calls={0};try{SppConnectPolicy.connect(new SppConnectPolicy.Attempt<Object>(){public Object open(int n)throws IOException{calls[0]++;throw new IOException();}public void prepareRetry(int n){fail();}},1);fail();}catch(IOException expected){}
  assertEquals(1,calls[0]);
 }
 @Test public void sixQuickFailuresBeforeRadioReadyDoNotAbandonTrial()throws Exception {
  RecoveryBundle b=new InstallerTest().valid();int[] opens={0};Radio r=new Radio(b,2);InstallJournal j=journal();
  new BootHandoffSession(new Clock()).run(()->{if(opens[0]++<6)throw new IOException("Gate still flashing");return r;},b,j,new StockUpdateSession.Progress(){public void update(String s){}public void awaitScreen(String b,long d){}});
  assertEquals(7,opens[0]);assertEquals("CFW_CONFIRMED",j.state());assertEquals("6",j.values.getProperty("boot.reconnect.failures"));
 }
 @Test public void lostHealthAckQueriesDurableResultWithoutRepeatingAckOrInstall()throws Exception {
  RecoveryBundle b=new InstallerTest().valid();Radio first=new Radio(b,2),next=new Radio(b,2);first.loseOpcode=0x5c;int[] opens={0},screens={0};InstallJournal j=journal();
  new BootHandoffSession(new Clock()).run(()->{if(opens[0]++==0)return first;next.acked=first.acked;return next;},b,j,new StockUpdateSession.Progress(){public void update(String s){}public void awaitScreen(String b,long d){screens[0]++;}});
  assertTrue(first.closed&&next.closed);assertEquals(2,opens[0]);assertEquals(1,screens[0]);assertEquals("CFW_CONFIRMED",j.state());
  assertFalse(next.ops.contains(0x5c));for(Radio r:Arrays.asList(first,next))for(int op:new int[]{0x40,0x41,0x43,0x47})assertFalse(r.ops.contains(op));
 }
 @Test public void uncommittedHealthAckRebindsOnNewConnectionWithoutAskingTwice()throws Exception {
  RecoveryBundle b=new InstallerTest().valid();Radio first=new Radio(b,2),next=new Radio(b,2);first.loseOpcode=0x5c;
  // Simulate no durable confirmation: the old connection disappeared before
  // the storage owner could accept that generation's health acknowledgement.
  int[] opens={0},screens={0};InstallJournal j=journal();
  new BootHandoffSession(new Clock()).run(()->opens[0]++==0?first:next,b,j,new StockUpdateSession.Progress(){public void update(String s){}public void awaitScreen(String b,long d){screens[0]++;}});
  assertEquals(1,screens[0]);assertEquals(1,Collections.frequency(next.ops,0x5c));assertTrue(next.ops.contains(0x9c));assertEquals("CFW_CONFIRMED",j.state());
 }
 @Test public void confirmationCommittingBetweenSnapshotsIsNotTerminalFailure()throws Exception {
  RecoveryBundle b=new InstallerTest().valid();Radio radio=new Radio(b,2);radio.confirmAtQuery=3;InstallJournal j=journal();
  new BootHandoffSession(new Clock()).run(()->radio,b,j,new StockUpdateSession.Progress(){public void update(String s){}public void awaitScreen(String b,long d){}});
  assertEquals("CFW_CONFIRMED",j.state());assertFalse(radio.ops.contains(0x5c));
 }
 @Test public void connectedButUnapprovedPollsWithoutHealthAckThenConfirms()throws Exception {
  RecoveryBundle b=new InstallerTest().valid();Radio radio=new Radio(b,2);InstallJournal j=journal();List<String> stages=new ArrayList<>();
  new BootHandoffSession(new Clock()).run(()->radio,b,j,new StockUpdateSession.Progress(){
   public void update(String s){}public void stage(String key,String t,long d,long n,String u){stages.add(key);}
   public void awaitScreen(String binding,long deadline,StockUpdateSession.ScreenCheck check)throws Exception {
    assertTrue(stages.contains("screen-confirm"));assertEquals("screen_confirmation",j.values.getProperty("boot.waiting.for"));
    int before=radio.bootQueries;for(int i=0;i<5;i++){check.poll();assertFalse(radio.acked);assertFalse(radio.ops.contains(0x5c));}
    assertEquals(before+5,radio.bootQueries);
   }
  });
  assertTrue(stages.indexOf("screen-confirm")<stages.lastIndexOf("health"));assertEquals("CFW_CONFIRMED",j.state());
  assertFalse(j.values.containsKey("boot.waiting.for"));
  assertEquals(1,Collections.frequency(radio.ops,0x5c));
 }
 @Test public void missingUserApprovalIsNotRetriedAsBluetoothFailure()throws Exception {
  RecoveryBundle b=new InstallerTest().valid();Radio radio=new Radio(b,2);InstallJournal j=journal();int[] opens={0};
  try{new BootHandoffSession(new Clock()).run(()->{opens[0]++;return radio;},b,j,new StockUpdateSession.Progress(){
   public void update(String s){}public void awaitScreen(String key,long d)throws Exception{throw new StockUpdateSession.ScreenConfirmationTimeout("user did not confirm");}
  });fail();}catch(StockUpdateSession.ScreenConfirmationTimeout expected){}
  assertEquals(1,opens[0]);assertFalse(radio.acked);assertFalse(j.values.containsKey("visual.confirmed"));
  assertFalse(j.values.containsKey("boot.reconnect.failures"));assertEquals("screen_confirmation",j.values.getProperty("boot.waiting.for"));
 }
}
