# Shared right-edge button hints

`Button_Hints.h` is a renderer usable by every page. Create once on the480x480
shell, then call this owner-task API each tick with the page's bindings:

```c
ButtonHintBinding rows[3] = {
    {BSP_BUTTON_UP, up_image, tap_action_image, hold_action_image},
    {BSP_BUTTON_ENTER, circle_image, NULL, phone_image},
    {BSP_BUTTON_DOWN, down_image, next_image, NULL}
};
ButtonHints_Update(rows, page_scope, page_alpha, now_ms);
// No hints on this page:
ButtonHints_Update(NULL, 0, 0, now_ms);
```

Images are immutable24px Material Icons Round A4 descriptors. Physical key
symbols are24px atx425..448, row centers188/228/268. Action glyphs are25px
(125% of20px); markers are5px dots and14x5 capsules (rounded80% of6/18x6).
Positions and inter-element padding stay fixed. No runtime allocations occur.

ButtonEvents is still the only BSP queue consumer. Its ButtonFeedback listener
records pressed/released state; ProductUI separately executes actions. App
supplies an opaque page scope, input-generation token and enabled flag through
ButtonFeedback_SetContext. Menus/warnings/context changes invalidate held
gestures. No button pin, timing threshold, UART or BT logic lives in Graphics.

Physical symbol: white normally, blue0x42A5F5 while pressed, red0xFF5252 after
the shared2001ms long threshold. On release the physical key returns white.
Only the selected tap/hold marker and its action icon retain blue/red until
released_ms+1000ms. Unsigned subtraction handles HAL tick wrap. Duplicate
SHORT/RELEASE events cannot restart the timer; boot-held keys stay suppressed.

The displayed action symbol is captured before play/pause changes the next
available action. Scope remains the same for dynamic play/pause or Phone1/2
changes; use a different scope when changing a page's button meanings. NULL
actions have neither marker nor release pulse. Feedback indicates a gesture
handled in that input context, not a Bluetooth media-service acknowledgement.

Music supplies bindings from MusicGraphic_Hints. The common hint object is
independent of the two animated content banks, so it fades with the relevant
page without sliding into the speed ring or rendering two overlapping copies.
Only Music supplies action bindings. All other riding pages and full Settings
call `ButtonHints_ShowKeys(scope, alpha, now)` for UP/O/DOWN alone. Dots,
capsules and function icons are absent outside Music.

Both variants auto-hide after5000ms without activity. A physical press or
release restarts that window; held input stays visible. The240ms cubic fade
retargets from its current alpha if a key is pressed while fading out. The
first press still executes its normal action, without a wake-only gesture.
Boot-held keys and duplicate packets do not keep hints awake. Activity is
tracked independently of whether a page action is enabled; it never removes
motion locks or grants access. ButtonFeedback diagnostics are version2.
