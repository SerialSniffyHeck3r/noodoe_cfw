"""Conservative content fingerprint for the standalone embedded build cache.

Shared implementation includes and assembler incbin are build inputs too.
This intentionally invalidates every object when a project header changes:
a small extra compile cost is preferable to a stale recovery executable.
"""
from pathlib import Path
import hashlib,re

_QUOTED = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.MULTILINE)
_BINARY = re.compile(r'^\s*\.incbin\s*"([^"]+)"', re.MULTILINE)

def fingerprint(sources, includes):
    includes=sorted(Path(p).resolve() for p in includes)
    paths={Path(p).resolve() for p in sources}
    for directory in includes:
        if directory.is_dir():
            paths.update(p.resolve() for p in directory.rglob('*.h'))
    pending=list(paths)
    while pending:
        path=pending.pop()
        if path.suffix.lower() not in ('.c','.h','.s','.inc'):continue
        text=path.read_text(encoding='utf-8',errors='replace')
        for name in _QUOTED.findall(text):
            # Inactive conditional includes may be unavailable on this target.
            # GCC remains responsible for diagnosing active missing includes.
            found=next((p.resolve() for p in [path.parent/name,*[d/name for d in includes]] if p.is_file()),None)
            if found is not None and found not in paths:paths.add(found);pending.append(found)
        for name in _BINARY.findall(text):
            binary=Path(name)
            if not binary.is_absolute():binary=path.parent/binary
            binary=binary.resolve()
            if not binary.is_file():raise FileNotFoundError(binary)
            paths.add(binary)
    digest=hashlib.sha256(b'noodoe-build-inputs-v2\0')
    for path in sorted(paths):
        data=path.read_bytes()
        digest.update(str(path).encode());digest.update(b'\0')
        digest.update(len(data).to_bytes(8,'little'));digest.update(data)
    return digest.hexdigest()
