#ifndef PHONE_CONTENT_H
#define PHONE_CONTENT_H
#include <stdint.h>
#define PHONE_CONTENT_SLOTS 1U
#define PHONE_NOTIFICATION_CAPACITY 10U
#define PHONE_ART_SIDE 32U
#define PHONE_ART_BYTES (PHONE_ART_SIDE*PHONE_ART_SIDE*2U)
typedef struct {uint32_t id,revision,visual_key,replyable;char app[24],title[48],body[96];} PhoneNotification;
/* Adapter supplies newest-first notifications, including an explicit known
 * empty list. Strings are UTF-8 input; current ASCII font replaces unsupported
 * codepoints once, rather than treating UTF-8 continuation bytes as glyphs. */
typedef struct {
    uint32_t battery_valid,battery_percent,charging,notifications_valid,count;
    uint32_t header_key,reply_key,reply_count,reply_revision,connection_epoch;
    PhoneNotification notifications[PHONE_NOTIFICATION_CAPACITY];
} PhoneStatus;
typedef struct {
    uint32_t valid,playing,position_ms,duration_ms,art_valid,visual_key;
    char title[64],artist[48];
    uint8_t art_rgb565[PHONE_ART_BYTES]; /* Little endian32x32, companion downscales. */
} PhoneMusic;
typedef struct {
    uint32_t connected,token,status_ms,music_ms,status_revision,music_revision;
    PhoneStatus status;PhoneMusic music;
} PhoneContentSlot;
typedef struct {PhoneContentSlot slots[PHONE_CONTENT_SLOTS];uint32_t token_counter;} PhoneContent;
void PhoneContent_Init(PhoneContent *cache);
/* Link transitions invalidate old payloads and issue a nonzero generation.
 * Adapters must obtain the current token; delayed previous-peer data is rejected. */
void PhoneContent_Links(PhoneContent *cache,uint32_t slot_mask);
uint32_t PhoneContent_Status(PhoneContent *cache,uint32_t slot,uint32_t token,const PhoneStatus *status,uint32_t now);
uint32_t PhoneContent_Music(PhoneContent *cache,uint32_t slot,uint32_t token,const PhoneMusic *music,uint32_t now);
#endif
