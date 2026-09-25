"""Create a private, pinned raw-DEFLATE stock APP asset; never contact hardware."""
from pathlib import Path
import argparse,hashlib,json,zlib,sys
APP_SHA='162faeecc64bf56d828570168785e66d1a7da0152ef912bf83f9a19094d91edf'
FULL_SHA='38207bed741a8fef9e6d2ed24b86c8cb8b376ba5e4c957163f5edd83fbeb8037'
def pack(source:Path,out:Path):
    raw=source.read_bytes()
    if len(raw)==0x80000:
        if hashlib.sha256(raw).hexdigest()!=FULL_SHA:raise ValueError('Unapproved full donor dump')
        raw=raw[0x10000:]
    if len(raw)!=0x70000 or hashlib.sha256(raw).hexdigest()!=APP_SHA:raise ValueError('Unapproved stock APP')
    # Host-only exhaustive DEFLATE encoder. The target still uses the same
    # 32KiB-window tinfl decoder and the exact, pinned stock APP bytes.
    # Cache only validated output: no donor data is sent to an online service.
    host_tools=Path(__file__).resolve().parents[3]/'.tools'
    sys.path.insert(0,str(host_tools/'build-python'))
    import zopfli,zopfli.zlib
    if zopfli.__version__!='0.4.3':raise ValueError('Install pinned host zopfli==0.4.3')
    cache=host_tools/'stock-deflate-cache'/f'{APP_SHA}-zopfli-0.4.3-i15.deflate'
    compressed=cache.read_bytes() if cache.exists() else None
    if compressed is None:
        compressed=zopfli.zlib.compress(raw,numiterations=15)[2:-4]
        if zlib.decompress(compressed,-15)!=raw:raise ValueError('Deflate verification failed')
        cache.parent.mkdir(parents=True,exist_ok=True)
        temp=cache.with_suffix('.tmp');temp.write_bytes(compressed);temp.replace(cache)
    if zlib.decompress(compressed,-15)!=raw:raise ValueError('Deflate verification failed')
    out.mkdir(parents=True,exist_ok=True)
    (out/'stock.deflate').write_bytes(compressed)
    # .incbin keeps the private donor bytes out of checked-in C sources.
    asm='.section .rodata.bootstrap_stock,"a",%progbits\n.balign 4\n.global g_bootstrap_stock_deflate\ng_bootstrap_stock_deflate:\n.incbin "'+(out/'stock.deflate').resolve().as_posix()+'"\n.balign 4\n.global g_bootstrap_stock_deflate_bytes\ng_bootstrap_stock_deflate_bytes:\n.word '+str(len(compressed))+'\n'
    (out/'stock_asset.s').write_text(asm,encoding='utf8')
    report=dict(stock_app_sha256=APP_SHA,bytes=len(raw),compressed_bytes=len(compressed),compressed_sha256=hashlib.sha256(compressed).hexdigest(),encoder='zopfli0.4.3/raw-deflate/iterations15')
    (out/'stock_asset.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    return report
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('source',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();print(json.dumps(pack(a.source,a.output),indent=2))
