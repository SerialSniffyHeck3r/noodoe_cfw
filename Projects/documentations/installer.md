# Installing, updating, and getting back to stock

The rule behind this installer is simple: **even if Product or Bluetooth dies, the rider needs a way back**. [한국어](installer.ko.md)

I'm serious about this. Around 25 September 2026, Samsung had a spectacular refrigerator-update fiasco in Korea: firmware went wrong and people's food spoiled, right around Chuseok. The joke writes itself, but the failure really isn't funny. An AK550 has no friendly user-facing hardware programming port. Firmware work normally happens over Bluetooth, and Bluetooth needs both a functioning chip and a functioning stack. What if my next build kills either one? I was not going to make “remove the front fairing, battery and cluster, then find SWD” the standard recovery procedure.

I thought about it while falling asleep, doing delivery shifts and eating dinner alone. Eventually this architecture emerged. Allow me one small moment of self-congratulation: genius. Okay, back to work.

## First install: stock to CFW

1. The Android wizard identifies the selected Noodoe and package, then sends a small Bootstrap through the **stock APP's update path**. The phone does not write arbitrary MCU addresses or replace the resident bootloader.
2. Bootstrap checks the board/BL identity and FAT allocation: free clusters, both FAT copies, stock update reserve and ownership of existing files. Before a modifying command, the app journals the session, package, device and recovery evidence locally.
3. Within verified ranges it prepares CFW-owned files and the exact stock APP recovery copy. Product is received once; where possible, duplicate copies are made on the device. “Wrote it” and “physically reread NOR and verified it” are different answers.
4. On device approval, the existing stock install path places Gate and Product in internal flash. A 100% transfer bar is not this step's finish line.
5. Product's first boot is a **trial**. Required tasks must progress normally for 30 seconds, services must come up, and the app must confirm the same device UID and candidate version before the new build becomes permanent. Any previous working CFW stays protected until then.

The stock bootloader's own first-install behavior cannot be retroactively made atomic by a Gate that hasn't been installed yet. So “Bluetooth connected,” “file received,” “new screen visible” and “permanently confirmed” appear as separate events on phone and device. This distinction may look fussy, but a lying green checkmark is worse.

For step-by-step screens, use the [installation manual](../Manuals/Installation/08-First-Install.md).

## Updating an existing CFW

With unchanged Gate and assets, the routine update sends just the **384 KiB Product APP** into the inactive external NOR A/B file. No need to rebuild FAT, retransmit stock recovery or send the same image three times. Before registering a candidate in the journal, the receiver checks target and UID, header/vector, required assets, length/hash and physical NOR reread. Gate copies the verified candidate to internal **0x08020000–0x0807FFFF** and checks that flash too.

After a link interruption, the app asks the device which sectors and offsets were verified, compares the package hash, then resumes. If a COMMIT or RESET response vanishes, it reads status before doing anything again. A failed health or phone confirmation can return to the previous working Product if one exists. **CFW rollback** and **stock restore** are separate outcomes. Changes to Gate, Bootstrap or asset-file format use a separate migration path. On the first ever CFW install there is, of course, no older working CFW to roll back to; pretending otherwise would be a lousy trick.

## Stock recovery with buttons

I guarded that 64 KiB Gate sector throughout Product development. Product could use the space, sure, but I would rather keep my fairing attached. Gate has no dependency on Product, Bluetooth, SDRAM, LVGL or external fonts. Its EVE ROM text and primitives draw a recovery screen, and it runs a watchdog. The intentional entry gesture is **key OFF → hold center O → turn key ON while holding → keep holding for the specified time**. Entering Gate doesn't itself erase Product or approve stock installation; the rider confirms that on Gate's screen.

Gate compares `CFWREC.DAT` with the stock version, board identity and preserved boot-code details. It stages the verified stock APP in the original updater area, physically rereads it and hands off to the stock installer. This **keeps CFW data** in NOR. Original stock photos and files stay put; CFW's added files stay too. Actually erasing their contents and returning FAT space belongs to a separate uninstall operation.

If CFW and radio both take a spectacular dive, the Gate/buttons/MCU/NOR/stock installer and recovery image can still bring stock back without ST-LINK. And yes, SWD remains the ultimate tool if the board itself is damaged. I'd still rather not remove the bike's bodywork. The resident stock bootloader is deliberately left alone; both install and recovery rely on it.

## What remains intact

Factory MAC, PIN and vehicle identity are not user preferences. Product updates and ordinary stock restore leave the lower boot/factory region alone. Settings, trips, three CFW photo slots, logs, assets and A/B images live in separately owned FAT files. After restoring the stock APP, seeing the original photos is expected. It does **not** mean every external NOR byte has returned to factory-shipping condition. Pairing keys may have changed while CFW ran, so the phone may need pairing again.

The [installation screens](../Manuals/Installation/README.md), [NDCP notes](technical-notes/ndcp-protocol.md) and [install-session contract](technical-notes/install-session-v2.md) have the operational details.
