/* Included by the sole Bootstrap storage owner. A fresh-install scope is
 * derived independently from FAT first-fit, never an arbitrary write grant.
 * Existing files stay read-only; the caller backs up metadata and new extents.
 * Whole unchanged-NOR hashes are computed locally before/after publication. */
static uint32_t FastRequest(BootstrapStorage *s,uint32_t op,const uint8_t *p,uint32_t n,uint32_t *extra)
{
 if(!Allowed(s))return BS_DENIED;
 if(op==0x88){
  /* Explicit fresh-install replacement of OWNED CFW A/B/journal, or one
   * resource slot while retaining the other verified slot. Never replace stock,
   * settings, photos, logs or FAT. Host retains the physical old bytes/hash.
   * Ownership and preimage are checked again on device before issuing a lease. */
  uint32_t kind=n>=4?U32(p):BS_FILE_COUNT;
  if(n!=104||!s->scope_v2||!s->scoped||s->scope_after||!s->backup_valid||
     (kind!=BS_RESOURCE&&(kind<BS_GATE_A||kind>BS_GATE_JOURNAL))||!(s->scope_existing&(1U<<kind))||
     !(s->verified_files&(1U<<BS_RECOVERY))||memcmp(p+40,s->backup_hash,32)||
     U32(p+4)!=(kind==BS_RESOURCE?524288U:sizes[kind])||!s->arena||sizes[kind]>s->capacity||
     (s->state!=BS_IDLE&&s->state!=BS_SAVED))return BS_DENIED;
  s->kind=kind;s->resource_update=kind==BS_RESOURCE;s->bytes=U32(p+4);s->first=s->scope_first[kind];s->position=s->phase=s->error=0;
  s->reseed=1;s->scope_verified=0;s->verified_files&=~(1U<<kind);
  memcpy(s->expected_hash,p+8,32);memcpy(s->reseed_before,p+72,32);
  s->state=BS_RESEED_CHECK;UpdateSha256_Init(&s->hash);return 0;
 }
 if(op==0x8a){
  if(n||!s->scope_v2||s->state!=BS_UPLOADING||s->position||
     !(s->kind==BS_CONFIG||s->kind==BS_RIDE||s->kind==BS_PHOTO||s->kind==BS_LOG||s->kind==BS_GATE_JOURNAL))return BS_ORDER;
  if(s->kind==BS_GATE_JOURNAL&&!(s->verified_files&(1U<<BS_GATE_A)))return BS_DENIED;
  s->local_source=2;s->local_remaining=s->bytes;s->state=BS_LOCAL_COPY;return 0;
 }
 if(op==0x89){
  if(n||s->state!=BS_SAVED||!(s->verified_files&(1U<<s->kind)))return BS_ORDER;
  memcpy(s->reply+32,s->inspected_hash,32);*extra=32;return 0;
 }
 if(op==0x87){
  if(n!=16||!s->scope_v2||s->state!=BS_UPLOADING||U32(p)!=s->position||U32(p+4)>1||
    !U32(p+12)||U32(p+12)>s->bytes-s->position||((U32(p+8)|U32(p+12))&1U))return BS_ORDER;
  uint32_t source=U32(p+4),off=U32(p+8),bytes=U32(p+12);
  if(source? (s->kind!=BS_GATE_B||s->position!=4096||off||bytes!=GATE_PRODUCT_BYTES||!(s->verified_files&(1U<<BS_GATE_A))) :
    (s->kind!=BS_RECOVERY||s->position!=4096||off||bytes!=RECOVERY_IMAGE_BYTES||!s->read_embedded))return BS_DENIED;
  s->local_source=source;s->local_offset=off;s->local_remaining=bytes;s->state=BS_LOCAL_COPY;return 0;
 }
 if(op==0x83){
  if(n!=8||s->state!=BS_UPLOADING||U32(p)!=s->position||!U32(p+4)||U32(p+4)>65536||U32(p+4)>s->bytes-s->position)return BS_ORDER;
  memset(s->arena+s->position,255,U32(p+4));s->position+=U32(p+4);return 0;
 }
 /* Missing files from startup inspection may be retried with a fresh audit. */
 if(s->state==BS_FAILED&&!s->scoped){s->state=BS_IDLE;s->error=0;}
 if(s->state!=BS_IDLE&&s->state!=BS_SAVED)return BS_BUSY;
 if(op==0x80){
  if(n!=8||!U32(p+4)||U32(p+4)>BOOTSTRAP_STORAGE_CHUNK||U32(p)>=0x8000000||U32(p+4)>0x8000000-U32(p))return BS_ARGUMENT;
  if(s->io.read_raw(s->io.context,U32(p),s->reply+32,U32(p+4)))return BS_IO;
  s->backup_position=U32(p)+U32(p+4);*extra=U32(p+4);return 0;
 }
 if(op==0x81){
  /* A stock round trip changes staging bytes. Reuse audits existing files
   * against a NEW read-only baseline; it never claims the old hash survived.
   * RUSE is deliberately distinct from a historical-baseline resume. */
  if((n!=68&&n!=100&&n!=72&&n!=76)||s->scoped||(n==72&&U32(p+68)!=0x45535552)||(n==76&&(U32(p+68)!=0x32504353||U32(p+72)>1)))return BS_ARGUMENT;
  s->reseed=0;s->scope_v2=n==76;s->scope_existing=s->scope_after=0;s->scope_reuse=n==72;s->scope_resume=n!=68;
  if(n==100)memcpy(s->scope_before,p+68,32);
  memcpy(s->scope_metadata,p,32);
  for(uint32_t k=0;k<BS_FILE_COUNT;k++)s->scope_first[k]=U32(p+32+4*k);
  s->scope_verified=s->backup_valid=s->verified_files=0;
  RecoveryStore_Init(&s->audit,s->io.read_raw,s->io.context,s->uid);s->audit.resident=s->resident;
  s->phase=s->position=0;s->state=BS_SCOPE_AUDIT;return 0;
 }
 if(op==0x82){
  if(n||!s->scoped||s->verified_files!=511U)return BS_DENIED;
  s->scope_verified=0;s->scope_after=1;s->position=0;
  if(s->scope_v2){s->scope_resume=1;s->scope_reuse=1;s->phase=0;
   RecoveryStore_Init(&s->audit,s->io.read_raw,s->io.context,s->uid);s->audit.resident=s->resident;s->state=BS_SCOPE_AUDIT;
  }else{s->state=BS_SCOPE_HASH;UpdateSha256_Init(&s->hash);}return 0;
 }
 return BS_ARGUMENT;
}
static uint32_t FastExcluded(const BootstrapStorage *s,uint32_t at)
{
 if(at>=0x1000&&at<0x9000)return 1;
 for(uint32_t k=0;k<BS_FILE_COUNT;k++)if(at>=s->scope_first[k]&&at-s->scope_first[k]<sizes[k])return 1;
 return 0;
}
static void FastProcess(BootstrapStorage *s)
{
 if(!Allowed(s)){Fail(s,BS_DENIED);return;}
 if(s->state==BS_SCOPE_AUDIT){
  if(!s->phase){uint32_t st=RecoveryStore_Process(&s->audit);
   if(st==RECOVERY_STORE_FAILED){Fail(s,BS_FORMAT);return;}
   if(st==RECOVERY_STORE_READY||st==RECOVERY_STORE_MISSING){
    if(!s->scope_v2&&(st==RECOVERY_STORE_READY)!=!!s->scope_resume){Fail(s,BS_COLLISION);return;}
    s->phase=1;
   }return;
  }
  if(Logical(s,s->position,s->metadata+s->position,4096)){Fail(s,BS_IO);return;}
  s->position+=4096;if(s->position!=sizeof(s->metadata))return;
  uint8_t h[32];Hash(s->metadata,sizeof(s->metadata),h);
  if(!s->scope_after&&memcmp(h,s->scope_metadata,32)){Fail(s,BS_HASH);return;}
  /* Same declared install order, but allocation is recomputed on-device. */
  static const uint8_t order[]={4,0,1,2,3,8,5,6,7};
  for(uint32_t i=0;i<BS_FILE_COUNT;i++){
   s->kind=order[i];s->bytes=sizes[s->kind];s->resource_update=0;
   if(s->scope_resume){
    uint32_t found=0,first=0;
    for(uint32_t j=0;j<512;j++){const uint8_t *p=s->metadata+0x5000+j*32;
     if(!p[0])break;
     if(p[0]!=0xe5&&p[11]!=15&&!memcmp(p,names[s->kind],11)){
      if((p[11]&0x18)||U32(p+28)!=s->bytes){Fail(s,BS_FORMAT);return;}
      found++;first=U16(p+26);
     }
    }
    if(!found&&s->scope_v2&&!s->scope_after){uint32_t e=Prepare(s);if(e||s->first!=s->scope_first[s->kind]){Fail(s,e?e:BS_HASH);return;}continue;}
    s->scope_existing|=1U<<s->kind;
    if(found!=1||first<2||first+s->bytes/32768>4080||0x9000+(first-2)*32768!=s->scope_first[s->kind]||s->scope_first[s->kind]>RECOVERY_STORE_SAFE_END-s->bytes){Fail(s,BS_FORMAT);return;}
    for(uint32_t j=0;j<s->bytes/32768;j++){uint32_t next=Fat(s,first+j);
     if(j+1<s->bytes/32768?next!=first+j+1:next<0xff8){Fail(s,BS_FORMAT);return;}
    }
   }else{uint32_t e=Prepare(s);if(e||s->first!=s->scope_first[s->kind]){Fail(s,e?e:BS_HASH);return;}}
  }
  s->scoped=1;
  if(s->scope_v2){
   /* Scope proof means audited ownership plus constrained writes. It is not a
    * hash of untouched file content. Host separately compares final metadata. */
   if(s->scope_after)s->scope_verified=1;
   else{memcpy(s->scope_before,s->scope_metadata,32);memcpy(s->backup_hash,s->scope_before,32);}
   s->backup_valid=1;s->state=BS_SAVED;return;
  }
  s->scope_after=s->scope_resume&&!s->scope_reuse;s->position=0;s->state=BS_SCOPE_HASH;UpdateSha256_Init(&s->hash);return;
 }
 /* Include addresses and fixed-size physical blocks to bind ordering/ranges. */
 if(!FastExcluded(s,s->position)){
  if(s->io.read_raw(s->io.context,s->position,s->work,4096)){Fail(s,BS_IO);return;}
  uint8_t address[4];P32(address,s->position);UpdateSha256_Feed(&s->hash,address,4);
  UpdateSha256_Feed(&s->hash,s->work,4096);
 }
 s->position+=4096;
 if(s->position==0x8000000){uint8_t h[32];UpdateSha256_Final(&s->hash,h);
  if(s->scope_after){if(memcmp(h,s->scope_before,32)){Fail(s,BS_HASH);return;}s->scope_verified=1;}
  else memcpy(s->scope_before,h,32);
  memcpy(s->backup_hash,h,32);s->backup_valid=1;
  s->state=BS_SAVED;
 }
}
