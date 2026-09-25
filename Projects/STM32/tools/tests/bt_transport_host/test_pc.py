"""No-radio unit checks for Windows ABI, socket lifetime and auth boundaries."""
import ctypes as C,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import bluetooth_pc as bt

class Fake:
    def __init__(self):
        self.ws=self;self.bt=self;self.sent=[];self.closed=[];self.services=[];self.error=10060
        self.callback_type=C.WINFUNCTYPE(bt.BOOL,C.c_void_p,C.POINTER(bt.AuthParams))
        self.callback=None;self.responses=[];self.registered=False
    def check(self,value,name):
        if value==-1:raise OSError(self.error,name)
        return value
    def socket(self,*args):return 123
    def setsockopt(self,*args):return 0
    def connect(self,sock,address,size):
        self.target=bt.SockAddrBth.from_buffer_copy(C.string_at(address,size));return 0
    def closesocket(self,handle):self.closed.append(handle);return 0
    def send(self,handle,data,count,flags):
        count=min(count,3);self.sent.append(C.string_at(data,count));return count
    def recv(self,*args):return -1
    def WSAGetLastError(self):return self.error
    def bind(self,*args):return 0
    def getsockname(self,handle,address,size):
        p=C.cast(address,C.POINTER(bt.SockAddrBth));p.contents.address=0x112233445566;p.contents.port=9;return 0
    def listen(self,*args):return 0
    def WSASetServiceW(self,query,operation,flags):
        q=C.cast(query,C.POINTER(bt.QuerySet)).contents
        self.services.append((operation,q.namespace,q.address_count,q.name,q.addresses.contents.protocol))
        return 0
    def BluetoothRegisterForAuthenticationEx(self,device,handle,callback,context):
        self.callback=callback;C.cast(handle,C.POINTER(bt.HANDLE)).contents.value=33;self.registered=True;return 0
    def BluetoothUnregisterAuthentication(self,handle):self.registered=False;return 1
    def BluetoothSendAuthenticationResponseEx(self,radio,response):
        self.responses.append(bt.AuthResponse.from_buffer_copy(C.string_at(response,C.sizeof(bt.AuthResponse))));return 0

class TestWindowsSPP(unittest.TestCase):
    def test_sdk_abi(self):
        self.assertEqual(C.sizeof(bt.SockAddrBth),30)
        self.assertEqual(bt.SockAddrBth.address.offset,2)
        self.assertEqual(bt.SockAddrBth.service.offset,10)
        self.assertEqual(bt.SockAddrBth.port.offset,26)
        self.assertEqual(C.sizeof(bt.DeviceInfo),560)
        self.assertEqual(C.sizeof(bt.AuthParams),576)
        self.assertEqual(C.sizeof(bt.AuthResponse),48)
        self.assertEqual(C.sizeof(bt.QuerySet),120 if C.sizeof(C.c_void_p)==8 else 60)
    def test_address_parse(self):
        self.assertEqual(bt.address_text(bt.address_number('01:23:45:67:89:ab')),'01:23:45:67:89:AB')
        for value in ['123','0:1:2:3:4:5','00:00:00:00:00:zz']:
            with self.assertRaises(ValueError):bt.address_number(value)
    def test_partial_send_and_sdp(self):
        api=Fake()
        with bt.BluetoothSocket('01:02:03:04:05:06',api=api) as client:
            client.sendall(b'0123456789')
            self.assertEqual(api.target.address,0x010203040506)
            self.assertEqual(api.target.port,0)
            self.assertEqual(bytes(api.target.service),bytes(bt.GUID.parse(bt.SPP_UUID)))
        self.assertEqual(b''.join(api.sent),b'0123456789');self.assertEqual(api.closed,[123])
    def test_timeout_and_error(self):
        api=Fake()
        with bt.BluetoothSocket(api=api) as client:
            with self.assertRaises(TimeoutError):client.recv(16)
            api.error=10054
            with self.assertRaises(OSError):client.recv(16)
    def test_service_registration_lifetime(self):
        api=Fake()
        with self.assertRaisesRegex(ValueError,'test'):
            with bt.spp_server(api,'simulator') as (server,info):
                self.assertEqual(info['channel'],9);self.assertEqual(info['address'],'11:22:33:44:55:66')
                raise ValueError('test')
        self.assertEqual(api.services,[(0,16,1,'simulator',3),(2,16,1,'simulator',3)])
        self.assertEqual(api.closed,[123])
    def test_auth_target_and_no_io(self):
        api=Fake();params=bt.AuthParams();params.device.address=0x112233445566;params.method=3;params.io_capability=3
        with bt.authentication(api,'11:22:33:44:55:66',just_works=True):
            self.assertEqual(api.callback(None,C.byref(params)),1)
            self.assertEqual(api.responses[-1].negative,0)
            params.io_capability=1;params.value=123456
            api.callback(None,C.byref(params));self.assertEqual(api.responses[-1].negative,1)
            params.device.address=0x112233445577
            self.assertEqual(api.callback(None,C.byref(params)),0);self.assertEqual(len(api.responses),2)
        self.assertFalse(api.registered)
    def test_auth_numeric_explicit(self):
        api=Fake();params=bt.AuthParams();params.device.address=0x112233445566;params.method=3;params.io_capability=1;params.value=123456
        with bt.authentication(api,'11:22:33:44:55:66',confirm=123456):
            api.callback(None,C.byref(params));self.assertEqual(api.responses[-1].negative,0)
    def test_simulated_protocols(self):
        self.assertEqual(bt.nmea_sentence('GPGLL,1'),b'$GPGLL,1*4D\r\n')
        self.assertEqual(bt.elm_response('01 0d'),'41 0D 2A\r>'.encode())
        self.assertEqual(bt.elm_response('0999'),b'?\r>')
        self.assertEqual(bt.elm_response('ATSP0'),b'OK\r>')

if __name__=='__main__':unittest.main()
