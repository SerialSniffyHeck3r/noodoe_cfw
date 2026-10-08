# 0.9.65 · Latest messages and sharing replies

Rapid messages are no longer discarded while Noodoe is receiving a text image. The phone retains the latest message and merges superseded pending revisions. Android conversation-style notifications use their last message. A completed old transfer cannot overwrite a newer revision.

On the Smartphone summary, press DOWN for the newest notification. The first notification page follows new arrivals. Older pages and an open reply selection stay on their selected conversation. When the phone has announced a new message but its image is not yet ready, the page shows **Updating...** instead of presenting old text as current. Radio transfer and rendering still take time; disconnected or permission-blocked notifications cannot arrive instantly. Intermediate burst messages may be omitted from the nine completed cards. The separate unread count is retained. Enabling notification access does not import the existing Android tray.

Content delivery and alert triggers are separate. Arrivals within the popup's ten-second alert window do not retrigger it. New content can replace HOME text within its existing twenty-second window without restarting that window or cancelling a manual skip. The top phone-icon pulse still lasts two seconds without extension. Popup filtering does not discard message content.

## Quick replies

Open a reply-capable notification, hold **O**, select with UP/DOWN and tap **O** to send. Hold O or select Back to cancel. The existing speed/ignition guards apply. Android must provide a direct-reply action for that particular notification.

| Slot | Action |
|---|---|
| 1–8 | Your saved text, in fixed slots. Empty slots do not send anything. |
| 9 | Current latitude/longitude and a clickable Google Maps link. |
| 10 | Text saying which artist and song you are currently listening to. |

Location and track data are read when you confirm sending, not when the menu was rendered. Location requires a real fix no older than five seconds, accuracy within thirty metres and valid coordinates. Synthetic GPS test locations are never shared. No fresh location means no message is sent. The map link opens Google Maps where supported or its browser map ([official Maps URLs](https://developers.google.com/maps/documentation/urls/get-started)).

Track sharing requires a currently playing Android MediaSession with both artist and title. Paused/stopped playback or missing information sends nothing. Your configured signature is appended to all valid replies. The old saved text in slots 9–10 is preserved in app preferences but is no longer used by the menu. A changed, removed or revoked notification cannot silently send to a different recipient. “Reply handed to app” confirms only Android acceptance, not recipient delivery; unavailable data produces “Reply not confirmed”.

Use the matching **0.9.65 APK and ZIP** and keep app data when upgrading. First installation still accepts stock **5.14 / 5.16** without HW, bootloader-version, model or PCBA whitelists; admission is not proof of all-model electrical compatibility. Bootstrap manual re-pairing and Gate/Bootstrap binaries are unchanged. If Stage 6/4 asks for reset, hold UP+O together for three seconds, release and let the phone verify the result. That underlying handoff issue is not claimed fixed.

This release was checked in software. Actual vehicle, Android messaging delivery and radio timing were not tested.
