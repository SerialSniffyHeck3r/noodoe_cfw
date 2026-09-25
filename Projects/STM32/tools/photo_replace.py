"""Replace one already provisioned CFWPIC slot using Product PhotoImport ABI3.

Uses only verified APP identity, live reads, SDRAM upload and the bounded RAM
mailbox. The firmware owns JPEG validation, inactive-bank erase/program,
physical readback and commit. No FAT creation, reset or APP write is issued.
"""
from pathlib import Path
import argparse, json, struct, subprocess, time, zlib, io
from bringup import Board, require, sha
from storage_swd_backup import LiveMemory, mailbox_symbol, NM


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--elf',type=Path,required=True);ap.add_argument('--serial',required=True)
    ap.add_argument('--jpeg',type=Path,required=True);ap.add_argument('--slot',type=int,choices=range(3),required=True)
    ap.add_argument('--output',type=Path,required=True);ap.add_argument('--read-khz',type=int,default=100)
    args=ap.parse_args();data=args.jpeg.read_bytes()
    require(0<len(data)<=131072,'JPEG exceeds 128KiB')
    from PIL import Image
    with Image.open(io.BytesIO(data)) as im:
        require(im.format=='JPEG' and not im.info.get('progressive') and 0<im.width<=480 and 0<im.height<=480,'Unsupported JPEG')
        im.load()
    out=args.output;out.mkdir(parents=True,exist_ok=False);(out/'input.jpg').write_bytes(data)
    # CubeProgrammer selects its raw loader by extension and rejects .jpg.
    # Keep the original JPEG as evidence; the .bin contains identical bytes,
    # without recompression, padding in the protocol length, or format changes.
    payload_path=out/'input.bin';payload_path.write_bytes(data)
    mem=LiveMemory(args.serial,out,mailbox_symbol(args.elf),args.read_khz,240)
    mem.descriptor();identity=mem.verify_app(args.elf);mem.running()
    listing=subprocess.check_output([str(NM),'-S','--defined-only',str(args.elf)],text=True)
    symbols={p[3]:(int(p[0],16),int(p[1],16)) for line in listing.splitlines() if len(p:=line.split())==4}
    address,size=symbols['g_photo_import']
    require(size==44 and 0x20000000<=address<=0x20030000-size and not address%4,'Invalid PhotoImport symbol')
    board=Board(args.serial,out,100);board.read_frequency_khz=args.read_khz;counter=0
    def state():
        nonlocal counter
        counter+=1;path=out/f'photo-{counter:04d}.bin'
        board.command(path.stem,'-u',hex(address),hex(size),str(path))
        words=struct.unpack('<11I',path.read_bytes())
        require(words[:2]==(0x50494D31,3),'Only Product ABI3 fixed-file photos are supported')
        return words
    b=state();require(b[8]==b[10] and b[3]==131072,'Photo worker busy or ABI mismatch')
    require(0xC0010000<=b[2]<=0xC4000000-len(data),'Input arena unavailable')
    board.command('upload-photo','-w',str(payload_path),hex(b[2]))
    require(mem.upload('input-readback',b[2],len(data)).read_bytes()==data,'SDRAM input differs')
    latest=state();require(latest==b,'Photo worker changed during upload')
    seq=(b[8]+1)&0xffffffff or 1
    report=dict(state='requested',slot=args.slot,input_sha256=sha(data),identity=identity,request=seq,
                filesystem_metadata_write=False,app_write=False)
    def save(): (out/'result.json').write_text(json.dumps(report,indent=2))
    save()
    board.command('replace-request','-w32',hex(address+16),hex(args.slot),hex(len(data)),hex(zlib.crc32(data)),hex(0x42414B32),'-w32',hex(address+32),hex(seq))
    deadline=time.monotonic()+180
    try:
        while True:
            b=state()
            if b[10]==seq:break
            require(time.monotonic()<deadline,'Request timed out; do not overwrite the busy input arena')
            time.sleep(.2)
        require(b[9]==0,f'PhotoStore failed: {b[9]:#x}')
        mem.running();report['state']='firmware_verified_commit';report['rendering_verified']=False;save()
    except BaseException as e:
        report.update(state='failed',error=str(e));save();raise
    print('CFWPIC commit complete; firmware verified physical readback and full JPEG decode.')


if __name__=='__main__':main()
