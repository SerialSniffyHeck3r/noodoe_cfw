#ifndef APP_DATA_CONFIG_H
#define APP_DATA_CONFIG_H
/* One application-wide source selector. Product defaults to live providers;
 * 1 supplies deterministic presentation placeholders without a debugger.
 * Vehicle speed/ODO/IGN/RTC and physical device/link state always stay live. */
#ifndef DATA_DEBUG
#if NOODOE_PRODUCT
#define DATA_DEBUG 0
#else
#define DATA_DEBUG 1
#endif
#endif
#if DATA_DEBUG != 0 && DATA_DEBUG != 1
#error "DATA_DEBUG must be 0 or 1"
#endif

/* Initial central page in DATA_DEBUG builds only:3 is UI_MUSIC. This is an
 * initial selection, not a forced preview; normal buttons still navigate. */
#ifndef DATA_DEBUG_START_CARD
#define DATA_DEBUG_START_CARD 0U
#endif
#if DATA_DEBUG_START_CARD > 7U
#error "DATA_DEBUG_START_CARD must be a central page index (0..7)"
#endif
#endif
