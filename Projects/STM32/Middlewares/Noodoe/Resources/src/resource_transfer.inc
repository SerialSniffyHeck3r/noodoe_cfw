/* Included by ResourceStore: the existing bounded StorageTask writer owns NOR.
 * SPP only fills its idle import arena. Received bytes are RAM acknowledgements;
 * state SAVED alone means physical readback + content SHA + commit completed.
 * A reset discards reception; it never invalidates the running resource slot. */
enum { RX_IDLE, RX_PREPARE, RX_RECEIVE, RX_WRITE, RX_SAVED, RX_ERROR, RX_CANCELLED };
static volatile uint32_t rx_state,rx_position,rx_cancel;
static uint32_t rx_tx,rx_bytes,rx_error,rx_fill,rx_sequence;
static uint8_t rx_hash[32];
uint32_t ResourceStore_TransferActive(void)
{return rx_state==RX_PREPARE||rx_state==RX_RECEIVE||rx_state==RX_WRITE;}
uint32_t ResourceStore_TransferCancel(void)
{
 if(!ResourceStore_TransferActive())return 1;
 __atomic_store_n(&rx_cancel,1,__ATOMIC_RELEASE);return 0;
}
/* Only bounded RAM operations execute in the I/O owner. BEGIN is gated by a
 * fresh, connection-bound full FAT/recovery audit in NoodoeControl. */
uint32_t ResourceStore_Transfer(uint32_t op,const uint8_t *p,uint32_t n,uint8_t *out)
{
 uint32_t s=__atomic_load_n(&rx_state,__ATOMIC_ACQUIRE),r=0;
 if(op==0x8e){
  if(n!=40||!U32(p)||U32(p+4)<4096||U32(p+4)>SLOT||(U32(p+4)&4095))return 1;
  if(ResourceStore_TransferActive()){
   if(rx_tx!=U32(p)||rx_bytes!=U32(p+4)||memcmp(rx_hash,p+8,32))return 3;
  }else{
   if(!stage||phase||compatibility==1||compatibility==2||g_resource_install.sequence!=g_resource_install.ack||Resources_GetStatus()!=RESOURCES_READY)return 3;
   rx_tx=U32(p);rx_bytes=U32(p+4);memcpy(rx_hash,p+8,32);rx_position=rx_error=rx_fill=rx_cancel=rx_sequence=0;
   __atomic_store_n(&rx_state,RX_PREPARE,__ATOMIC_RELEASE);
  }
 }else if(op==0x8f){
  if(s!=RX_RECEIVE||n<10||n>968||U32(p)!=rx_tx||U32(p+4)!=rx_position||
     (n&1)||n-8>rx_bytes-rx_position||n-8>4096-(rx_position&4095))return 1;
  memcpy(stage+rx_position,p+8,n-8);__atomic_store_n(&rx_position,rx_position+n-8,__ATOMIC_RELEASE);
 }else if(op==0x90){
  if(n!=36||U32(p)!=rx_tx||memcmp(p+4,rx_hash,32))return 1;
  if(s==RX_WRITE||s==RX_SAVED){} /* Idempotent status of this exact resource. */
  else if(s!=RX_RECEIVE||rx_position!=rx_bytes)return 3;
  else __atomic_store_n(&rx_state,RX_WRITE,__ATOMIC_RELEASE);
 }else if(op==0x91){
  if(n!=4||U32(p)!=rx_tx)return 1;
  (void)ResourceStore_TransferCancel();
 }else if(op!=0x8d||n)return 1;
 P32(out,1);P32(out+4,rx_state);P32(out+8,rx_tx);P32(out+12,rx_position);
 P32(out+16,rx_bytes);P32(out+20,rx_error);P32(out+24,g_resource_install.progress);
 memcpy(out+28,g_resource_requirement.sha256,32);memcpy(out+60,rx_hash,32);
 return r;
}
/* Returns 1 while it owns the import arena, including wait-for-data. The
 * completed whole slot is immutable until the existing writer returns ack. */
static uint32_t TransferStep(void)
{
 uint32_t s=__atomic_load_n(&rx_state,__ATOMIC_ACQUIRE);
 if(s==RX_PREPARE||s==RX_RECEIVE){
  if(rx_cancel){rx_state=RX_CANCELLED;return 0;}
  if(s==RX_PREPARE){memset(stage+rx_fill,255,4096);rx_fill+=4096;
   if(rx_fill==SLOT)__atomic_store_n(&rx_state,RX_RECEIVE,__ATOMIC_RELEASE);}
  return 1;
 }
 if(s!=RX_WRITE)return 0;
 if(!rx_sequence){
  if(rx_cancel){rx_state=RX_CANCELLED;return 0;}
  if(Resources_CheckHeader(stage,rx_hash)||((U32(stage+8)+8191)&~4095U)!=rx_bytes){rx_error=24;rx_state=RX_ERROR;return 0;}
  rx_sequence=g_resource_install.sequence+1;if(!rx_sequence)rx_sequence=1;
  g_resource_install.command=2;g_resource_install.slot=1-g_resources.slot;
  g_resource_install.token=0x42414b32;__DMB();g_resource_install.sequence=rx_sequence;
 }
 if(g_resource_install.ack==rx_sequence){
  rx_error=g_resource_install.error;rx_sequence=0;
  __atomic_store_n(&rx_state,rx_error?RX_ERROR:RX_SAVED,__ATOMIC_RELEASE);return 0;
 }
 /* Do not interrupt the marker publication or report that a committed slot
  * was cancelled. At a sector boundary the inactive incomplete slot is safe. */
 if(rx_cancel&&phase&&phase<5){Done(25);rx_sequence=0;rx_state=RX_CANCELLED;return 0;}
 return 0;
}
