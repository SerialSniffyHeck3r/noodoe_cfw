# Official Noodoe preset archive, 2026-09-06

## Current server result

The authenticated production Firebase catalog returned 69 `presets` entries.
Sixty-five still resolve to Creation metadata and were archived without a
download error:

- 14 analog clock themes
- 9 digital clock themes
- 9 analog speedometer themes
- 9 digital speedometer themes
- 16 weather themes
- 8 Around Me themes

The four unresolved entries are `_@PRESET_GROUP_01` through
`_@PRESET_GROUP_04`. They remain in the catalog but their Creation records are
absent. The much larger `publicCreationList` contained 186,738 entries at the
same observation time; it is user-published content and is not being described
as the official preset set.

The preserved source archive is under:

`preservation/official-themes/sunray-production/2026-09-06-presets`

Each resolved Creation directory includes the server metadata, original
`appData` ZIP, preview, thumbnail, extracted editable source, and hashes. The
archive contains no copied authentication token.

## Source ZIP is not the dashboard ZIP

The CDN `appData` object is the official editable source. The official Android
app unarchives it into a `SunrayCreation`, runs `CreationBundler`, rasterizes and
flattens widgets, renames payloads to their content MD5, and emits one JSON cfg.
Only that generated cfg and payload set is suitable for OpenNoodoe file import.

The exact offline conversion path is prepared in
`tools/frida/noodoe-official-batch-bundler.js`. It runs inside the preserved
official app process on the rooted tablet, reads staged ZIPs from
`/data/local/tmp/opennoodoe-presets`, and writes only to the separate private
`files/opennoodoe-bundles` directory. It does not replace the official app's
`files/installed` state.

After conversion, each generated directory must pass the same cfg/files/MD5
validator used by OpenNoodoe before it is packaged as an individually
downloadable ZIP. A vehicle transfer is still required to validate renderer
acceptance for each widget family.
