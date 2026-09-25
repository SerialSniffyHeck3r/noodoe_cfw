"""Capture real FT81x RGB565 output through a bounded SWD SRAM mailbox.

No flash write, reset, direct peripheral operation, or synthetic UI rendering.
--execute explicitly enables SWD; otherwise the validated plan is printed.
The MCU snapshots its current EVE output and returns4KiB chunks with CRC32.
Raw bytes, chunk logs, decoded PNG and a SHA256 manifest are kept together.
"""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path
import secrets
import struct
import time
import zlib
from bringup import Board, APP, require, sha
from verified_flash import read_expected

FIELDS = "magic version request_seq command offset length expected_generation response_seq status generation width height format total_bytes payload_length payload_crc32 duration_ms eve_frames requests failures".split()
HEADER = struct.Struct("<20I")
TOTAL = 480*480*2
CHUNK = 4096
MAILBOX_SIZE = HEADER.size+CHUNK


class LiveBoard(Board):
    """Capture-only hotplug transport; central flashing clock rules stay intact.

    MCU runs throughout reads and mailbox publication. The response is immutable
    after its final sequence store until this host sends another request. This
    local subclass never invokes inherited program/resume/snapshot methods.
    """
    def __init__(self, serial: str, folder: Path, frequency_khz: int, mailbox: int, hud_address: int | None = None):
        require(frequency_khz in (50,100,400,950),"Unsupported capture-only SWD frequency")
        self.serial,self.folder,self.frequency_khz=serial,folder,frequency_khz
        self.read_frequency_khz=frequency_khz
        self.live_flash_reads=True
        self.mailbox=mailbox
        self.hud_address=hud_address

    def prepare(self) -> None:
        info=self.command("target","-ob","displ")
        require(re.search(r"Device ID\s*:\s*0x419\b",info),"Unexpected MCU ID")
        require(re.search(r"(?:Flash|NVM) size\s*:\s*512\s*KBytes",info),"Expected512KiB target")
        require(re.search(r"RDP\s*:\s*0xAA\b",info,re.I),"Expected RDP0; never unlock automatically")

    def dump(self, name: str, address: int, size: int, resume: bool = True) -> bytes:
        del resume
        require((APP<=address<address+size<=0x08080000) or
                (self.mailbox<=address<address+size<=self.mailbox+MAILBOX_SIZE) or
                (address==0xE000EDF0 and size==4) or
                (self.hud_address is not None and address==self.hud_address and size==4),
                "Capture read is outside APP identity/mailbox/DHCSR ranges")
        path=self.folder/f"{name}.bin"
        require(not path.exists(),f"Refusing to reuse capture data: {path}")
        self.command(name,"-u",hex(address),hex(size),str(path))
        require(path.exists() and path.stat().st_size==size,f"Incomplete live read: {path}")
        return path.read_bytes()

    def assert_running(self, name: str) -> int:
        """Observe S_HALT without modifying debug control or resuming a target.

        The capture path needs a running graphics owner. An unexpectedly halted
        CPU is an error for the operator to investigate, never an implicit -run.
        """
        value=struct.unpack("<I",self.dump(name,0xE000EDF0,4))[0]
        require(not value&(1<<17),f"CPU is halted during capture: DHCSR=0x{value:08X}")
        return value


def decode_header(data: bytes) -> dict:
    require(len(data) >= HEADER.size, "Truncated screenshot mailbox header")
    result = dict(zip(FIELDS, HEADER.unpack(data[:HEADER.size])))
    require(result["magic"] == 0x43415031 and result["version"] == 1, "Screenshot mailbox ABI mismatch")
    require((result["width"], result["height"], result["format"], result["total_bytes"]) == (480,480,7,TOTAL), "Unexpected snapshot format/dimensions")
    return result


