# Product companion NDCP additions (schema 1)

Common NDCP v1 framing/CRC/sequence remains unchanged. All request lengths below exclude the 4-byte result in replies. Numbers are unsigned little-endian32 unless noted. SPP authentication and a current connection are prerequisites; a reply is not proof of persistence unless specified.

A single I/O task owns parser and PhoneContent publication. Settings/NOR operations are queued to their existing owners. Installation uses the same socket exclusively. Product GPS continues to use opcode09; UART speed remains the driving speed source.

| Opcode | Request | Successful reply after result |
|---|---|---|
|70|empty|schema1, connection epoch, IGN-valid, IGN-on, speed-valid, UART km/h, power mode|
|71|catalog start index|schema1, count<=8, six words per row: stable field ID, signed value/min/max/step, kind|
|72|epoch, field ID, signed value|request ID; RAM application and persistence are separate|
|73|epoch, request ID|ID, applied flag, application result, durable result|
|74|epoch, notification ID, app/title/body byte lengths(u8 each), strings|empty; newest-first upsert, six entries maximum|
|75|epoch, notification ID (0=clear)|empty|
|76|epoch, valid/playing/reserved/reserved(u8), position ms, duration ms, title/artist lengths(u8), strings|empty|
|77|epoch, byte offset,1..512 bytes|received offset; existing32×32RGB565 art only publishes at2048 bytes|
|78|epoch, battery percent, charging flag|empty|

Every mutating content request carries the epoch returned by70. A new connection or Product phone token clears assembly state. No partial artwork is published. Strings are bounded byte fields using the existing ASCII/fallback adapter. **This is not CJK support; L4 text rendering is still outstanding.** Album art is RAM-only.

Device media requests use opcode10 and a device-owned high-bit sequence number. Phone executes public Android media controls and replies with the same sequence. A duplicate ID returns its prior result and does not toggle twice. An older event in the current session is discarded. No arbitrary touch injection exists.

# Maintenance extensions

| Opcode | Meaning |
|---|---|
|5B|80-byte result-inclusive boot snapshot, schema2; candidate/previous/result metadata|
|5C|exact trial sequence + Product SHA32; accepted only in the current connection|
|5D|40-byte result-inclusive log status, schema1|
|5E|queue log read: token, expected journal sequence,256-byte aligned offset|
|5F|read queued token result; success includes256 bytes|
|61|acknowledge rollback transaction; acceptance is queued, poll5B until durable|
|62|request bounded-step read-only recovery/FAT preflight|
|63|preflight status; same connection ready result enables maintenance|
|64|40-byte result-inclusive Gate hash, schema2|

Product update BEGIN target2 accepts only384KiB Product images with the v2 trial contract. Gate and the lower64KiB are not routine update targets. Resume replies report physically verified4KiB boundaries; final SHA remains mandatory. Resource changes are not yet transported by the routine-update client.

CFWLOG page retrieval pins a sequence logically: any change rejects the page so the caller cannot present a mixed generation as one verified log. Diagnostic read failure must be reported, not replaced with empty successful logs.

The original test status above describes the initial schema1 implementation.
Current package compatibility and validation are recorded per release under
Reversing/analysis; this historical document is not a deployment gate.

##6.9.8 music transport

Capabilities0x4000 negotiates binary kind9, lossless RLE over the existing
kind7 music tile. The16-byte identity header stays plain; exactly11,168 pixel
bytes follow after decompression. Control bit7 denotes a repeated next byte,
otherwise literal bytes; length is(control&127)+1. CRC covers the complete
transmitted representation. The storage worker validates CRC/length/epoch/view
before publishing, reusing bounded scratch. Raw kind7 remains supported.
See NoodoeInstaller/PROTOCOL.md for the matching phone contract.
