# Shared scalar animation

`Scalar_Transition.h` provides allocation-free slow-fast-slow interpolation for
opacity, dimming and other bounded scalar values. It uses the same cubic ease as
page, carousel and scene transitions. State belongs to the calling owner task;
callers must not share one instance between concurrent owners.

```c
ScalarTransition opacity = {0};
ScalarTransition_Request(&opacity, 255, 240, now_ms);
uint32_t alpha = ScalarTransition_Value(&opacity, now_ms);
```

Targets are0..1024, duration is0..60000ms and timestamps are unsigned milliseconds.
Sampling handles the32-bit timestamp wrap. Repeating the same target is a no-op
and does not extend the animation. A new target first samples the current value,
so reversals do not jump to an endpoint. Duration0 applies a changed target
immediately. No allocation, RTOS wait, GPU access or callback is hidden here.

GraphicsBackground owns separate instances for image opacity and central shade.
All selected-photo and album-art changes pass through that renderer. Photo/art bank crossfade is240ms. Same-bank large-photo replacements use
240ms fade legs with a separately bounded upload between them.
Provider failure stays black and is visible through the background diagnostics.
See App_Logic/UI/WALLPAPER.md for source policy and EVE memory ownership.

`tools/tests/wallpaper` runs the actual scalar and streaming renderer sources on
ARM atO0/Os/Oz+LTO with bothDATA_DEBUG values, including reversal, repeat requests,
clock wrap, source coalescing, swap retirement, provider busy/failure and bounds.
