#ifndef BOOTSTRAP_STORAGE_H
#define BOOTSTRAP_STORAGE_H
#include <stddef.h>
#include "Recovery_Store.h"
#define BOOTSTRAP_STORAGE_CHUNK 960U
enum { BS_IDLE,BS_BACKUP_A,BS_BACKUP_B,BS_UPLOADING,BS_VERIFYING,BS_AUDITING,BS_PREPARED,BS_WRITING,BS_SAVED,BS_FAILED,BS_INSPECTING,BS_HASHING,BS_SCOPE_AUDIT,BS_SCOPE_HASH,BS_LOCAL_COPY,BS_RESEED_CHECK };
enum { BS_OK,BS_ARGUMENT,BS_DENIED,BS_BUSY,BS_IO,BS_HASH,BS_FORMAT,BS_COLLISION,BS_SPACE,BS_ORDER,BS_TIMEOUT };
enum { BS_RESOURCE,BS_CONFIG,BS_RIDE,BS_PHOTO,BS_RECOVERY,BS_GATE_A,BS_GATE_B,BS_GATE_JOURNAL,BS_LOG,BS_FILE_COUNT };
typedef struct {
    void *context;
    uint32_t (*read_raw)(void *,uint32_t,void *,uint32_t);
    uint32_t (*grant)(void *,uint32_t first,uint32_t bytes,uint32_t metadata_mask);
    uint32_t (*erase)(void *,uint32_t);
    uint32_t (*program)(void *,uint32_t,const void *,uint32_t);
    void (*lock)(void *);
    uint32_t (*ign_on)(void *);
} BootstrapStoragePlatform;
/* One storage owner invokes all methods. The BT owner queues requests, never
 * calls them concurrently. arena is caller-owned >=1MiB upload storage, held
 * until SAVED/FAILED. RecoveryStore used by early rescue is a separate context. */
typedef struct BootstrapStorageTag {
    BootstrapStoragePlatform io;RecoveryStore audit;
    const uint8_t *resident;
    uint8_t *arena;uint32_t capacity,uid[3],epoch,authenticated,state,error,kind;
    uint32_t backup_position,backup_pass,backup_valid,position,bytes,phase,first,root_index,page,sector;
    uint32_t request_pending,reply_pending,request_op,request_size,reply_size;
    uint32_t verified_files,inspect_cluster;
    uint32_t bench_active,bench_sequence,bench_command;
    uint32_t resource_update,resource_total;
    uint32_t scoped,scope_verified,scope_after,scope_resume,scope_reuse,scope_first[BS_FILE_COUNT];
    uint8_t scope_metadata[32],scope_before[32];
    uint8_t resource_old_hash[32];
    uint8_t backup_hash[32],expected_hash[32],metadata_hash[32];
    /* Valid only with the corresponding verified_files bit. */
    uint8_t gate_sha[2][32],gate_boot_sha[32];
    uint8_t request[1024],reply[1024],metadata[0x9000],work[4096];
    /* v2 scopes prove FAT ownership/range confinement instead of claiming a
     * whole-NOR content hash. An accepted write keeps its immutable lease until
     * file publication finishes, even after the transport epoch is revoked. */
    uint32_t scope_v2,scope_existing,write_lease,draining;
    uint32_t local_source,local_offset,local_remaining;
    uint8_t inspected_hash[32],reseed_before[32];
    uint32_t reseed;
    /* Resource reseed preserves one valid slot and binds the other preimage. */
    uint32_t resource_preserve_slot;
    uint8_t resource_before_hash[32];
    uint32_t (*read_embedded)(void *,uint32_t,void *,uint32_t);
    UpdateSha256 hash;
} BootstrapStorage;
/* Sole owner, between bounded physical operations. Never formats or retries. */
void BootstrapStorage_Timeout(BootstrapStorage *);
/* Returns 1 only after an accepted physical write has finished safely. */
uint32_t BootstrapStorage_Cancel(BootstrapStorage *);
uint32_t BootstrapStorage_ReadProduct(BootstrapStorage *,uint32_t,void *,uint32_t);
void BootstrapStorage_Init(BootstrapStorage *,const BootstrapStoragePlatform *,void *arena,uint32_t capacity,const uint32_t uid[3]);
/* authenticated means caller verified paired/encrypted link and local intent;
 * UID/token are NOT authentication. Different/disconnected epoch revokes writes. */
void BootstrapStorage_SetSession(BootstrapStorage *,uint32_t epoch,uint32_t authenticated);
uint32_t BootstrapStorage_Request(BootstrapStorage *,uint32_t opcode,const void *,uint32_t bytes);
void BootstrapStorage_Process(BootstrapStorage *);
uint32_t BootstrapStorage_TakeReply(BootstrapStorage *,void *,uint32_t capacity,uint32_t *bytes);
/* Read-only validation of existing files, including a pre-provisioned donor.
 * Caller may run this before authenticating; it grants no write permission. */
uint32_t BootstrapStorage_Inspect(BootstrapStorage *,uint32_t kind);
uint32_t BootstrapStorage_FilesReady(const BootstrapStorage *);
uint32_t BootstrapStorage_ResourceCompatible(const BootstrapStorage *,const uint8_t sha256[32]);
typedef struct {
    uint32_t magic,version,buffer,capacity,command,token,uid[3];
    uint8_t backup_sha256[32],image_sha256[32];
    uint32_t sequence,ack,state,error,first,bytes,metadata;
} BootstrapStorageBenchMailbox;
_Static_assert(sizeof(BootstrapStorageBenchMailbox)==128,"Bootstrap SWD mailbox ABI");
_Static_assert(offsetof(BootstrapStorageBenchMailbox,sequence)==100,"Bootstrap SWD sequence ABI");
_Static_assert(offsetof(BootstrapStorageBenchMailbox,ack)==104,"Bootstrap SWD acknowledgement ABI");
extern volatile BootstrapStorageBenchMailbox g_bootstrap_storage_bench;
/* Physically attached SWD-only REC creation. Host must freshly validate exact
 * APP/UID, full independent A/B files and every live preimage. This mailbox's
 * token is intent, not remote authentication or proof host files exist. Host
 * writes fields before publishing a new sequence last. One outstanding request
 * only; exception command3 cancels one pending operation with a new sequence.
 * Device publishes status before ack. Host checks matching ack and state/error.
 * PREPARE=1 accepts only approved REC; COMMIT=2 cannot replace an existing file.
 * HASH=4 reads all128MiB raw NOR, publishes its32-byte SHA at image_sha256,
 * then ack. It neither grants writes nor represents a downloaded full backup.
 * PREPARE_RESOURCE_B=8 accepts a required512KiB package only when an existing
 * valid A package is preserved and B is wholly erased. No FAT edits occur. */
void BootstrapStorage_BenchProcess(BootstrapStorage *);
uint32_t BootstrapStorage_BenchActive(const BootstrapStorage *);
#endif
