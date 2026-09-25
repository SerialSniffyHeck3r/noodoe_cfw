"""Exercise trace correlation, tick wrap, ABI rejection and read-only transport."""
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import bt_startup_read as host


def fixture():
    words=[0]*118
    words[:6]=[host.MAGIC,1,6,1,2,1]
    words[6:13]=[0xFFFFFFFE,1,0x1D00,2,0xC0,0,0]
    words[13:20]=[2,7,0x1D00,2,0xC0,1,3]
    return words


class Tests(unittest.TestCase):
    def test_wrap_and_levels(self):
        result=host.decode(struct.pack('<118I',*fixture()))
        self.assertEqual(result['samples'][1]['elapsed_ms'],4)
        self.assertTrue(result['samples'][1]['cts_pa11_high'])
        self.assertTrue(result['samples'][1]['enable_pi1_high'])

    def test_bad_abi_and_uncommitted(self):
        for index,value in ((0,0),(1,2),(2,0),(2,7),(3,0),(4,17),(5,8),(14,10),(13,0xFFFFFFFD)):
            with self.subTest(index=index,value=value):
                words=fixture();words[index]=value
                with self.assertRaises(RuntimeError):host.decode(struct.pack('<118I',*words))

    def test_torn_snapshot_retried(self):
        words=fixture();raw=struct.pack('<118I',*words)
        class Board:
            mailbox=0x20002000
            def __init__(self):self.items=iter([struct.pack('<I',4),raw,struct.pack('<I',6),struct.pack('<I',6),raw,struct.pack('<I',6)])
            def dump(self,*args):return next(self.items)
        self.assertEqual(host.stable_read(Board())['sequence'],6)

    def test_read_only_rejects_any_write(self):
        with tempfile.TemporaryDirectory() as folder:
            board=host.AmbientBoard('STLINK_SERIAL_REQUIRED',Path(folder),0x20002000,host.BYTES,(),950)
            board.identity_verified=True
            with patch('subprocess.run',side_effect=AssertionError('Must not reach hardware')):
                for args in (('-w32','0x20002000','1'),('-w32','0x40020014','0'),('-rst',),('-halt',),('-e','all')):
                    with self.assertRaises(RuntimeError):board.command('forbidden',*args)


if __name__=='__main__':unittest.main()