def decode_rgb565(raw: bytes, destination: Path) -> None:
    """Losslessly interpret each captured little-endian RGB565 value as RGB8.

    This changes encoding only: no resizing, cropping, recoloring, masking or
    reconstruction. Panel glass/backlight defects are outside EVE's snapshot.
    """
    require(len(raw)==TOTAL,"RGB565 image has the wrong byte length")
    from PIL import Image
    rgb=bytearray(480*480*3)
    for index in range(480*480):
        value=raw[index*2]|raw[index*2+1]<<8
        r,g,b=(value>>11)&31,(value>>5)&63,value&31
        rgb[index*3:index*3+3]=bytes(((r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2)))
    Image.frombytes("RGB",(480,480),bytes(rgb)).save(destination)


class Mailbox:
    def __init__(self, board: Board, address: int):
        self.board,self.address=board,address
        self.sequence=secrets.randbits(31) or 1
        self.call_count=0

    def request(self, kind:int, generation:int=0, offset:int=0, length:int=0) -> int:
        self.sequence=(self.sequence+1)&0x7FFFFFFF or 1
        self.call_count+=1
        operations=[]
        for relative,value in ((12,kind),(16,offset),(20,length),(24,generation),(8,self.sequence)):
            operations += ["-w32",hex(self.address+relative),hex(value)]
        self.board.command(f"request-{self.call_count:04d}",*operations)
        return self.sequence

    def response(self, sequence:int, with_payload:bool=False) -> tuple[dict,bytes]:
        deadline=time.monotonic()+15
        for attempt in range(1,21):
            data=self.board.dump(f"response-{self.call_count:04d}-{attempt:02d}",self.address,
                                 MAILBOX_SIZE if with_payload else HEADER.size)
            result=decode_header(data)
            require(result["request_seq"]==sequence,"Another requester changed the screenshot mailbox")
            if result["response_seq"]==sequence:
                require(result["status"]==0,f"Firmware screenshot failed: {result}")
                size=result["payload_length"]
                require(size<=CHUNK and (with_payload or size==0),"Invalid mailbox payload length")
                payload=data[HEADER.size:HEADER.size+size]
                require(len(payload)==size and zlib.crc32(payload)&0xFFFFFFFF==result["payload_crc32"],"Screenshot chunk CRC mismatch")
                return result,payload
            require(time.monotonic()<deadline,"Graphics owner did not process the screenshot request")
            time.sleep(0.05)
        raise RuntimeError("Screenshot request did not complete")


