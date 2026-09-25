# Noodoe OTA API archive

Date: 2026-08-31 KST

## Result

The currently reachable Noodoe OTA API exposed 9 unique firmware binaries and
10 matching resource archives. All 19 files were downloaded and SHA-256 verified.
The archive is 55,929,090 bytes.

The preserved API surface is the one used by official app version 2.1.14:

- v3 for Sunray 1.x devices
- v4 for Sunray 2.0 devices
- official request channels `stable`, `preview`, and `beta`

The server may return a lower channel than requested. In particular, a `beta`
request can return an `internal`, `preview`, or `stable` record. This makes beta
the broadest discovery request, but every discovered pair was subsequently
queried with all three official request channels.

## Preserved firmware

| API family | Series | Server/product label | Version | Returned channel | Size |
| --- | ---: | --- | --- | --- | ---: |
| v3 | 1 | SR1.5 | 5.16 | stable | 458748 |
| v3 | 2 | E-Bike | 5.16 | stable | 458748 |
| v3 | 3 | SR1 | 1.09 | stable | 458748 |
| v3 | 3 | NewAK | 2.07 | internal | 458748 |
| v4 | 1 | SR2.0 Common | 0.49 | preview | 917480 |
| v4 | 1 | SR2.0 Common | 0.53 | internal | 917480 |
| v4 | 4 | SR2.0 CV3 | 1.24 | preview | 917480 |
| v4 | 4 | SR2.0 CV3 | 1.28 | internal | 917480 |
| v4 | 5 | SR2.0 CBK | 0.50 | internal | 917480 |

Names such as `NewAK`, `CV3`, and `CBK` come from the server filenames. They are
not yet a verified mapping to retail motorcycle model names or model years.

## Preserved resources

- SR1.5 common Pack A 5.14
- SR1.5 common Pack B 5.14
- E-Bike Pack A 5.14
- E-Bike Pack B 5.14
- SR1 resources 3.01 and 3.02
- SR2.0 Common/Nars resources 0.31 and 0.32
- SR2.0 CV3/Nars resource 0.45
- SR2.0 CBK/Nars resource 0.46

## Discovered compatibility pairs

| API | Firmware series | Resource series |
| --- | ---: | ---: |
| v3 | 1 | 1 |
| v3 | 1 | 4 |
| v3 | 2 | 2 |
| v3 | 2 | 5 |
| v3 | 3 | 3 |
| v4 | 1 | 1 |
| v4 | 4 | 4 |
| v4 | 5 | 5 |

The v3 series 1 and 2 firmware images are shared by two resource packs. The
resource series must therefore be preserved as part of the hardware/language
compatibility tuple even when the firmware bytes are identical.

## Enumeration coverage

The API has no list operation. The archive used the following auditable search:

1. Every firmware/resource series pair in the dense square `0..31` was queried
   against both v3 and v4.
2. Every discovered resource series was paired with firmware IDs `32..255`.
3. Every discovered firmware series was paired with resource IDs `32..255`.
4. Every discovered pair was queried with stable, preview, and beta.
5. For every pair/channel, the reported current firmware was swept from `0.00`
   through `6.99`.
6. For every v4 pair/channel, every two-letter lowercase locale from `aa` through
   `zz` was queried.

The query log contains 28,051 firmware responses and 40 repeated resource
responses with zero transport or server errors. Repeated resource queries came
from resumable archive passes and produced the same URLs.

Changing the current version, including current version `0.75`, did not expose
any additional firmware URL. It only changed whether the server reported an
update. Sweeping all two-letter locale values also exposed no regional binary;
the returned v4 records identify their locale as `global`.

## Remaining limits

This is an exhaustive archive of the records exposed by the official app API
within the documented search boundary, not a database dump.

- A compatibility pair whose firmware and resource IDs are both above 31 could
  remain undiscovered if neither ID appears in a discovered low-ID pair.
- IDs above 255 were not queried. The meter fields and observed IDs make the
  one-byte range the defensible first boundary, but the HTTP fields are integers.
- Current versions above 6.99 and non-official channel strings were not swept.
- Deleted or superseded CDN objects that no longer have a reachable API record
  cannot be discovered from these endpoints without a known URL.
- A server filename is not proof that a binary belongs to a particular retail
  motorcycle. Never flash by filename or apparent model name alone.

## Archive layout

- `artifacts/ota-archive/2026-08-31-full/index.json`: full metadata and sightings
- `artifacts/ota-archive/2026-08-31-full/manifest.csv`: compact artifact table
- `artifacts/ota-archive/2026-08-31-full/SHA256SUMS.txt`: verification manifest
- `artifacts/ota-archive/2026-08-31-full/queries.jsonl`: raw request/response log
- `artifacts/ota-archive/2026-08-31-full/blobs/firmware`: firmware binaries
- `artifacts/ota-archive/2026-08-31-full/blobs/resource`: resource archives

## Reproduction

```powershell
$py = '<local-user>\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $py .\tools\archive_noodoe_ota.py `
  --output .\artifacts\ota-archive\2026-08-31-full `
  --seed-max 31 --max-id 255 --workers 12 --download `
  --sweep-major-max 6 --sweep-minor-max 99 --sweep-locales
```

The tool is resumable. Successful query keys in `queries.jsonl` are skipped on
later runs, existing blobs are hashed rather than downloaded again, and widening
the bounds appends new evidence.
