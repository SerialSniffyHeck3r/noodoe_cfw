# 0.9.73 caller identity and riding reconnection

[User manual](README.en.md) · [한국어](34-Call-Reconnect.md)

The supplemental call detector now retains caller numbers supplied by Android. With contact permission and a matching entry, it resolves the contact name. It cannot identify a caller whose number Android does not provide. The previous caller is cleared when the call ends or a new incoming call begins.

A screen-transfer position or session error now closes the old Bluetooth connection and resynchronizes current state on a fresh connection. Retries wait 2 seconds, then 5 seconds, then up to 10 seconds; an absent vehicle does not exhaust a retry count. Manual riding-sync stops and permission/pairing problems still require the appropriate user action.

Answer, end and dial button actions are not automatically repeated after reconnecting. Maps may briefly reload while the new connection prepares them. Failure diagnostics record numeric sent/received positions, not phone numbers, contact names or message contents.

**Upgrade:** install APK 0.9.73 over the existing app without clearing its data. Product and the matching installation ZIP are byte-identical to 0.9.71/0.9.72. Devices already running that CFW need no firmware reinstall. First installation, retained settings, the Stage 6/4 manual reset limitation and Bootstrap connection procedure remain unchanged.

Validation uses software simulations, not physical phone, Bluetooth or vehicle tests.
