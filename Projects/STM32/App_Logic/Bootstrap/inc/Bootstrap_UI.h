#ifndef BOOTSTRAP_UI_H
#define BOOTSTRAP_UI_H
#include <stdint.h>
/* Stable maintenance-view ABI. Runtime facts, not UI wishes, own progress. */
enum { BOOT_UI_CHECK, BOOT_UI_READY, BOOT_UI_CONNECT, BOOT_UI_WORK,
       BOOT_UI_INSTALL_READY, BOOT_UI_INSTALL, BOOT_UI_PAUSED,
       BOOT_UI_RECOVERY, BOOT_UI_ERROR, BOOT_UI_BT_TEST, BOOT_UI_HELP, BOOT_UI_AMBIENT };
enum { BOOT_ACTION_NONE, BOOT_ACTION_PAIR, BOOT_ACTION_RECOVERY,
       BOOT_ACTION_ALLOW_INSTALL, BOOT_ACTION_CANCEL, BOOT_ACTION_BT_TEST };
/* Stable source+detail codes: storage IO=4 must never mean display failure. */
enum { BOOT_ERR_BT=0x10000,BOOT_ERR_NOR=0x20000,BOOT_ERR_RAM=0x30000,
       BOOT_ERR_DISPLAY=0x40000,BOOT_ERR_STORAGE=0x50000,BOOT_ERR_UPDATE=0x60000,
       BOOT_ERR_RECOVERY=0x70000 };
typedef struct {
 uint32_t state,selection,permission_until,install_allowed,error;
 uint32_t phase,position,total,connected,busy,can_cancel;
 /* Copied live facts only. A ready controller is not proof of SPP traffic. */
 uint32_t bt_state,bt_secure,requests,replies,bt_error;
 uint32_t recovery_hold_ms;
 uint32_t now,mark,tracked,old_phase,old_position,old_subphase,old_kind;
 uint32_t subphase,kind,stalled;
 /* UI lifetime is not the lifetime of one storage request. */
 uint32_t checking,install_session,work_started,checked_files;
 uint32_t commit_phase,commit_result,help_page,cancel_pending,progress[20];
 /* One boot-only presentation overlay; never an installation state or lease. */
 uint32_t welcome, welcome_started;
 /* Diagnostic facts; invalid samples are never presented as zero lux. */
 uint32_t ambient[11],radio[5];
} BootstrapUI;
typedef struct { const char *title,*line1,*line2,*hint; uint32_t percent,error;
 const char *line3; char detail[64]; const char *notice;
 uint32_t overall,has_overall;char counts[80]; uint32_t welcome; char extra[3][64]; } BootstrapView;
void BootstrapUI_ShowWelcome(BootstrapUI *,uint32_t now);
void BootstrapUI_Track(BootstrapUI *,uint32_t now,uint32_t subphase,uint32_t kind);
void BootstrapUI_Context(BootstrapUI *,uint32_t checking,uint32_t checked_files,uint32_t install_session);
void BootstrapUI_WorkStarted(BootstrapUI *);
void BootstrapUI_Init(BootstrapUI *s);
/* delta +/-1 navigates; select is one debounced short press, never held repeat. */
uint32_t BootstrapUI_Input(BootstrapUI *s,int delta,uint32_t select,uint32_t now);
void BootstrapUI_Update(BootstrapUI *s,uint32_t now,uint32_t ign,uint32_t connected,
                        uint32_t phase,uint32_t position,uint32_t total,uint32_t busy,uint32_t error);
void BootstrapUI_View(const BootstrapUI *s,BootstrapView *v);
#endif