def main(argv=None) -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--serial",default="STLINK_SERIAL_REQUIRED")
    parser.add_argument("--frequency-khz","--frequency",dest="frequency",type=int,choices=(50,100,400,950),default=100)
    parser.add_argument("--execute",action="store_true")
    parser.add_argument("--page",type=int,choices=(1,2,3),help="Snapshot one results page, then restore automatic/prior selection")
    args=parser.parse_args(argv)
    manifest=json.loads(args.manifest.read_text(encoding="utf-8"))
    image_base=int(manifest["flash_address"],0)
    require(manifest.get("status")=="valid" and
            (manifest.get("layout_version",1),image_base) in ((1,APP),(2,0x08020000)),
            "Expected validated Product/legacy APP ELF manifest")
    symbol=manifest["symbols"].get("g_bsp_capture")
    require(isinstance(symbol,dict) and symbol["size"]==MAILBOX_SIZE,"ELF has no matching screenshot mailbox")
    address=int(symbol["address"],0) if isinstance(symbol["address"],str) else symbol["address"]
    require(0x20000000<=address<=0x20030000-MAILBOX_SIZE,"Screenshot mailbox is outside ordinary SRAM")
    hud_address=None
    if args.page is not None:
        hud=manifest["symbols"].get("g_graphics_bringup_page")
        require(isinstance(hud,dict) and hud["size"]==4,"Image does not expose the integrated results page selector")
        hud_address=int(hud["address"],0) if isinstance(hud["address"],str) else hud["address"]
        require(0x20000000<=hud_address<=0x2002FFFC and hud_address%4==0,"Invalid results page selector address")
    plan={"hardware_access":args.execute,"flash_writes":False,"reset":False,"mailbox":hex(address),
          "mailbox_bytes":MAILBOX_SIZE,"image_bytes":TOTAL,"chunks":(TOTAL+CHUNK-1)//CHUNK,
          "capture_storage":"firmware-owned snapshot; mailbox ABI does not expose GPU addresses","output":str(args.output.resolve()),
          "measurement":"Actual EVE raster; not a photograph of the panel",
          "debug_effects":"Live HOTPLUG reads/mailbox writes; no CPU halt/reset/freeze. Snapshot pauses EVE output briefly",
          "frequency_khz":args.frequency,"results_page":args.page}
    if not args.execute:
        print(json.dumps(plan,indent=2));return 0
    folder=args.output.resolve();folder.mkdir(parents=True,exist_ok=False)
    board=LiveBoard(args.serial,folder,args.frequency,address,hud_address);result=dict(plan,state="incomplete")
    previous_page=None
    try:
        board.prepare()
        reference=args.manifest.parent/"app.bin"
        expected=reference.read_bytes()
        require(len(expected)==manifest["binary_size"] and sha(expected)==manifest["binary_sha256"],
                "Local capture reference differs from the validated manifest")
        require(0<len(expected)<=0x08080000-image_base,"Capture reference escapes APP flash")
        # Preserve the original observation. Inconsistent immutable flash pages
        # require TWO matching fresh reads and exact reference equality.
        image=read_expected(board,"app-identity",image_base,expected)
        require(sha(image)==manifest["binary_sha256"],"Running APP differs from the mailbox symbol manifest")
        initial=decode_header(board.dump("mailbox-before",address,HEADER.size))
        require(initial["request_seq"]==initial["response_seq"],"Another screenshot request is active")
        mailbox=Mailbox(board,address)
        result["dhcsr_before_snapshot"]=board.assert_running("dhcsr-before-snapshot")
        if hud_address is not None:
            previous_page=struct.unpack("<I",board.dump("page-before",hud_address,4))[0]
            require(previous_page<=3,"Invalid existing results page selection")
            board.command("select-page","-w32",hex(hud_address),hex(args.page))
            time.sleep(0.65) # UI owner publishes at500ms; leave a render interval too.
        sequence=mailbox.request(1);snapshot,_=mailbox.response(sequence)
        result["dhcsr_after_snapshot"]=board.assert_running("dhcsr-after-snapshot")
        if previous_page is not None:
            board.command("restore-page","-w32",hex(hud_address),hex(previous_page))
            previous_page=None
        generation=snapshot["generation"];require(generation!=0,"Invalid capture generation")
        raw=bytearray();chunks=[]
        while len(raw)<TOTAL:
            offset=len(raw);size=min(CHUNK,TOTAL-offset)
            sequence=mailbox.request(2,generation,offset,size)
            header,payload=mailbox.response(sequence,True)
            require(header["generation"]==generation and header["expected_generation"]==generation and header["offset"]==offset and len(payload)==size,"Snapshot generation/offset/length changed during export")
            raw.extend(payload);chunks.append({"offset":offset,"length":size,"crc32":f"{header['payload_crc32']:08x}"})
            if len(chunks)%10==0:print(f"Screenshot {len(raw):,}/{TOTAL:,} bytes",flush=True)
        sequence=mailbox.request(3);mailbox.response(sequence)
        result["dhcsr_after_export"]=board.assert_running("dhcsr-after-export")
        (folder/"display.rgb565").write_bytes(raw)
        decode_rgb565(bytes(raw),folder/"display.png")
        result.update(state="complete",snapshot=snapshot,raw_sha256=sha(raw),png_sha256=sha((folder/"display.png").read_bytes()),
                      chunks=chunks,source_app_sha256=manifest["binary_sha256"],visual_panel_confirmation=False)
        print(str(folder/"display.png"),flush=True)
        return 0
    except Exception as error:
        result.update(state="failed",error=str(error));raise
    finally:
        if previous_page is not None and previous_page<=3:
            try:
                board.assert_running("dhcsr-before-page-cleanup")
                board.command("restore-page-after-error","-w32",hex(hud_address),hex(previous_page))
            except Exception as error:
                result["page_restore_error"]=str(error)
        (folder/"capture.json").write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")


if __name__=="__main__":
    raise SystemExit(main())
