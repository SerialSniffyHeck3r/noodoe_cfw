"""Offline layout2 installer package. Never bypasses ELF budgets or target audit."""
from pathlib import Path
import argparse, hashlib, json, struct, sys, zipfile

ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT/'STM32'
sys.path.insert(0, str(PROJECT/'tools'))
import recovery_bundle as legacy
import gate_bundle
from validate_image import Elf32, validate, require, SHF_ALLOC, SHT_NOBITS
from memory_report import report as memory_report

def inspect(args):
    expected = legacy.resource_id(PROJECT/'Middlewares/Noodoe/Resources/inc/Resources_Expected.h')
    stock, stock_report = legacy.stock_source(args.stock)
    resources = legacy.resources(args.resources, expected)
    bootstrap, bootstrap_report = legacy.build_artifact(args.bootstrap_elf,
        args.bootstrap_bin, 'bootstrap', expected)
    budgets = [memory_report(args.product, 'Product', 'Release'),
               memory_report(args.debug, 'Product', 'Debug')]
    for budget in budgets:
        require(budget['budget_passed'], 'Product budget failed: '+repr(budget['failures']))
    product, product_report = validate(Elf32(args.product.read_bytes()))
    require(product_report['layout_version'] == 2, 'Product must be layout2')
    require(product[0x230:0x240] == struct.pack('<4I',0x3250554e,2,2,0x60000),
            'Product lacks the v2 trial/rollback contract')
    require(product[0x200:0x22c] == struct.pack('<III', 0x51534352, 1, 1)+expected,
            'Product requires another resource package')
    combined = gate_bundle.gate_image(args.gate)+product.ljust(0x60000, b'\xff')
    if args.identity:
        info, evidence = legacy.read_identity(args.identity)
        scope = 'observed'
    else:
        info, evidence = legacy.bench_identity(args.bench_identity, args.stock)
        scope = 'bench-only'
    legacy.compatible(info)  # Exact observed profiles; capture0.15 BL on first Bootstrap connection.
    images = dict(bootstrap=bootstrap, cfw=combined, stock=stock, resources=resources)
    uninstall=Elf32(args.uninstall_elf.read_bytes()); rebuilt=bytearray(b'\xff'*0x70000)
    for ph in uninstall.programs:
        if ph['type']!=1 or not ph['filesz']: continue
        a,n=ph['paddr'],ph['filesz']
        require(0x08010000<=a and a+n<=0x08080000,'Uninstall load outside APP')
        require(a+n<=0x08011000 or a>=0x08020000,'Uninstall overwrites journal')
        rebuilt[a-0x08010000:a-0x08010000+n]=uninstall.data[ph['offset']:ph['offset']+n]
    data=args.uninstall_bin.read_bytes();require(data==rebuilt,'Uninstall BIN differs from ELF')
    require(data[0x200:0x220]==struct.pack('<8I',0x31424e55,1,0x08010000,0x70000,0x08011000,0xf000,0x00100005,0),'Uninstall contract')
    count=struct.unpack('<I',uninstall.code_bytes(uninstall.symbol('g_bootstrap_stock_deflate_bytes')['value'],4))[0]
    original=__import__('zlib').decompress(uninstall.code_bytes(uninstall.symbol('g_bootstrap_stock_deflate')['value'],count),-15)
    require(original==stock,'Uninstall embedded stock differs')
    import re
    pin=(PROJECT/'Middlewares/Noodoe/Update/inc/Uninstall_Expected.h').read_text()
    digest=bytes(int(x,16) for x in re.findall(r'0x([0-9a-f]{2})\b',pin))
    require(digest==bytes.fromhex(legacy.sha(data)) and digest in product,'Product uninstall pin differs')
    images['uninstall']=data
    diagnostic=Elf32(args.diagnostic_elf.read_bytes()); dimage=bytearray(b'\xff'*0x60000)
    # objcopy --gap-fill=0xFF exports allocatable sections, not the ELF's
    # zero-filled segment alignment holes (vector/descriptor gaps included).
    # Reconstruct those same sections and still validate every flash boundary.
    sections=[]
    for section in diagnostic.sections:
        if not section['flags']&SHF_ALLOC or not section['size'] or section['type']==SHT_NOBITS:continue
        a=diagnostic.section_lma(section);data=diagnostic.section_data(section)
        require(0x08020000<=a and a+len(data)<=0x08080000,'Diagnostic load crosses Gate/Product boundary')
        sections.append((a,data))
    end=0x08020000
    for a,data in sorted(sections):
        require(a>=end,'Overlapping Diagnostic sections');end=a+len(data)
        dimage[a-0x08020000:end-0x08020000]=data
    sp,entry=struct.unpack_from('<II',dimage)
    require(sp==0x2002ff00 and entry&1 and 0x08020000<=entry<0x08080000,'Diagnostic vector differs')
    require((diagnostic.entry&~1)==(entry&~1),'Diagnostic ELF entry differs from vector')
    require(args.diagnostic_bin.read_bytes()==dimage,'Diagnostic BIN/ELF mismatch')
    require(dimage[0x200:0x22c]==struct.pack('<III',0x51534352,1,0)+bytes(32),'Diagnostic requires external assets')
    require(dimage[0x230:0x240]==struct.pack('<4I',0x3250554e,2,3,0x60000),'Diagnostic role mismatch')
    require(combined[0x200:0x210]==struct.pack('<4I',0x31544647,1,1,0xfffffffe),'Gate lacks typed Diagnostic support')
    images['diagnostic']=bytes(dimage)
    manifest = {'format':'NOODOE_INSTALLER_2', 'layout.version':'2',
        'app.base':'0x08010000', 'app.bytes':'0x70000', 'target.scope':scope,
        # Identity observation alone is not no-SWD lifecycle qualification.
        'target.deployment':'bootstrap-diagnostics' if scope=='observed' else 'blocked-bench-only',
        'target.boot.binding':'device-capture' if info['boot_minor']==15 else 'pinned-capture',
        'target.hardware':str(info['hardware']), 'target.boot.major':str(info['boot_major']),
        'target.boot.minor':str(info['boot_minor']), 'target.boot.sha256':'capture-on-bootstrap' if info['boot_minor']==15 else legacy.BL_SHA,
        'target.stock.major':'5', 'target.stock.minor':'16',
        'target.model':info['model'], 'target.pcba':info['pcba'], 'diagnostic.version':'1', 'diagnostic.hardware-tested':'false', 'uninstall.version':'7', 'uninstall.hardware-tested':'false'}
    for role, data in images.items():
        manifest[role+'.file']=role+'.bin'; manifest[role+'.sha256']=legacy.sha(data)
        if role in legacy.VERSIONS:
            major, minor = legacy.VERSIONS[role]
            manifest[role+'.major']=str(major); manifest[role+'.minor']=str(minor)
    audit = dict(format='NOODOE_INSTALLER_AUDIT_2', hardware_access=False,
        wireless_verified=False, installable=scope=='observed', qualification='bootstrap-diagnostics-not-no-swd-qualified', budgets=budgets,
        remaining_hardware_tests=['healthy-radio no-SWD lifecycle', 'interrupted stock BL install/restore'],
        bootstrap=bootstrap_report, stock=stock_report, product=product_report,
        target=info, evidence=evidence, manifest=manifest)
    return images, manifest, audit

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('stock','bootstrap-elf','bootstrap-bin','gate','product','debug','uninstall-elf','uninstall-bin','diagnostic-elf','diagnostic-bin'):
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--resources', type=Path, default=PROJECT/'Resources/NOODOE.RSC')
    target=p.add_mutually_exclusive_group(required=True)
    target.add_argument('--identity',type=Path)
    target.add_argument('--bench-identity',type=Path)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    try:
        images, manifest, audit = inspect(a)
        temporary=a.output/'installer.zip.partial'
        with zipfile.ZipFile(temporary,'w') as z:
            files={'manifest.properties':''.join(k+'='+v+'\n' for k,v in manifest.items()).encode()}
            files.update({k+'.bin':v for k,v in images.items()})
            for name,data in files.items():
                e=zipfile.ZipInfo(name,(2026,1,1,0,0,0));e.compress_type=zipfile.ZIP_DEFLATED
                e.external_attr=0o600<<16;z.writestr(e,data,compresslevel=9)
        with zipfile.ZipFile(temporary) as z:
            require(z.testzip() is None,'ZIP readback failed')
            require(all(z.read(n)==d for n,d in files.items()),'ZIP bytes changed')
        audit['bundle_sha256']=hashlib.sha256(temporary.read_bytes()).hexdigest()
        temporary.rename(a.output/'installer.zip')
    except Exception as error:
        (a.output/'audit.json').write_text(json.dumps(dict(installable=False,error=str(error)),indent=2),encoding='utf-8')
        raise
    (a.output/'audit.json').write_text(json.dumps(audit,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(audit,indent=2))

if __name__=='__main__':main()
