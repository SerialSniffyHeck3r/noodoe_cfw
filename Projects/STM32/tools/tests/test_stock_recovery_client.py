"""Same-session restore and ambiguous ACK tests, no radio."""
import importlib.util
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
import stock_recovery_client as recovery
from stock_installer import Journal
INSTALLABLE = SimpleNamespace(require_installable=lambda: None)


class Tests(unittest.TestCase):
    def test_confirm_disconnect_is_unverified_and_no_replay(self):
        class Peer:
            def __init__(self): self.calls = []
            def request(self, op, payload=b''):
                self.calls.append((op, payload))
                if op == 0x1f: return b''
                action = struct.unpack_from('<I', payload)[0]
                if action == 0: raise ConnectionError('reboot or lost link')
                return struct.pack('<7I', 2 if action == 1 else 6, 0, 0, 0x70000, 0, 3, 0)
        with tempfile.TemporaryDirectory() as directory:
            j = Journal(Path(directory)/'journal.json','AA','bundle'); peer=Peer()
            with patch.object(recovery,'identity',return_value=[1,2,3]), patch.object(recovery,'backup_proof',return_value='ab'*32), patch.object(recovery.time,'sleep'):
                result=recovery.restore(peer,INSTALLABLE,j,None,None,None)
            self.assertEqual(result['state'],'RECOVERY_RESULT_UNKNOWN_BOOT_UNVERIFIED')
            self.assertEqual([op for op,p in peer.calls],[0x1f,0x48,0x48,0x48])
            with self.assertRaises(ValueError): recovery.restore(peer,INSTALLABLE,j,None,None,None)

    def test_lost_confirm_preserves_unknown_before_transmit(self):
        class Peer:
            def request(self,op,payload=b''):
                if op==0x1f:return b''
                if struct.unpack_from('<I',payload)[0]==2:raise TimeoutError('lost ACK')
                return struct.pack('<7I',2,0,0,0x70000,0,3,0)
        with tempfile.TemporaryDirectory() as directory:
            j=Journal(Path(directory)/'journal.json','AA','bundle')
            with patch.object(recovery,'identity',return_value=[1,2,3]),patch.object(recovery,'backup_proof',return_value='ab'*32):
                with self.assertRaises(TimeoutError):recovery.restore(Peer(),INSTALLABLE,j,None,None,None)
            self.assertEqual(j.data['state'],'RECOVERY_COMMIT_RESULT_UNKNOWN')

    def test_port_preserves_fragmented_receives(self):
        class Socket:
            def recv(self,n):return b'abcdef'
            def sendall(self,data):self.sent=data
        socket=Socket();port=recovery.SocketPort(socket)
        self.assertEqual(port.read(2),b'ab');self.assertEqual(port.read(3),b'cde');self.assertEqual(port.read(3),b'f')
        self.assertEqual(port.write(b'ok'),2);self.assertEqual(socket.sent,b'ok')


if __name__=='__main__':unittest.main()
