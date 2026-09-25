"""Windows sharing violations in status output must not stop the UART worker."""
from pathlib import Path
import importlib.util,unittest
from unittest.mock import patch
spec=importlib.util.spec_from_file_location('dashboard_uart',Path(__file__).resolve().parents[1]/'dashboard_uart.py')
peer=importlib.util.module_from_spec(spec);spec.loader.exec_module(peer)
class Logging(unittest.TestCase):
    def test_busy_destination_retries_next_report(self):
        with patch.object(Path,'write_text') as write,patch.object(Path,'replace',side_effect=[PermissionError(5,'busy'),None]) as replace:
            before=peer.log_write_failures
            self.assertFalse(peer.write_json('unused-status.json',{'running':True}))
            self.assertEqual(peer.log_write_failures,before+1)
            self.assertTrue(peer.write_json('unused-status.json',{'running':True}))
            self.assertEqual(write.call_count,2);self.assertEqual(replace.call_count,2)
    def test_full_disk_does_not_disrupt_transport(self):
        with patch.object(Path,'write_text',side_effect=OSError('full')),patch.object(Path,'replace') as replace:
            self.assertFalse(peer.write_json('unused-status.json',{}));replace.assert_not_called()
if __name__=='__main__':unittest.main()
