"""Read-only migration of the retired raw NVM journal, never a write permit.

Accept only complete outer journal CRC + device/layout-bound SET1 v1/v2/v3.
The stock FAT may own these addresses: originals remain unchanged regardless
of whether a valid record is found. Unknown committed schemas abort migration.
"""
import struct
import zlib

LAYOUT = (0x7f70000, 0x7f70000, 0x7f80000, 0x7f80000, 0x7f90000, 0x7f90000, 0x8000000)


def u32(b, off):
    return struct.unpack_from('<I', b, off)[0]


def check(ok, text):
    if not ok:
        raise ValueError(text)


def migrate(raw, uid):
    candidates = []
    for slot in range(16):
        b = raw[0x7f70000+slot*4096:0x7f70000+(slot+1)*4096]
        if u32(b, 0) != 0x4e564d31 or u32(b, 4092) != 0x434d5431:
            continue
        n = u32(b, 8)
        if n > 2048 or u32(b, 16) != (~n & 0xffffffff):
            continue
        data = b[20:20+n]
        if zlib.crc32(b[4:12]+data) != u32(b, 12):
            continue
        check(n >= 64 and u32(data, 0) == 0x31544553, 'Unknown committed legacy payload')
        version = u32(data, 4)
        check(version in (1, 2, 3), 'Newer legacy schema is read-only; do not create defaults')
        expected = {1: 244, 2: 260, 3: 312}[version]
        check(n == expected and u32(data, 8) == n and u32(data, 12) == 1 and u32(data, 56) == 1, 'Legacy length/flags')
        check(u32(data, n-4) == zlib.crc32(data[:n-4]), 'Legacy settings CRC')
        check(tuple(uid) == struct.unpack_from('<3I', data, 16), 'Legacy UID differs')
        check(LAYOUT == struct.unpack_from('<7I', data, 28), 'Legacy layout differs')
        check(u32(data, 64) <= 100 and u32(data, 68) <= 1 and u32(data, 88) == 1, 'Legacy preferences/keys')
        for i in range(2):
            r = data[72+i*8:80+i*8]
            check(r[6] <= 30 and r[7] <= 1 and (not r[7] or r[:6] not in (b'\0'*6, b'\xff'*6)), 'Legacy binding')
        for i in range(6):
            check(data[118+i*24] <= 8 and data[119+i*24] <= 1, 'Legacy link key')
        name = b''
        if version == 3:
            name, nul, tail = data[256:305].partition(b'\0')
            check(nul and len(name) <= 48 and not any(tail+data[305:308]), 'Legacy rider tail')
            decoded = name.decode('utf-8', 'strict')
            check(all(ord(c) >= 32 and not 127 <= ord(c) <= 159 and ord(c) not in (0x2028, 0x2029) for c in decoded), 'Legacy rider control character')
        usage, known = 0, 0
        if version >= 2:
            usage = struct.unpack_from('<Q', data, 240)[0]; known = u32(data, 248)
            check(known <= 1 and (known or usage == 0) and u32(data, 252) == 0, 'Legacy lifetime usage')
        fields = {0x1002: data[64:68], 0x1011: data[68:72], 0x0201: name,
                  0x0202: struct.pack('<2I', 1, u32(data, 92))+data[96:240], 0x0203: data[72:80]}
        # Preserve the retired GPS binding as an opaque reserved field. It is
        # never reinterpreted as a phone or external GPS connection request.
        fields[0x0204] = data[80:88]
        payload = struct.pack('<2I', 0x31474643, 1)+b''.join(struct.pack('<HH', k, len(v))+v for k, v in sorted(fields.items()))
        ride = bytearray(304); struct.pack_into('<2I', ride, 0, 0x31444952, 1)
        struct.pack_into('<Q', ride, 256, usage); struct.pack_into('<I', ride, 296, known)
        candidates.append((u32(b, 4), slot, version, payload, bytes(ride)))
    if not candidates:
        return b'', b'', dict(source='none', imported=False)
    best = candidates[0]
    for item in candidates[1:]:
        diff = (item[0]-best[0]) & 0xffffffff
        if 0 < diff < 0x80000000:
            best = item
    return best[3], best[4], dict(source='retired-raw-journal', imported=True, sequence=best[0], sector=best[1], version=best[2])
