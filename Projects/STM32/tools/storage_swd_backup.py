"""Read128MiB NOR through a live firmware SDRAM mailbox, without USB.

Only HOTPLUG uploads and writes to the validated volatile request words are
emitted. New CFW also uses its ELF-validated storage-quiesce mailbox, draining
physical writers before A/B reads and releasing the lease in finally.
No halt/run/reset/flash/option-byte commands are supported.
This script must have exclusive debugger ownership; it does not run on import.
"""
from __future__ import annotations
import argparse
import datetime as dt
import hashlib
import importlib.util
import json
import os
import re
import struct
import subprocess
import time
import zlib
from pathlib import Path

CLI = Path(r'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe')
NM = Path(r'C:\ST\STM32CubeIDE_1.18.1\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344\tools\bin\arm-none-eabi-nm.exe')
CAPACITY, BUFFER_BYTES, MAILBOX_BYTES = 0x08000000, 0x00800000, 128
MAGIC, REQUEST_MAGIC, ARM = 0x31445753, 0x31514552, 0x52454144
UID_ADDRESS, META_ADDRESS, DHCSR = 0x1FFF7A10, 0x08008000, 0xE000EDF0
APP_BASE, APP_END = 0x08010000, 0x08080000
FIELDS = ('magic', 'abi', 'bytes', 'state', 'init_result', 'buffer_address', 'buffer_capacity',
          'nor_capacity', 'uid0', 'uid1', 'uid2', 'jedec_id', 'request_magic', 'arm',
          'request_offset', 'request_length', 'request_seq', 'active_seq', 'response_offset',
          'response_length', 'result', 'response_address', 'crc32', 'completed', 'response_seq',
          'request_result', 'last_request_seq', 'rejected_seq', 'polls', 'accepted', 'errors', 'busy_rejections')
STABLE_FIELDS = FIELDS[:12] + FIELDS[17:25]

# Share the installed-image contract with the flash precondition validator.
# Loading this local module performs no objcopy/programmer/device operation.
_image_spec = importlib.util.spec_from_file_location('storage_swd_image_validator',
                                                    Path(__file__).with_name('validate_image.py'))
_image_validator = importlib.util.module_from_spec(_image_spec)
_image_spec.loader.exec_module(_image_validator)


def require(condition: bool, message: str) -> None:
    """Use runtime checks which Python -O cannot erase."""
    if not condition:
        raise RuntimeError(message)


def decode(data: bytes) -> dict:
    """Validate fixed ABI before treating any address as a writable mailbox."""
    require(len(data) == MAILBOX_BYTES, 'Mailbox size mismatch')
    result = dict(zip(FIELDS, struct.unpack('<32I', data)))
    require(result['magic'] == MAGIC and result['abi'] == 1 and result['bytes'] == MAILBOX_BYTES,
            'Target does not expose StorageSWD ABI1')
    require(result['state'] in (0, 1, 2, 3), 'Invalid mailbox state')
    require(result['init_result'] == 0, f'StorageSWD initialization failed: {result}')
    require(result['buffer_capacity'] == BUFFER_BYTES and result['nor_capacity'] == CAPACITY,
            'Unexpected SDRAM/NOR capacity')
    require(0xC0000000 <= result['buffer_address'] <= 0xC4000000 - BUFFER_BYTES and
            result['buffer_address'] % 32 == 0, 'Buffer is outside aligned validated SDRAM range')
    require(result['jedec_id'] == 0xC2201B, 'Unexpected NOR JEDEC ID')
    return result


def stable(a: dict, b: dict) -> bool:
    """Ignore changing poll counters; response and buffer identity must stay fixed."""
    return all(a[key] == b[key] for key in STABLE_FIELDS)


