#ifndef NOODOE_MAINTENANCE_COPY_H
#define NOODOE_MAINTENANCE_COPY_H
/* ROM-safe English used by our maintenance screens; original resident BL is
 * untouched. Human copy never substitutes for a code or verification result. */
#define COPY_RECOVERY "RECOVERY MODE"
#define COPY_BOOTSTRAP "FuckNudo Bootstrap"
#define COPY_WELCOME "Welcome"
/* Branding is presentation only: keep protocol identities and package IDs. */
#if NOODOE_BOOTSTRAP
#define COPY_INSTALLER_TITLE COPY_BOOTSTRAP
#else
#define COPY_INSTALLER_TITLE "NOODOE INSTALLER"
#endif
#define COPY_RESCUE "Ready for the rescue."
#define COPY_CONNECTED "Connected. Good to go."
#define COPY_WORKING "Hang tight. Getting ready."
#define COPY_WRITE_WAIT "One sec. Finishing this write."
#define COPY_UNKNOWN "Well, shit. Result not confirmed."
#define COPY_ERROR "SYSTEM ERROR"
#define COPY_FATAL "Okay, this one's bad as fuck. :("
#define COPY_POWER "Keep main power on."
#endif
