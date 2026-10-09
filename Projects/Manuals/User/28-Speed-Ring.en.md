# 0.9.66 · Balanced speed ring colours

The speed ring now uses sky blue, soft green, warm yellow and coral red, preserving the vivid UI character without primary red/yellow. At the normal 200 km/h full scale, 80 km/h is green.

| Nominal speed band at 200 km/h full scale | Colour |
|---|---|
| 0–60 km/h | Sky blue |
| 60–110 km/h | Soft green |
| 110–150 km/h | Warm yellow |
| Above 150 km/h | Coral red |

The boundaries are 30%, 55% and 75% of the configured full scale. For example, a 100 km/h scale uses 30/55/75 km/h, and a 300 km/h scale uses 90/165/225 km/h. These are display colour bands, not a judgement of safe speed or a speed-limit warning. Changing km/mile text does not change the physical speed thresholds.

Each boundary has a smooth transition across ±2.5% of full scale: at 200 km/h, 55–65, 105–115 and 145–155 km/h. Colours remain steady between those transitions. The red plateau therefore begins at 155 km/h after the transition centred on 150.

Colours and intermediate hues share CIELCh reference lightness L*=68, with moderate chroma reduced smoothly where needed to fit sRGB without channel clipping. In the emitted 8-bit RGB samples, calculated L* stays 67.76–68.20. This reduces the old hue-dependent brightness jumps; it is not a measurement or calibration of the physical LCD. Panel gamma, viewing angle and surroundings can still affect perceived brightness.

Only the ring colour selection changes. Ring geometry, 150 ms motion interpolation, opacity, ambient brightness/backlight control and warning graphics are unchanged. IGN OFF remains neutral gray; invalid vehicle data retains its existing inactive colour. The peak marker continues to remember the displayed ring colour. The device reads a 585-byte palette using integer arithmetic; no runtime colour-space conversion or extra framebuffer is needed.

Use the matching **0.9.66 APK and ZIP**. Update the app without clearing data, then use its CFW update flow with the matching ZIP. Ordinary updates preserve settings, photos and ride/service data. First installation still admits stock **5.14 / 5.16**, without hardware/bootloader-version/model/PCBA whitelists; this does not establish electrical compatibility on every vehicle. Bootstrap manual re-pairing and Gate/Bootstrap binaries are unchanged.

If Stage 6/4 requests a manual reset, hold **UP + O together for 3 seconds**, release and let the phone verify the result. The underlying handoff limitation is unchanged. Follow the installation and recovery manual for an unresolved result.

Validation: actual ARM speed/startup models, all 10,001 normalized colour samples, Debug/Release builds, matching APK/Product hash and the Android ZIP importer. No physical LCD or vehicle test was performed.

![sRGB reference palette](../images/speed-ring-0.9.66.png)