def mailbox_symbol(elf: Path) -> int:
    """Read the exact linked symbol and require the128-byte main-SRAM object."""
    output = subprocess.check_output([str(NM), '-S', '--defined-only', str(elf)], text=True)
    matches = []
    for line in output.splitlines():
        fields = line.split()
        if len(fields) == 4 and fields[3] == 'g_storage_swd':
            matches.append((int(fields[0], 16), int(fields[1], 16)))
    require(len(matches) == 1 and matches[0][1] == MAILBOX_BYTES, 'Missing or incompatible g_storage_swd ELF symbol')
    address = matches[0][0]
    require(0x20000000 <= address <= 0x20030000 - MAILBOX_BYTES and address % 4 == 0,
            'Mailbox ELF symbol is outside main SRAM')
    return address


def elf_app_image(elf: Path) -> tuple[bytes, bytes, dict]:
    """Reconstruct the exact canonical APP image used by the flashing validator.

    PT_LOAD bounds remain mandatory, but its incidental file padding is not
    payload. Allocated file-backed sections map through their validated LMAs;
    gaps use0xFF exactly as validate_image/objcopy --gap-fill=0xFF do.
    """
    data = elf.read_bytes()
    try:
        image, manifest = _image_validator.validate(_image_validator.Elf32(data))
    except _image_validator.InvalidImage as error:
        raise RuntimeError(f'Invalid Noodoe APP ELF: {error}') from error
    require(0 < len(image) <= APP_END - APP_BASE, 'Canonical APP size is outside flash')
    return data, image, manifest


def save(path: Path, data: dict) -> None:
    """Flush and replace our manifest; raw capture files are never replaced."""
    temporary = path.with_suffix(path.suffix + '.tmp')
    with temporary.open('w', encoding='utf-8') as output:
        json.dump(data, output, indent=2)
        output.flush()
        os.fsync(output.fileno())
    # Windows readers (including a progress viewer) may briefly omit delete
    # sharing. Retry only that transient publication failure, never acquisition
    # or device requests. The fully flushed temporary remains the source.
    for attempt in range(41):
        try:
            temporary.replace(path)
            break
        except PermissionError:
            if attempt == 40:
                raise
            time.sleep(0.05)


