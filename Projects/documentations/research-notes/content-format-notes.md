# Noodoe content format notes

Date: 2026-08-30 KST

## Scope

These notes track how the public Noodoe Android app prepares files before
installing them to the meter. This is separate from the Bluetooth frame
protocol.

Evidence level in this file: static analysis of the public app decompile,
unless otherwise stated.

## Big picture

The test app revealed many hardware-control and protocol constants. The public
app reveals a different layer: how user-visible content becomes installable
meter files.

Content should be treated as two families:

1. Gallery images: six independent image slots.
2. Creations: bundled dashboard/clock/weather/speedometer/POI/group/audio
   content with a configuration file and generated image/resources.

## Gallery image format

Current static conclusion:

- Gallery has six slots.
- The gallery image directory is app-private `files/gallery`.
- Gallery file extension is `.jpg`.
- User image selection opens the crop UI with `MaskType.FULL`, base side `480`,
  and output folder set to the gallery directory.
- The crop UI initially writes a PNG file with a UUID filename.
- `DashboardManager.installGalleryImage(...)` loads the selected/cropped image,
  converts it to `480x480` `RGB_565`, and writes it as JPEG.
- The JPEG write path uses the app `ImageUtils.bitmapToFile(...)` helper, whose
  default quality is `80`.
- After writing the JPEG, the app appends the gallery slot index as one byte at
  end of file.
- It then computes MD5 of that final file and renames it to:

```text
<md5>.jpg
```

The stored gallery path is saved in app settings. During sync, the transmitter
loads up to six saved paths from app settings and sends existing files only.

Important consequence:

The meter does not receive the original phone photo. It receives a transformed
file: cropped/full-mask, 480x480, RGB_565 source bitmap, JPEG quality 80, with
one trailing slot-index byte, named by MD5.

## Gallery transfer identity

`GalleryTransmitModel` calculates a gallery content ID by MD5-hashing the file
names of all selected gallery files in order.

For each file sent:

- File ID is the 1-based gallery slot position.
- MD5 is `file.getName().substring(0, 32)`.
- Target file path is the full file name.

The install starts with:

```text
startInstallCreationTask(GALLERY, total_gallery_file_bytes, gallery_id)
```

If no gallery files are configured, the app sends a delete/remove task for the
gallery location.

## Creation bundle format

Current static conclusion:

- Clock, weather, speedometer, POI, group, and audio use `CreationTransmitModel`.
- The app asks `DashboardManager.getInstalledCreationBundlePath(type)` for the
  installed bundle directory.
- If the directory exists, every file in it is added to a `CreationBundle`.
- The `.cfg` file is specially mapped as `CreationBundle.CONFIGURATION_CFG`.
- Non-`.cfg` files are mapped by base filename.
- Total transfer size is the sum of all files in the bundle.
- The content ID used for install is the first 32 characters of the `.cfg`
  filename.

The transmitter then sends each file in that bundle directory. For each file:

- File ID is the send order, 1-based.
- Command ID is also set to that file ID.
- MD5 is the first 32 characters of the file name.
- Target file path is the full file name.

Important consequence:

For these creations, we need the generated bundle directory, not just the final
SPP packets. Reimplementation requires reproducing both the `.cfg` schema and
the asset generation rules, or at least replaying known-good generated bundle
files.

## Creation generation

`DashboardManager.generateCreationBundle(...)` does this:

1. Loads a source creation archive/file.
2. Unarchives it into a `SunrayCreation`.
3. Deletes/recreates the target `<creation>.bundle` directory.
4. Runs `CreationBundler(...).bundle()`.

`CreationBundler` then:

- Flattens background widgets.
- Converts compound background PNG-like bitmaps to JPEG where needed.
- Writes generated image files.
- Generates a `.cfg` configuration file.
- Renames generated files by MD5.
- Builds a `CreationBundle` containing widgets and a file map.

The meter install path is therefore:

```text
creation archive/cloud object
    -> SunrayCreation model
    -> generated bundle directory
    -> .cfg plus image/resource files named by MD5
    -> FILE_TRANSFER_NEGOTIATE / FILE_TRANSFER_CONTROL / FILE_TRANSFER
```

## File-transfer headers

Install negotiation serializes:

```text
[task_id:1]
[transfer_type:1]
[location_id:2 little-endian]
[total_size:4 little-endian for FILE, or 4 zero bytes for DATA]
[transfer_attribute:1]
[content_id_or_extra:16]
```

For normal install, `content_id_or_extra` is usually the 16 raw bytes decoded
from the 32-character hex content ID.

Each file transfer control packet includes:

```text
[task_id:2 little-endian]
[operation_code:1]
[file_transfer_id:2 little-endian]
[file_md5:16 or file_id plus padding]
[crc32:4 little-endian]
[file_size:4 little-endian]
[target_path:string bytes or two zero bytes]
```

The exact string termination/length handling for `target_path` still needs
verification against live captured bytes.

## What remains unknown

- Exact `.cfg` JSON/schema fields for each creation type.
- Whether all bundle files are plain PNG/JPEG/config, or if some are opaque
  binary resources.
- Whether the meter validates only MD5/file size/CRC, or also semantic content.
- Whether gallery accepts any JPEG with the same transform, or has stricter
  JPEG encoder expectations.
- How much of cloud creation download can be bypassed by generating local
  `SunrayCreation` objects.

## Next proof steps

1. Pull actual app-private `files/gallery` from the rooted tablet after a
   gallery transfer.
2. Check whether gallery files end with the expected slot byte.
3. Compare computed MD5 of the file including that byte with the filename.
4. Pull installed `.bundle` directories for clock/weather/speedometer.
5. Inventory each bundle: file names, extensions, sizes, magic bytes, MD5.
6. Capture SPP for one gallery sync and map each transferred file to its MD5,
   file ID, target path, size, and CRC32.
7. Repeat for one speedometer or clock bundle.
