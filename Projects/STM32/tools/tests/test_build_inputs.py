from pathlib import Path
import sys,tempfile,unittest,os
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from build_inputs import fingerprint

class Inputs(unittest.TestCase):
 def test_included_c_incbin_and_nested_header_content(self):
  with tempfile.TemporaryDirectory() as directory:
   root=Path(directory);inc=root/'inc';inc.mkdir();(inc/'nested').mkdir()
   source=root/'main.c';source.write_text('#include "shared.c"\n')
   shared=inc/'shared.c';shared.write_text('old implementation')
   header=inc/'nested/header.h';header.write_text('old header')
   binary=root/'stock.bin';binary.write_bytes(b'old stock')
   asm=root/'stock.s';asm.write_text('.incbin "stock.bin"\n')
   inputs=[source,asm];last=fingerprint(inputs,[inc])
   self.assertEqual(last,fingerprint(inputs,[inc]))
   for path,content in ((shared,b'new implementation'),(header,b'new header'),(binary,b'new stock')):
    before=path.stat();path.write_bytes(content)
    os.utime(path,ns=(before.st_atime_ns,before.st_mtime_ns))
    current=fingerprint(inputs,[inc]);self.assertNotEqual(last,current);last=current
   binary.unlink()
   with self.assertRaises(FileNotFoundError):fingerprint(inputs,[inc])
if __name__=='__main__':unittest.main()