class LiveMemory:
    """Single debugger owner with a hard whitelist of read/request operations."""
    def __init__(self, serial: str, directory: Path, mailbox: int, read_khz: int,
                 timeout: float, cli: Path = CLI):
        require(0x20000000 <= mailbox <= 0x20030000 - MAILBOX_BYTES and mailbox % 4 == 0,
                'Mailbox must be aligned main SRAM')
        require(1 <= read_khz <= 24000 and timeout > 0, 'Invalid SWD rate/timeout')
        self.serial, self.directory, self.mailbox = serial, directory, mailbox
        self.read_khz, self.timeout, self.cli = read_khz, timeout, cli
        self.counter = 0
        self.validated = False
        self.app_verified = False
        self.quiesce_address = None
        self.quiesce_owned = False

    def _command(self, name: str, operations: list[str], read: bool) -> str:
        """Only private upload/request methods construct operations; never accept raw CLI text."""
        self.counter += 1
        log = self.directory / f'{self.counter:05d}-{name}.log'
        command = [str(self.cli), '-c', 'port=SWD', f'sn={self.serial}',
                   f'freq={self.read_khz if read else 50}', 'mode=HOTPLUG', *operations]
        with log.open('x', encoding='utf-8') as output:
            output.write(json.dumps(command) + '\n');output.flush()
            result = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT,
                                    timeout=self.timeout, check=False)
        text = log.read_text(encoding='utf-8', errors='replace')
        require(result.returncode == 0 and not re.search(r'(?im)^\s*(?:Error\s*:|No STM32 target)', text),
                f'Programmer command failed; see {log}')
        return str(log)

    def upload(self, name: str, address: int, length: int) -> Path:
        """Upload APP identity/mailbox/UID/DHCSR or SDRAM only; never lower FLASH code."""
        intervals = ((self.mailbox, self.mailbox + MAILBOX_BYTES),
                     (UID_ADDRESS, UID_ADDRESS + 12), (META_ADDRESS, META_ADDRESS + 20),
                     (DHCSR, DHCSR + 4), (APP_BASE, APP_END), (0xC0000000, 0xC4000000))
        if self.quiesce_address is not None:
            intervals += ((self.quiesce_address, self.quiesce_address + 12),)
        require(length > 0 and length <= BUFFER_BYTES and
                any(start <= address and address + length <= end for start, end in intervals),
                'Read is outside the mailbox/identity/SDRAM whitelist')
        destination = self.directory / f'{self.counter + 1:05d}-{name}.bin'
        require(not destination.exists(), 'Refusing to reuse an existing upload')
        self._command(name, ['-u', hex(address), hex(length), str(destination)], True)
        require(destination.exists() and destination.stat().st_size == length, 'Incomplete live memory upload')
        return destination

    def running(self) -> None:
        """Observe S_HALT; never repair a halted target by implicitly issuing RUN."""
        data = self.upload('dhcsr', DHCSR, 4).read_bytes()
        require(not struct.unpack('<I', data)[0] & (1 << 17),
                'CPU is halted; stop and investigate debugger behavior without implicit resume')

    def quiesce(self, elf: Path, enable: bool, adopt_existing: bool = False) -> bool:
        """Drain normal CFW writers; only the exact verified ELF RAM flag is writable."""
        require(self.app_verified, 'APP must be verified before maintenance request')
        listing = subprocess.check_output([str(NM), '-S', '--defined-only', str(elf)], text=True)
        symbols = [p for line in listing.splitlines() if len(p := line.split()) == 4 and p[3] == 'g_cfw_quiesce']
        if not symbols:
            return False  # Previous firmware has no periodic CFW writer.
        require(len(symbols) == 1 and int(symbols[0][1], 16) == 12, 'Quiesce ABI mismatch')
        address = int(symbols[0][0], 16)
        require(0x20000000 <= address <= 0x20030000 - 12 and not address % 4, 'Invalid quiesce RAM symbol')
        self.quiesce_address = address
        def state():
            words = struct.unpack('<3I', self.upload('quiesce-state', address, 12).read_bytes())
            require(words[0] == 0x53554150 and words[1] <= 1 and words[2] <= 1, 'Quiesce RAM corrupt')
            return words
        before = state()
        if enable:
            if adopt_existing:
                require(before[1:] == (1, 1), 'Previous drained quiesce lease is no longer held')
                self.quiesce_owned = True
                return True  # Explicit resumer owns release; no new target write.
            require(before[1:] == (0, 0), 'Another maintenance lease is already active')
            self.quiesce_owned = True  # Even an uncertain command must be unwound.
        self._command('quiesce-request', ['-w32', hex(address + 4), str(int(enable))], False)
        deadline = time.monotonic() + 120
        while state()[2] != int(enable):
            require(time.monotonic() < deadline, 'Storage did not quiesce/resume; no snapshot accepted')
            time.sleep(.2)
        if not enable:
            self.quiesce_owned = False
        return True

    def descriptor(self) -> dict:
        """Read all128bytes once; callers double-read the stable response fields."""
        result = decode(self.upload('mailbox', self.mailbox, MAILBOX_BYTES).read_bytes())
        self.validated = True
        return result

    def verify_app(self, elf: Path) -> dict:
        """Bind the ELF-derived mailbox address to matching live APP bytes before writes."""
        self.app_verified = False
        data, expected, manifest = elf_app_image(elf)
        symbol = manifest['symbols'].get('g_storage_swd')
        require(symbol and symbol['size'] == MAILBOX_BYTES and int(symbol['address'], 0) == self.mailbox,
                'Canonical ELF mailbox symbol does not match the selected main-SRAM address')
        length = len(expected)
        image_base = int(manifest['flash_address'], 0)
        require((manifest.get('layout_version', 1), image_base) in
                ((1, APP_BASE), (2, 0x08020000)) and image_base + length <= APP_END,
                'Unsupported installed APP layout')
        self.running()
        path = self.upload('running-app-identity', image_base, length)
        actual = path.read_bytes()
        self.running()
        require(actual == expected, 'Running APP differs from canonical ELF image; SRAM writes denied')
        self.app_verified = True
        return dict(elf=str(elf.resolve()), elf_sha256=hashlib.sha256(data).hexdigest(),
                    live_read_path=str(path), live_span_address=image_base, live_span_bytes=length,
                    live_span_sha256=hashlib.sha256(actual).hexdigest(),
                    compared_bytes=length, canonical_image_match=True, gap_fill='0xFF',
                    canonical_sha256=manifest['binary_sha256'],
                    validator='validate_image.Elf32/validate')

    def request(self, offset: int, length: int, sequence: int) -> None:
        """Invalidate seq, write four immutable request fields, then commit seq last."""
        require(self.validated, 'Read and validate target mailbox before any SRAM request write')
        require(self.app_verified, 'Verify running APP against the selected ELF before any SRAM request write')
        require(0 < sequence <= 0xFFFFFFFF and 0 <= offset < CAPACITY and
                0 < length <= BUFFER_BYTES and length <= CAPACITY - offset, 'Invalid NOR request')
        box = self.descriptor()
        require(box['state'] != 1 and sequence != box['last_request_seq'],
                'Mailbox is busy or the request sequence was already consumed')
        self.running()
        self._command('invalidate-request', ['-w32', hex(self.mailbox + 64), '0'], False)
        self._command('request-fields', ['-w32', hex(self.mailbox + 48), hex(REQUEST_MAGIC),
                                       hex(ARM), hex(offset), hex(length)], False)
        self._command('commit-request', ['-w32', hex(self.mailbox + 64), hex(sequence)], False)
        self.running()


