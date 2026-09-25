#!/usr/bin/env python3
"""Read-only, framed USB CDC export of the complete Noodoe SPI NOR.

Examples:
  python tools/storage_backup.py ports
  python tools/storage_backup.py hello --port COM7
  python tools/storage_backup.py backup --port COM7 --directory evidence/nor
  python tools/storage_backup.py verify --a evidence/nor/A.bin --b evidence/nor/B.bin

Default commands send HELLO, IDENTITY, READ and CANCEL. Files are created exclusively, incomplete
captures retain the .partial suffix, and an A/B manifest is verified only after
both independent protocol-complete 128MiB reads match byte-for-byte.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import secrets
import struct
import sys
import time
import zlib

REQUEST = struct.Struct("<4sBBHIII")
RESPONSE = struct.Struct("<4sBBHIIII")
HELLO = struct.Struct("<12I")
CAPACITY = 0x08000000
MAX_CHUNK = 4096
HELLO_FIELDS = ("jedec_id", "capacity_bytes", "spi_clock_hz", "initialization_result",
                "max_chunk", "capabilities", "filesystem_bytes", "nvm_address",
                "nvm_bytes", "bl_staging_address", "app_staging_address", "app_staging_bytes")


class ProtocolError(RuntimeError):
    """An incomplete or contradictory stream is never accepted as a backup."""


class Client:
    def __init__(self, port, timeout: float = 15.0):
        self.port = port
        self.timeout = timeout
        self.sequence = secrets.randbits(32)

    def send(self, opcode: int, address: int = 0, length: int = 0) -> int:
        self.sequence = (self.sequence + 1) & 0xFFFFFFFF
        request = REQUEST.pack(b"NDRQ", 1, opcode, 0, self.sequence, address, length)
        if self.port.write(request) != len(request):
            raise ProtocolError("Short USB request write")
        return self.sequence

    def exact(self, length: int) -> bytes:
        """Serial reads can split at any byte; timeout applies to this field."""
        result = bytearray()
        deadline = time.monotonic() + self.timeout
        while len(result) < length:
            chunk = self.port.read(length - len(result))
            if chunk:
                result.extend(chunk)
            elif time.monotonic() >= deadline:
                raise ProtocolError(f"Timed out after {len(result)}/{length} bytes")
        return bytes(result)

    def frame(self) -> tuple[int, int, int, int, bytes]:
        header = self.exact(RESPONSE.size)
        magic, version, opcode, status, sequence, address, length, crc = RESPONSE.unpack(header)
        if magic != b"NDRS" or version != 1:
            raise ProtocolError(f"Invalid response header: {header.hex()}")
        if length > MAX_CHUNK:
            raise ProtocolError(f"Oversized frame payload: {length}")
        payload = self.exact(length)
        actual_crc = zlib.crc32(payload) & 0xFFFFFFFF
        if actual_crc != crc:
            raise ProtocolError(f"CRC mismatch at 0x{address:08X}: {crc:08X} != {actual_crc:08X}")
        return opcode, status, sequence, address, payload

    def synchronize(self) -> None:
        """Drain a previous stream without treating its bytes as a new capture.

        Input reset may leave a partially transmitted stale frame. Seek magic
        only during this explicit pre-capture phase; actual READ is strict.
        """
        self.port.reset_input_buffer()
        sequence = self.send(3)
        deadline = time.monotonic() + self.timeout
        window = bytearray()
        while time.monotonic() < deadline:
            byte = self.port.read(1)
            if not byte:
                continue
            window.extend(byte)
            if len(window) > 4:
                del window[0]
            if window != b"NDRS":
                continue
            header = b"NDRS" + self.exact(RESPONSE.size - 4)
            _, version, opcode, status, seq, _, length, crc = RESPONSE.unpack(header)
            if version != 1 or length > MAX_CHUNK:
                window.clear()
                continue
            payload = self.exact(length)
            if zlib.crc32(payload) & 0xFFFFFFFF != crc:
                window.clear()
                continue
            if opcode == 3 and seq == sequence:
                if status or length:
                    raise ProtocolError("CANCEL acknowledgement is invalid")
                return
            window.clear()
        raise ProtocolError("No matching CANCEL acknowledgement; port remains unsynchronized")

    def hello(self) -> dict:
        sequence = self.send(1)
        opcode, status, seq, address, payload = self.frame()
        if (opcode, status, seq, address, len(payload)) != (1, 0, sequence, 0, HELLO.size):
            raise ProtocolError(f"Invalid HELLO response: {(opcode, status, seq, address, len(payload))}")
        return dict(zip(HELLO_FIELDS, HELLO.unpack(payload)))

    def identity(self, *, optional=False) -> dict | None:
        sequence = self.send(4)
        opcode, status, seq, address, payload = self.frame()
        if optional and (opcode, status, seq, len(payload)) == (4, 5, sequence, 0):
            return None  # First read-only firmware predates this additive opcode.
        if (opcode, status, seq, address, len(payload)) != (4, 0, sequence, 0, 32):
            raise ProtocolError("Invalid/missing board IDENTITY response")
        words = struct.unpack("<8I", payload)
        return {"uid_words": list(words[:3]), "usb_serial": "".join(f"{v:08X}" for v in words[:3]),
                "boot_metadata_words": list(words[3:])}

    def storage_control(self, opcode: int) -> None:
        """Allowlist explicit control requests; no host raw program/erase API."""
        parameters = {0x10: (0x07F70000, 0x42414B32), 0x11: (0, 0),
                      0x12: (0x07F70000, 0x464D5431)}
        if opcode not in parameters:
            raise ValueError("Unsupported storage control opcode")
        address, token = parameters[opcode]
        sequence = self.send(opcode, address, token)
        op, status, seq, offset, payload = self.frame()
        expected_size = 0 if opcode == 0x11 else 4
        if (op, seq, offset, len(payload)) != (opcode, sequence, address, expected_size):
            raise ProtocolError("Unexpected storage control response")
        result = struct.unpack("<I", payload)[0] if payload else 0
        if status or result:
            raise ProtocolError(f"Storage control rejected: status={status}, device_result={result}")

    def capture(self, destination: Path, address: int, length: int, *, progress=True) -> dict:
        """Write only validated payloads and require the terminal DONE marker.

        Failed captures stay .partial for analysis. A read never truncates an
        existing file, reuses old bytes, resumes, or silently fills missing data.
        """
        if not 0 <= address < CAPACITY or not 0 < length <= CAPACITY - address:
            raise ValueError("Read range is outside the 128MiB NOR")
        destination = destination.resolve()
        partial = destination.with_name(destination.name + ".partial")
        if destination.exists():
            raise FileExistsError(destination)
        destination.parent.mkdir(parents=True, exist_ok=True)
        digest = hashlib.sha256()
        received = frames = 0
        started = last_report = time.monotonic()
        with partial.open("xb") as output:
            sequence = self.send(2, address, length)
            while True:
                opcode, status, seq, offset, payload = self.frame()
                if seq != sequence or status or offset != address + received:
                    raise ProtocolError(f"READ frame mismatch: opcode={opcode} status={status} sequence={seq} offset=0x{offset:08X}; expected sequence={sequence}, offset=0x{address+received:08X}")
                if opcode == 0x82:
                    if payload or received != length:
                        raise ProtocolError(f"Premature/invalid DONE: {received}/{length}")
                    break
                if opcode != 2 or not payload or len(payload) > length - received:
                    raise ProtocolError("Invalid READ opcode, empty block, or excess bytes")
                output.write(payload)
                digest.update(payload)
                received += len(payload)
                frames += 1
                now = time.monotonic()
                if progress and now - last_report >= 2:
                    rate = received / max(now - started, 0.001)
                    print(f"{destination.name}: {received/length:6.1%}, {received:,} bytes, {rate/1024:.0f} KiB/s, ETA {(length-received)/max(rate,1):.0f}s", flush=True)
                    last_report = now
            output.flush()
            os.fsync(output.fileno())
        if partial.stat().st_size != length:
            raise ProtocolError("On-disk file size differs from the validated stream")
        # On Windows rename fails if the destination exists; repeat the explicit
        # check for other hosts to avoid replacing a concurrently created dump.
        if destination.exists():
            raise FileExistsError(destination)
        partial.rename(destination)
        return {"path": str(destination), "address": address, "length": received,
                "sha256": digest.hexdigest(), "sequence": sequence, "data_frames": frames,
                "terminal_done_validated": True, "elapsed_seconds": round(time.monotonic()-started, 3)}


def compare_files(a: Path, b: Path, expected_size: int = CAPACITY) -> dict:
    """Hash both independent files and compare every byte in one bounded pass."""
    a, b = a.resolve(), b.resolve()
    if a == b or os.path.samefile(a, b):
        raise ValueError("A and B must be distinct files, not aliases of one capture")
    if a.stat().st_size != expected_size or b.stat().st_size != expected_size:
        raise ValueError(f"Both files must contain exactly {expected_size:,} bytes")
    hashes = [hashlib.sha256(), hashlib.sha256()]
    offset = differences = 0
    first_difference = None
    with a.open("rb") as left, b.open("rb") as right:
        while True:
            x, y = left.read(1024 * 1024), right.read(1024 * 1024)
            if not x and not y:
                break
            hashes[0].update(x)
            hashes[1].update(y)
            if x != y:
                for i, (vx, vy) in enumerate(zip(x, y)):
                    if vx != vy:
                        differences += 1
                        if first_difference is None:
                            first_difference = offset + i
                if len(x) != len(y):
                    raise ProtocolError("Files changed size during verification")
            offset += len(x)
    if offset != expected_size:
        raise ProtocolError("Files changed size during verification")
    return {"a": str(a), "b": str(b), "length": offset,
            "sha256_a": hashes[0].hexdigest(), "sha256_b": hashes[1].hexdigest(),
            "differing_bytes": differences, "first_difference": first_difference,
            "byte_identical": differences == 0}


def save_manifest(path: Path, value: dict) -> None:
    """Replace only our own manifest; raw captures are never overwritten."""
    temporary = path.with_suffix(path.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8") as output:
        json.dump(value, output, indent=2)
        output.write("\n")
        output.flush()
        os.fsync(output.fileno())
    temporary.replace(path)


def verify_unlock_manifest(path: Path, identity: dict) -> dict:
    """Freshly reread both whole files and bind them to this MCU's UID.

    The explicit firmware token protects accidental calls, not an adversarial
    host. The host performs this proof; the MCU does not hash the host's files.
    """
    manifest = json.loads(path.read_text(encoding="utf-8"))
    captures = manifest.get("captures", [])
    if manifest.get("verified") is not True or manifest.get("state") != "verified" or len(captures) != 2:
        raise ValueError("Manifest is not a verified independent A/B capture")
    for capture in captures:
        if capture.get("address") != 0 or capture.get("length") != CAPACITY or capture.get("terminal_done_validated") is not True:
            raise ValueError("Manifest lacks two complete protocol-validated raw NOR reads")
    recorded = (manifest.get("identity") or {}).get("usb_serial") or manifest.get("usb_serial")
    if not recorded or recorded.upper() != identity["usb_serial"].upper():
        raise ValueError("Backup MCU UID is missing or belongs to another device")
    result = compare_files(Path(captures[0]["path"]), Path(captures[1]["path"]))
    if not result["byte_identical"] or result["sha256_a"] != captures[0]["sha256"] or result["sha256_b"] != captures[1]["sha256"]:
        raise ValueError("Fresh A/B validation failed; device remains locked")
    metadata = identity["boot_metadata_words"]
    if metadata[0] != 0x000E0000 or metadata[4] != 0:
        raise ValueError("Unexpected stock boot version or pending install; device remains locked")
    return result


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("ports", help="List serial ports; do not open any")
    for command in ("hello", "read", "backup", "unlock", "lock", "format"):
        child = sub.add_parser(command)
        child.add_argument("--port", required=True)
        child.add_argument("--timeout", type=float, default=15)
        if command == "read":
            child.add_argument("--output", type=Path, required=True)
            child.add_argument("--address", type=lambda x: int(x, 0), default=0)
            child.add_argument("--length", type=lambda x: int(x, 0), default=CAPACITY)
        if command == "backup":
            child.add_argument("--directory", type=Path, required=True,
                               help="New directory for A.bin, B.bin and manifest.json")
        if command in ("unlock", "format"):
            child.add_argument("--manifest", type=Path, required=True,
                               help="Verified full128MiB A/B manifest bound to this MCU")
    verify = sub.add_parser("verify", help="Offline comparison; does not prove protocol completion")
    verify.add_argument("--a", type=Path, required=True)
    verify.add_argument("--b", type=Path, required=True)
    args = parser.parse_args(argv)
    if args.command == "verify":
        result = compare_files(args.a, args.b)
        print(json.dumps(result, indent=2))
        return 0 if result["byte_identical"] else 2
    try:
        import serial
        import serial.tools.list_ports
    except ImportError as exc:
        raise RuntimeError("Install pyserial for USB CDC: python -m pip install pyserial") from exc
    if args.command == "ports":
        print(json.dumps([{"port": p.device, "description": p.description, "hwid": p.hwid}
                          for p in serial.tools.list_ports.comports()], indent=2))
        return 0
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    manifest = manifest_path = None
    if args.command == "backup":
        directory = args.directory.resolve()
        directory.mkdir(parents=True, exist_ok=False)
        manifest_path = directory / "manifest.json"
        matching_port = next((p for p in serial.tools.list_ports.comports() if p.device.lower() == args.port.lower()), None)
        manifest = {"schema": 1, "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
                    "port": args.port, "state": "started", "verified": False, "captures": []}
        if matching_port is not None:
            manifest["usb_serial"] = matching_port.serial_number
        save_manifest(manifest_path, manifest)
    try:
        with serial.Serial(args.port, 115200, timeout=0.2, write_timeout=args.timeout) as port:
            client = Client(port, args.timeout)
            client.synchronize()
            hello = client.hello()
            print(json.dumps(hello, indent=2), flush=True)
            if args.command == "hello":
                print(json.dumps(client.identity(optional=True), indent=2))
                return 0
            if args.command == "lock":
                client.storage_control(0x11)
                print("Filesystem/NVM writes locked; OTA capability is independent.")
                return 0
            if hello["capacity_bytes"] != CAPACITY or hello["jedec_id"] != 0xC2201B:
                raise ProtocolError("Unexpected NOR capacity/ID; no capture started")
            if hello["initialization_result"] or not hello["capabilities"] & 1 or not 0 < hello["max_chunk"] <= MAX_CHUNK:
                raise ProtocolError("Device read transport is not verified/ready")
            if args.command == "read":
                print(json.dumps(client.capture(args.output, args.address, args.length), indent=2))
                return 0
            if args.command in ("unlock", "format"):
                identity = client.identity()
                comparison = verify_unlock_manifest(args.manifest, identity)
                print(json.dumps(comparison, indent=2), flush=True)
                client.storage_control(0x10)
                if args.command == "format":
                    # Format is an explicit separate request after proof/unlock.
                    # Relock even if format returns an ordinary protocol error.
                    try:
                        client.storage_control(0x12)
                        print("Filesystem formatted; NVM and BL/APP staging preserved.")
                    finally:
                        client.storage_control(0x11)
                else:
                    print("Filesystem/NVM writes unlocked for this boot/USB session.")
                return 0
            manifest["hello"] = hello
            manifest["identity"] = client.identity(optional=True)
            manifest["state"] = "capturing"
            save_manifest(manifest_path, manifest)
            for name in ("A.bin", "B.bin"):
                manifest["captures"].append(client.capture(directory / name, 0, CAPACITY))
                save_manifest(manifest_path, manifest)
            comparison = compare_files(directory / "A.bin", directory / "B.bin")
            manifest["comparison"] = comparison
            # Re-read hashes must match their streaming hashes too: comparison
            # alone cannot bless two files modified identically after capture.
            if comparison["sha256_a"] != manifest["captures"][0]["sha256"] or comparison["sha256_b"] != manifest["captures"][1]["sha256"]:
                raise ProtocolError("Capture files changed after streaming SHA256")
            manifest["verified"] = comparison["byte_identical"]
            manifest["state"] = "verified" if manifest["verified"] else "mismatch"
            save_manifest(manifest_path, manifest)
            print(json.dumps(comparison, indent=2), flush=True)
            print(f"Manifest: {manifest_path}", flush=True)
            return 0 if manifest["verified"] else 2
    except BaseException as exc:
        if manifest is not None:
            manifest["state"] = "failed"
            manifest["error"] = f"{type(exc).__name__}: {exc}"
            save_manifest(manifest_path, manifest)
        raise


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, ValueError, OSError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
