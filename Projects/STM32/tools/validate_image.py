#!/usr/bin/env python3
"""Validate a Noodoe APP ELF and export an APP-only binary, without hardware access.

This is a flash precondition, not a programmer: a successful build alone does not
establish a safe destination address.  Parse the ELF itself, require the resident
bootloader's memory contract, and cross-check GNU objcopy's output byte for byte.
An ELF cannot prove the filename of its linker script; the build configuration
must independently select Linker/Noodoe_APP.ld.  This script proves its layout.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile


APP_BASE = 0x08010000
APP_END = 0x08080000
RAM_BASE = 0x20000000
RAM_END = 0x20030000
CCM_BASE = 0x10000000
CCM_END = 0x10010000
PT_LOAD = 1
SHT_SYMTAB = 2
SHT_NOBITS = 8
SHF_ALLOC = 2
STB_GLOBAL = 1
STT_OBJECT = 1
STT_FUNC = 2
FUNCTIONS = (
    "Reset_Handler", "main", "SystemInit", "BSP_BootEarly",
    "BSP_BootRuntimeReady", "StartDefaultTask", "BSP_BringupMark",
    "BSP_BringupHalTick", "BSP_FaultRecord", "BSP_FaultClear",
)
OBJECTS = ("g_bsp_boot", "g_bsp_bringup", "g_bsp_fault")


class InvalidImage(Exception):
    """An unsupported or inconsistent image must never be offered to flashing."""


def require(condition: bool, message: str) -> None:
    if not condition:
        raise InvalidImage(message)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def contained(start: int, size: int, lower: int, upper: int) -> bool:
    return size >= 0 and lower <= start and start + size <= upper


class Elf32:
    """Minimal, bounds-checked ELF32/ARM reader; no third-party Python packages."""

    def __init__(self, data: bytes):
        self.data = data
        require(len(data) >= 52, "ELF header is truncated")
        require(data[:7] == b"\x7fELF\x01\x01\x01", "Expected ELF32 little-endian version 1")
        fields = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
        (etype, machine, version, self.entry, phoff, shoff, flags,
         ehsize, phsize, phcount, shsize, shcount, shnames) = fields
        require(etype == 2 and machine == 40 and version == 1,
                "Expected a linked ARM executable, not an object/shared library")
        require(ehsize == 52 and phsize == 32 and shsize == 40,
                "Unsupported ELF table entry size")
        require(0 < phcount < 0xFFFF and 0 < shcount < 0xFF00,
                "Missing/extended ELF tables are not supported")
        require(0 < shnames < shcount, "Invalid section-name table")
        self.flags = flags
        self.programs = []
        for index in range(phcount):
            values = struct.unpack("<IIIIIIII", self.slice(phoff + index * phsize, phsize))
            item = dict(zip(("type", "offset", "vaddr", "paddr", "filesz", "memsz", "flags", "align"), values))
            self.slice(item["offset"], item["filesz"])
            self.programs.append(item)
        self.sections = []
        for index in range(shcount):
            values = struct.unpack("<IIIIIIIIII", self.slice(shoff + index * shsize, shsize))
            item = dict(zip(("name_offset", "type", "flags", "addr", "offset", "size", "link", "info", "align", "entsize"), values))
            if item["type"] != SHT_NOBITS:
                self.slice(item["offset"], item["size"])
            self.sections.append(item)
        names = self.section_data(self.sections[shnames])
        for index, section in enumerate(self.sections):
            section["index"] = index
            section["name"] = self.string(names, section["name_offset"])
        self.symbols = {}
        for section in self.sections:
            if section["type"] != SHT_SYMTAB:
                continue
            require(section["entsize"] == 16 and section["size"] % 16 == 0,
                    "Malformed symbol table")
            require(section["link"] < len(self.sections), "Invalid symbol string-table link")
            strings = self.section_data(self.sections[section["link"]])
            for offset in range(0, section["size"], 16):
                name, value, size, info, other, shndx = struct.unpack(
                    "<IIIBBH", self.slice(section["offset"] + offset, 16))
                symbol_name = self.string(strings, name)
                # Ignore locals/undefined references. Required definitions must
                # be unique, externally visible symbols in the final executable.
                if not symbol_name or info >> 4 == 0 or shndx == 0:
                    continue
                require(symbol_name not in self.symbols, f"Duplicate global symbol {symbol_name}")
                self.symbols[symbol_name] = {
                    "value": value, "size": size, "bind": info >> 4,
                    "type": info & 15, "section": shndx, "other": other,
                }
        require(self.symbols, "ELF has no usable static symbol table; do not strip the validation ELF")

    def slice(self, offset: int, size: int) -> bytes:
        require(contained(offset, size, 0, len(self.data)), "ELF file range is out of bounds")
        return self.data[offset:offset + size]

    @staticmethod
    def string(table: bytes, offset: int) -> str:
        require(offset < len(table), "ELF string offset is out of bounds")
        end = table.find(b"\0", offset)
        require(end >= offset, "ELF string lacks a terminator")
        return table[offset:end].decode("utf-8", errors="strict")

    def section_data(self, section: dict) -> bytes:
        require(section["type"] != SHT_NOBITS, "NOBITS section has no file data")
        return self.slice(section["offset"], section["size"])

    def section(self, name: str) -> dict:
        matches = [s for s in self.sections if s["name"] == name]
        require(len(matches) == 1, f"Expected exactly one {name} section")
        return matches[0]

    def symbol(self, name: str) -> dict:
        require(name in self.symbols, f"Required symbol is missing: {name}")
        return self.symbols[name]

    def section_lma(self, section: dict) -> int:
        # For initialized SRAM, VMA is RAM but physical/load address is FLASH.
        # Check BOTH virtual mapping and file mapping to reject malformed ELFs.
        matches = [p for p in self.programs if p["type"] == PT_LOAD
                   and contained(section["addr"], section["size"], p["vaddr"], p["vaddr"] + p["filesz"])
                   and contained(section["offset"], section["size"], p["offset"], p["offset"] + p["filesz"])
                   and section["addr"] - p["vaddr"] == section["offset"] - p["offset"]]
        require(len(matches) == 1, f"Section {section['name']} has ambiguous/missing file-backed LOAD mapping")
        return matches[0]["paddr"] + section["addr"] - matches[0]["vaddr"]

    def code_bytes(self, address: int, size: int) -> bytes:
        matches = [s for s in self.sections if s["flags"] & SHF_ALLOC
                   and s["type"] != SHT_NOBITS
                   and contained(address, size, s["addr"], s["addr"] + s["size"])]
        require(len(matches) == 1, f"No unique bytes at 0x{address:08X}")
        section = matches[0]
        return self.slice(section["offset"] + address - section["addr"], size)


def validate(elf: Elf32) -> tuple[bytes, dict]:
    """Reject unsafe load addresses before invoking any binary export tool."""
    # Only the project-owned Product linker may opt into the independent gate
    # layout. Never infer a safe flash address merely from an arbitrary vector.
    layout = elf.symbols.get('__noodoe_layout_version', {}).get('value', 1)
    require(layout in (1, 2), 'Unknown Noodoe linker layout')
    APP_BASE = 0x08020000 if layout == 2 else 0x08010000
    RAM_END = 0x2002FF00 if layout == 2 else 0x20030000
    loads = [p for p in elf.programs if p["type"] == PT_LOAD]
    require(loads, "ELF has no loadable segments")
    for segment in loads:
        require(segment["filesz"] <= segment["memsz"], "LOAD file size exceeds memory size")
        if segment["filesz"]:
            require(contained(segment["paddr"], segment["filesz"], APP_BASE, APP_END),
                    f"LOAD LMA 0x{segment['paddr']:08X}+0x{segment['filesz']:X} leaves APP flash; BL/data must be preserved")
        if segment["memsz"]:
            require(any(contained(segment["vaddr"], segment["memsz"], lo, hi)
                        for lo, hi in ((APP_BASE, APP_END), (RAM_BASE, 0x20030000), (CCM_BASE, CCM_END))),
                    f"Unsupported LOAD runtime range at 0x{segment['vaddr']:08X}")

    # Our strong Reset_Handler copies .data and clears .bss and .ccm_bss.
    # CCM is explicitly clocked and cleared by BSP_CCM_Init. Initialized
    # .ccmram is unsupported: it has no copy loop. Reject orphan RAM sections.
    # .noinit is intentional retained diagnostic storage; the generated heap/
    # stack reservation likewise needs no initializer and must be NOBITS.
    allowed_ram_sections = {".data", ".bss", ".noinit", "._user_heap_stack"}
    for section in elf.sections:
        if not section["flags"] & SHF_ALLOC or not section["size"]:
            continue
        if 0x2002FF00 <= section['addr'] < 0x20030000 and layout == 2:
            require(section['name'] == '.gate_mailbox' and section['addr'] == 0x2002FF00
                    and section['size'] == 256 and section['type'] == SHT_NOBITS,
                    'Reserved RecoveryGate mailbox must be exactly 256 retained bytes')
            continue
        if CCM_BASE <= section["addr"] < CCM_END:
            require(section['name']=='.ccm_bss' and section['type']==SHT_NOBITS,
                    'Only explicitly cleared CCM BSS is supported')
            require(section['addr']==elf.symbol('_sccmbss')['value'] and
                    section['addr']+section['size']==elf.symbol('_eccmbss')['value'] and
                    contained(section['addr'],section['size'],CCM_BASE,CCM_END-12288),
                    'CCM clear range/reserve mismatch')
            require(elf.symbol('BSP_CCM_Init')['type']==STT_FUNC,'Missing CCM startup')
        if RAM_BASE <= section["addr"] < RAM_END:
            require(section["name"] in allowed_ram_sections,
                    f"SRAM section {section['name']} is outside the startup copy/clear/retained-storage contract")
            require(contained(section["addr"], section["size"], RAM_BASE, RAM_END),
                    f"SRAM section {section['name']} leaves internal SRAM")
            if section["name"] == "._user_heap_stack":
                require(section["type"] == SHT_NOBITS,
                        "Heap/stack reservation must be NOBITS, not unhandled initialized data")

    vectors = elf.section(".isr_vector")
    if layout == 2:
        mailbox = elf.section('.gate_mailbox')
        require(mailbox['addr'] == RAM_END and mailbox['size'] == 256 and
                mailbox['type'] == SHT_NOBITS, 'Missing RecoveryGate mailbox reservation')
        require(elf.symbol('g_recovery_mailbox')['section'] == mailbox['index'],
                'RecoveryGate ABI object is missing or incorrectly placed')
    require(vectors["addr"] == APP_BASE and vectors["size"] >= 8,
            f"APP vector table must begin at 0x{APP_BASE:08X}")
    require(elf.section_lma(vectors) == APP_BASE, "Vector LMA differs from APP base")
    initial_msp, reset_vector = struct.unpack("<II", elf.section_data(vectors)[:8])
    require(initial_msp == RAM_END and initial_msp & 7 == 0,
            f"Initial MSP must equal the layout's aligned stack ceiling 0x{RAM_END:08X}")
    require(initial_msp & 0x2FFC0000 == 0x20000000, "Initial MSP fails the stock BL acceptance mask")
    require(reset_vector & 1 == 1, "Reset vector must select Thumb state")
    require(elf.symbol("_estack")["value"] == initial_msp, "_estack differs from vector MSP")
    reset = elf.symbol("Reset_Handler")
    require(reset["bind"] == STB_GLOBAL and reset["type"] == STT_FUNC,
            "Reset_Handler must be a strong global function, not the Cube weak default")
    require((reset["value"] & ~1) == (reset_vector & ~1)
            and (elf.entry & ~1) == (reset_vector & ~1), "ELF entry/vector/Reset_Handler disagree")
    require(elf.code_bytes(reset_vector & ~1, 2) == b"\x72\xb6",
            "Reset_Handler must begin with CPSID i before touching BL runtime state")
    require(elf.symbol("g_pfnVectors")["value"] == APP_BASE, "g_pfnVectors differs from vector base")

    for name in FUNCTIONS:
        symbol = elf.symbol(name)
        require(symbol["type"] == STT_FUNC and symbol["size"] > 0,
                f"{name} is not a nonempty function")
        require(contained(symbol["value"] & ~1, symbol["size"], APP_BASE, APP_END),
                f"{name} is not wholly in APP flash")
        elf.code_bytes(symbol["value"] & ~1, symbol["size"])

    # The actual task must resolve to main.c's strong entry, not Cube's weak
    # fallback. This complements the USER CODE/source check after regeneration.
    require(elf.symbol("StartDefaultTask")["bind"] == STB_GLOBAL,
            "StartDefaultTask must resolve to the strong main task")

    bss, noinit, data = (elf.section(name) for name in (".bss", ".noinit", ".data"))
    require(bss["type"] == SHT_NOBITS and noinit["type"] == SHT_NOBITS,
            ".bss and .noinit must be NOBITS/NOLOAD")
    require(noinit["size"] > 0 and noinit["flags"] & SHF_ALLOC,
            "Missing allocated boot diagnostic NOLOAD storage")
    require(contained(noinit["addr"], noinit["size"], RAM_BASE, RAM_END),
            ".noinit must reside in internal SRAM")
    require(noinit["addr"] >= bss["addr"] + bss["size"],
            ".noinit overlaps or precedes .bss; boot records would be erased")
    for segment in loads:
        if segment["filesz"]:
            require(segment["vaddr"] + segment["filesz"] <= noinit["addr"]
                    or segment["vaddr"] >= noinit["addr"] + noinit["size"],
                    ".noinit is NOBITS but a file-backed LOAD segment still covers it")
    require(elf.symbol("_sbss")["value"] == bss["addr"]
            and elf.symbol("_ebss")["value"] == bss["addr"] + bss["size"],
            "Startup BSS bounds differ from .bss")
    require(elf.symbol("_sdata")["value"] == data["addr"]
            and elf.symbol("_edata")["value"] == data["addr"] + data["size"],
            "Startup data bounds differ from .data")
    require(elf.symbol("_sidata")["value"] == elf.section_lma(data),
            "Startup initializer address differs from .data LMA")
    for name in OBJECTS:
        symbol = elf.symbol(name)
        expected = bss if name == "g_bsp_bringup" else noinit
        require(symbol["type"] == STT_OBJECT and symbol["size"] > 0
                and symbol["section"] == expected["index"]
                and contained(symbol["value"], symbol["size"], expected["addr"], expected["addr"] + expected["size"]),
                f"{name} does not occupy the required {expected['name']} storage")

    # Reconstruct exactly the bytes objcopy is allowed to export. ELF debug
    # sections are deliberately excluded. Unoccupied FLASH gaps become erased
    # bytes (0xFF), never a second image or a guessed memory range.
    pieces = []
    for section in elf.sections:
        if not section["flags"] & SHF_ALLOC or not section["size"] or section["type"] == SHT_NOBITS:
            continue
        lma = elf.section_lma(section)
        require(contained(lma, section["size"], APP_BASE, APP_END),
                f"Allocated section {section['name']} leaves APP flash")
        pieces.append((lma, elf.section_data(section), section["name"]))
    pieces.sort()
    require(pieces and pieces[0][0] == APP_BASE, "Binary must start with the APP vector")
    end = max(address + len(payload) for address, payload, _ in pieces)
    image = bytearray(b"\xff" * (end - APP_BASE))
    previous_end = APP_BASE
    for address, payload, name in pieces:
        require(address >= previous_end, f"Overlapping load bytes in {name}")
        image[address - APP_BASE:address - APP_BASE + len(payload)] = payload
        previous_end = address + len(payload)
    requested_symbols = FUNCTIONS + OBJECTS + ("g_pfnVectors", "_estack", "_sdata", "_edata", "_sidata", "_sbss", "_ebss")
    # Display diagnostics are optional in future product builds where LCDTest
    # remains in source but is not called and is removed by section GC.
    optional_functions = ("LCDTest", "Graphics_Init", "Graphics_Process",
                          "GraphicsTest_Init", "GraphicsTest_Process")
    optional_objects = ("g_bsp_lcd_test", "g_bsp_eve", "g_bsp_lcd_panel", "g_bsp_backlight",
                        "g_bsp_buttons", "g_bsp_display", "g_bsp_lcd_buttons",
                        "g_graphics", "g_graphics_test", "g_graphics_test_results",
                        "g_graphics_performance", "g_bsp_cpu_load", "g_graphics_arc_sweep",
                        "g_noodoe_runtime", "g_bluetooth", "g_bsp_bt_hci", "g_bsp_bt_hci_fault", "g_bsp_nor",
                        "g_usb_bridge", "g_storage_backup", "g_bsp_dash", "g_dash_service", "g_dash_mailbox",
                        "g_bsp_ambient", "g_bsp_clock", "g_bsp_ram", "g_bsp_power", "g_bsp_capture",
                        "g_storage_service", "g_storage_swd", "g_noodoe_control",
                        "g_runtime_update", "g_graphics_bringup_page", "g_settings",
                        "g_ambient_service", "g_ambient_mailbox", "g_bluetooth_control", "g_bsp_ambient_bitbang",
                        "g_bsp_ambient_address", "g_bsp_bt_hci_startup",
                        "g_bsp_bt_reset_diagnostic",
                        "g_bsp_ramtest", "g_bsp_ramtest_command", "g_product_ui", "g_product_preview", "g_oil_usage")
    # GC가 제거한 선택적 시험은 허용하되 남은 진단 심볼은 실제 SRAM 객체여야
    # 한다. 이름만 같은 함수/절대 심볼을 RAM 주소로 해석하지 못하게 한다.
    for name in optional_functions:
        if name in elf.symbols:
            symbol = elf.symbol(name)
            require(symbol['type'] == STT_FUNC and symbol['size'] > 0 and
                    contained(symbol['value'] & ~1, symbol['size'], APP_BASE, APP_END),
                    f'{name} is not a nonempty APP function')
    for name in optional_objects:
        if name in elf.symbols:
            symbol = elf.symbol(name)
            require(symbol['type'] == STT_OBJECT and symbol['size'] > 0 and
                    symbol['section'] in (data['index'], bss['index']) and
                    contained(symbol['value'], symbol['size'], RAM_BASE, RAM_END),
                    f'{name} is not an initialized internal SRAM diagnostic object')
    requested_symbols += tuple(name for name in optional_functions + optional_objects if name in elf.symbols)
    manifest = {
        "schema_version": 2, "layout_version": layout, "status": "valid", "hardware_access": False,
        "flash_capacity": APP_END - APP_BASE,
        "flash_address": f"0x{APP_BASE:08X}", "flash_limit_exclusive": f"0x{APP_END:08X}",
        "binary_size": len(image), "binary_sha256": sha256(image),
        "initial_msp": f"0x{initial_msp:08X}", "reset_vector": f"0x{reset_vector:08X}",
        "entry": f"0x{elf.entry:08X}", "elf_flags": f"0x{elf.flags:08X}",
        "linker_contract": f"Noodoe layout {layout} APP at 0x{APP_BASE:08X}; linker filename checked by build configuration",
        "symbols": {name: {"address": f"0x{elf.symbol(name)['value']:08X}",
                           "size": elf.symbol(name)["size"]} for name in requested_symbols},
        "load_segments": [{key: (f"0x{value:08X}" if key in ("vaddr", "paddr") else value)
                           for key, value in segment.items()} for segment in loads],
        "noinit": {"address": f"0x{noinit['addr']:08X}", "size": noinit["size"], "noload": True},
    }
    return bytes(image), manifest


def find_objcopy(toolchain: Path | None) -> Path:
    """Accept an explicit executable/bin/toolchain directory or find CubeIDE GCC."""
    executable = "arm-none-eabi-objcopy.exe" if os.name == "nt" else "arm-none-eabi-objcopy"
    if toolchain:
        candidates = [toolchain, toolchain / executable, toolchain / "bin" / executable,
                      toolchain / "tools" / "bin" / executable]
    else:
        candidates = []
        on_path = shutil.which(executable)
        if on_path:
            candidates.append(Path(on_path))
        # No user-specific paths are embedded in source or generated project files.
        root = Path(os.environ.get("SystemDrive", "C:") + os.sep) / "ST"
        if root.is_dir():
            candidates.extend(sorted(root.glob(
                "STM32CubeIDE*/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32*/tools/bin/" + executable), reverse=True))
    for candidate in candidates:
        if candidate.is_file() and candidate.name.lower() == executable.lower():
            return candidate.resolve()
    raise InvalidImage("arm-none-eabi-objcopy not found; supply --toolchain <bin directory>")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path, help="unstripped Debug/Release ELF")
    parser.add_argument("--toolchain", type=Path, help="GCC toolchain root, bin folder, or objcopy executable")
    parser.add_argument("--check-only", action="store_true", help="validate/export in a temporary folder without replacing app.bin or manifest")
    args = parser.parse_args()
    try:
        elf_path = args.elf.resolve(strict=True)
        source = elf_path.read_bytes()
        expected, manifest = validate(Elf32(source))
        objcopy = find_objcopy(args.toolchain)
        with tempfile.TemporaryDirectory(prefix="noodoe-image-") as scratch:
            exported = Path(scratch) / "app.bin"
            command = [str(objcopy), "-O", "binary", "--gap-fill=0xFF", str(elf_path), str(exported)]
            result = subprocess.run(command, capture_output=True, text=True, timeout=30,
                                    creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            require(result.returncode == 0, f"objcopy failed: {result.stderr.strip()}")
            require(exported.read_bytes() == expected, "objcopy bytes differ from independently validated ELF load bytes")
        require(elf_path.read_bytes() == source, "ELF changed during validation; wait for the build to finish")
        manifest.update({"elf": str(elf_path), "elf_size": len(source), "elf_sha256": sha256(source),
                         "objcopy": str(objcopy), "binary": str(elf_path.parent / "app.bin")})
        if not args.check_only:
            # Stage complete validated files beside their destination; os.replace
            # avoids ever exposing a partially written binary or JSON document.
            for destination, payload in (
                (elf_path.parent / "app.bin", expected),
                (elf_path.parent / "app.manifest.json", (json.dumps(manifest, indent=2) + "\n").encode("utf-8")),
            ):
                temporary = None
                try:
                    with tempfile.NamedTemporaryFile(dir=destination.parent, prefix=destination.name + ".", delete=False) as stream:
                        temporary = Path(stream.name)
                        stream.write(payload)
                    os.replace(temporary, destination)
                finally:
                    if temporary is not None and temporary.exists():
                        temporary.unlink()
        print(json.dumps(manifest, indent=2))
        return 0
    except (InvalidImage, OSError, ValueError, struct.error, subprocess.SubprocessError) as error:
        print(f"IMAGE VALIDATION FAILED: {error}", file=sys.stderr)
        print("Do not flash; any older app.bin/manifest is not a result of this invocation.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