def identity(memory: LiveMemory) -> dict:
    """Bind the firmware mailbox to direct MCU UID and current stock metadata."""
    memory.running()
    box = memory.descriptor()
    uid = list(struct.unpack('<3I', memory.upload('uid', UID_ADDRESS, 12).read_bytes()))
    require(uid == [box['uid0'], box['uid1'], box['uid2']], 'Mailbox/direct MCU UID mismatch')
    metadata = list(struct.unpack('<5I', memory.upload('metadata', META_ADDRESS, 20).read_bytes()))
    require(metadata[0] == 0x000E0000 and metadata[4] == 0,
            'Unexpected resident/pending install; cannot certify a quiescent backup')
    return dict(usb_serial=''.join(f'{word:08X}' for word in uid), uid_words=uid,
                boot_metadata_words=metadata, jedec_id=box['jedec_id'], capacity_bytes=box['nor_capacity'])


def wait_ready(memory: LiveMemory, sequence: int, offset: int, length: int, timeout: float) -> dict:
    """Require a matching committed response; BUSY/progress is never treated as data."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        first = memory.descriptor()
        if first['rejected_seq'] == sequence:
            raise RuntimeError(f'Request rejected: {first}')
        if first['response_seq'] == sequence:
            second = memory.descriptor()
            if not stable(first, second):
                continue
            require(first['state'] == 2 and first['result'] == 0, f'NOR read failed: {first}')
            require(first['active_seq'] == sequence and first['response_offset'] == offset and
                    first['response_length'] == length and first['completed'] == length and
                    first['response_address'] == first['buffer_address'], 'Response identity/range mismatch')
            return first
        time.sleep(1.0)
    raise TimeoutError('Firmware mailbox did not publish a complete response; no reset/retry request sent')


def read_segment(memory: LiveMemory, offset: int, length: int, sequence: int,
                 ready_timeout: float, attempts: int) -> tuple[Path, dict]:
    """On SWD corruption retry the SAME immutable buffer, never a fresh NOR fill."""
    memory.request(offset, length, sequence)
    response = wait_ready(memory, sequence, offset, length, ready_timeout)
    reads = []
    for attempt in range(1, attempts + 1):
        before = memory.descriptor()
        require(stable(response, before), 'Response changed before SDRAM download')
        path = memory.upload(f'segment-{offset:08x}-try{attempt}', response['response_address'], length)
        after = memory.descriptor()
        memory.running()
        require(stable(response, after), 'Buffer changed during download; capture is invalid')
        data = path.read_bytes()
        crc = zlib.crc32(data)
        reads.append(dict(path=str(path), crc32=crc, match=crc == response['crc32']))
        if crc == response['crc32']:
            return path, dict(offset=offset, length=length, sequence=sequence,
                              crc32=crc, sha256=hashlib.sha256(data).hexdigest(), reads=reads,
                              committed_response=response)
    raise RuntimeError(f'Live SWD data failed all same-buffer CRC attempts: {reads}')


def capture(memory: LiveMemory, destination: Path, segment_bytes: int, ready_timeout: float,
            attempts: int, start_sequence: int, progress) -> tuple[dict, int]:
    """Create one full raw NOR file from independently validated SDRAM segments."""
    require(not destination.exists(), 'Never overwrite a raw capture')
    partial = destination.with_suffix('.partial')
    require(not partial.exists(), 'Never append an unverified old partial capture')
    digest = hashlib.sha256();segments = [];sequence = start_sequence;started = time.monotonic()
    with partial.open('xb') as output:
        for offset in range(0, CAPACITY, segment_bytes):
            length = min(segment_bytes, CAPACITY - offset)
            sequence = (sequence + 1) & 0xFFFFFFFF or 1
            path, evidence = read_segment(memory, offset, length, sequence, ready_timeout, attempts)
            with path.open('rb') as source:
                for data in iter(lambda: source.read(1024 * 1024), b''):
                    output.write(data);digest.update(data)
            output.flush();os.fsync(output.fileno())
            segments.append(evidence)
            progress(offset + length, segments)
        require(output.tell() == CAPACITY, 'Raw capture byte count mismatch')
    require(not destination.exists(), 'Capture destination appeared concurrently')
    partial.rename(destination)
    return dict(path=str(destination.resolve()), address=0, length=CAPACITY,
                sha256=digest.hexdigest(), terminal_done_validated=True, segments=segments,
                elapsed_seconds=round(time.monotonic() - started, 3)), sequence


def compare(a: Path, b: Path) -> dict:
    """Fresh independent A/B byte comparison plus SHA256, compatible with USB manifest."""
    require(not os.path.samefile(a, b) and a.stat().st_size == CAPACITY and b.stat().st_size == CAPACITY,
            'Need two distinct full128MiB captures')
    hashes = [hashlib.sha256(), hashlib.sha256()];total = 0;differences = 0;first = None
    with a.open('rb') as left, b.open('rb') as right:
        while True:
            x, y = left.read(1024 * 1024), right.read(1024 * 1024)
            if not x and not y:
                break
            require(len(x) == len(y), 'Capture sizes changed during comparison')
            hashes[0].update(x);hashes[1].update(y)
            if x != y:
                for index, (vx, vy) in enumerate(zip(x, y)):
                    if vx != vy:
                        differences += 1
                        if first is None:
                            first = total + index
            total += len(x)
    require(total == CAPACITY, 'Capture changed size during comparison')
    return dict(a=str(a), b=str(b), length=total, sha256_a=hashes[0].hexdigest(),
                sha256_b=hashes[1].hexdigest(), differing_bytes=differences,
                first_difference=first, byte_identical=differences == 0)


def main() -> None:
    """Only explicit invocation contacts a debugger; importing this module is offline."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('status', 'backup'))
    parser.add_argument('--serial', required=True, help='specific ST-LINK serial')
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--elf', type=Path)
    source.add_argument('--mailbox-address', type=lambda value: int(value, 0))
    parser.add_argument('--directory', type=Path, required=True, help='new evidence directory')
    parser.add_argument('--read-khz', type=int, default=100, help='use only a separately qualified SWD read rate')
    parser.add_argument('--timeout', type=float, default=600)
    parser.add_argument('--ready-timeout', type=float, default=180)
    parser.add_argument('--segment-bytes', type=lambda value: int(value, 0), default=BUFFER_BYTES)
    parser.add_argument('--read-attempts', type=int, default=3)
    args = parser.parse_args()
    require(0 < args.segment_bytes <= BUFFER_BYTES and args.segment_bytes % 4096 == 0,
            'Segment length must be aligned4KiB and at most8MiB')
    require(1 <= args.read_attempts <= 5 and args.ready_timeout > 0, 'Invalid retry/ready timeout')
    mailbox = mailbox_symbol(args.elf) if args.elf else args.mailbox_address
    directory = args.directory.resolve();directory.mkdir(parents=True, exist_ok=False)
    memory = LiveMemory(args.serial, directory, mailbox, args.read_khz, args.timeout)
    manifest_path = directory / 'manifest.json'
    manifest = dict(schema=1, transport='live-SWD-SDRAM-mailbox', state='started', verified=False,
                    captures=[], created_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
                    stlink_serial=args.serial, read_khz=args.read_khz, mailbox_address=mailbox,
                    elf=str(args.elf.resolve()) if args.elf else None)
    save(manifest_path, manifest)
    quiesced = False
    try:
        initial = identity(memory);manifest['identity'] = initial;save(manifest_path, manifest)
        if args.command == 'status':
            manifest['state'] = 'status_only';save(manifest_path, manifest)
            print(json.dumps(initial, indent=2));return
        require(args.elf is not None, 'Backup requires --elf; mailbox-address alone is read-only status')
        manifest['app_verification'] = memory.verify_app(args.elf)
        quiesced = memory.quiesce(args.elf, True)
        manifest['writes_quiesced'] = quiesced
        save(manifest_path, manifest)
        box = memory.descriptor()
        require(box['state'] != 1, 'An existing mailbox transfer is busy; do not replace it')
        sequence = box['last_request_seq']
        for label in ('A', 'B'):
            def progress(received, segments):
                manifest['progress'] = dict(capture=label, received=received, segments=segments)
                save(manifest_path, manifest)
                print(f'{label}: {received}/{CAPACITY} bytes CRC-validated', flush=True)
            result, sequence = capture(memory, directory / f'{label}.bin', args.segment_bytes,
                                       args.ready_timeout, args.read_attempts, sequence, progress)
            manifest['captures'].append(result);save(manifest_path, manifest)
        final = identity(memory)
        require(final == initial, 'MCU/NOR identity or boot metadata changed during capture')
        comparison = compare(directory / 'A.bin', directory / 'B.bin')
        manifest['comparison'] = comparison
        require(comparison['byte_identical'], 'A/B differ; writes remain locked')
        manifest.update(state='verified', verified=True)
        save(manifest_path, manifest)
        print(json.dumps(comparison, indent=2))
    except BaseException as error:
        manifest.update(state='failed', verified=False, error=f'{type(error).__name__}: {error}')
        save(manifest_path, manifest)
        raise
    finally:
        if memory.quiesce_owned:
            try:
                memory.quiesce(args.elf, False)
                manifest['write_lease_released'] = True
            except BaseException as resume_error:
                # Keep the original capture error and the independent resume
                # failure. Do not leave a successful-looking maintenance run.
                manifest.update(state='resume_failed', verified=False,
                                write_lease_released=False, resume_error=str(resume_error))
                save(manifest_path, manifest)
                raise
            save(manifest_path, manifest)


if __name__ == '__main__':
    main()
