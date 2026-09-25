"""Windows Classic Bluetooth SPP CLI, using WinSock/Bluetooth APIs, no GUI.

Also importable: BluetoothSocket(address, channel=0, timeout=10) provides
sendall/recv/close/context-manager for the firmware/control protocol CLI.
Service registration uses NS_BTH + WSASetService, not a bare RFCOMM listener.
Reference ABI: Microsoft win32metadata ws2bth.h (SOCKADDR_BTH is packed1),
bluetoothapis.h and WinSock2 WSAQUERYSETW. No Python Bluetooth package needed.
"""
from __future__ import annotations
import argparse,ctypes as C,json,os,socket,sys,time,uuid
from contextlib import contextmanager
from pathlib import Path

U8=C.c_uint8;U16=C.c_uint16;U32=C.c_uint32;U64=C.c_uint64;BOOL=C.c_int
HANDLE=C.c_void_p;SOCKET=C.c_size_t;INVALID_SOCKET=C.c_size_t(-1).value
SPP_UUID='00001101-0000-1000-8000-00805f9b34fb'
class GUID(C.Structure):
    _fields_=[('d1',U32),('d2',U16),('d3',U16),('d4',U8*8)]
    @classmethod
    def parse(cls,value):return cls.from_buffer_copy(uuid.UUID(value).bytes_le)
class SockAddrBth(C.Structure):
    _pack_=1
    _fields_=[('family',U16),('address',U64),('service',GUID),('port',U32)]
class SystemTime(C.Structure):_fields_=[('fields',U16*8)]
class DeviceInfo(C.Structure):
    _fields_=[('size',U32),('address',U64),('device_class',U32),('connected',BOOL),
              ('remembered',BOOL),('authenticated',BOOL),('last_seen',SystemTime),
              ('last_used',SystemTime),('name',C.c_wchar*248)]
class SearchParams(C.Structure):
    _fields_=[('size',U32),('authenticated',BOOL),('remembered',BOOL),('unknown',BOOL),
              ('connected',BOOL),('inquiry',BOOL),('multiplier',U8),('radio',HANDLE)]
class AuthParams(C.Structure):
    _fields_=[('device',DeviceInfo),('method',U32),('io_capability',U32),('requirements',U32),('value',U32)]
class PinInfo(C.Structure):_fields_=[('pin',U8*16),('length',U8)]
class AuthValue(C.Union):_fields_=[('pin',PinInfo),('oob',U8*32),('numeric',U32)]
class AuthResponse(C.Structure):
    _fields_=[('address',U64),('method',U32),('value',AuthValue),('negative',U8)]
class SocketAddress(C.Structure):_fields_=[('address',C.c_void_p),('length',C.c_int)]
class CSAddr(C.Structure):
    _fields_=[('local',SocketAddress),('remote',SocketAddress),('socket_type',C.c_int),('protocol',C.c_int)]
class QuerySet(C.Structure):
    _fields_=[('size',U32),('name',C.c_wchar_p),('service',C.POINTER(GUID)),('version',C.c_void_p),
              ('comment',C.c_wchar_p),('namespace',U32),('ns_provider',C.c_void_p),('context',C.c_wchar_p),
              ('protocol_count',U32),('protocols',C.c_void_p),('query',C.c_wchar_p),
              ('address_count',U32),('addresses',C.POINTER(CSAddr)),('output_flags',U32),('blob',C.c_void_p)]

def address_number(value:str)->int:
    text=value.replace(':','').replace('-','')
    if len(text)!=12 or any(c not in '0123456789abcdefABCDEF' for c in text):raise ValueError('Bluetooth address must contain exactly six hex octets')
    return int(text,16)
def address_text(value:int)->str:
    return ':'.join(f'{(value>>shift)&255:02X}' for shift in range(40,-1,-8))
def win_error(code:int,operation:str):
    raise OSError(code,f'{operation}: Windows error {code}')
def _signature(lib,name,args,result):
    fn=getattr(lib,name);fn.argtypes=args;fn.restype=result;return fn

class WindowsAPI:
    def __init__(self):
        if os.name!='nt':raise RuntimeError('This transport requires Windows')
        self.ws=C.WinDLL('ws2_32',use_last_error=True)
        self.bt=C.WinDLL('bthprops.cpl',use_last_error=True)
        self.callback_type=C.WINFUNCTYPE(BOOL,C.c_void_p,C.POINTER(AuthParams))
        _signature(self.ws,'WSAStartup',[U16,C.c_void_p],C.c_int)
        _signature(self.ws,'WSACleanup',[],C.c_int)
        _signature(self.ws,'WSAGetLastError',[],C.c_int)
        for name in ('socket',):_signature(self.ws,name,[C.c_int,C.c_int,C.c_int],SOCKET)
        for name in ('connect','bind'):_signature(self.ws,name,[SOCKET,C.c_void_p,C.c_int],C.c_int)
        _signature(self.ws,'closesocket',[SOCKET],C.c_int)
        _signature(self.ws,'send',[SOCKET,C.c_void_p,C.c_int,C.c_int],C.c_int)
        _signature(self.ws,'recv',[SOCKET,C.c_void_p,C.c_int,C.c_int],C.c_int)
        _signature(self.ws,'setsockopt',[SOCKET,C.c_int,C.c_int,C.c_void_p,C.c_int],C.c_int)
        _signature(self.ws,'getsockname',[SOCKET,C.c_void_p,C.POINTER(C.c_int)],C.c_int)
        _signature(self.ws,'listen',[SOCKET,C.c_int],C.c_int)
        _signature(self.ws,'accept',[SOCKET,C.c_void_p,C.POINTER(C.c_int)],SOCKET)
        _signature(self.ws,'WSASetServiceW',[C.POINTER(QuerySet),C.c_int,U32],C.c_int)
        _signature(self.bt,'BluetoothFindFirstDevice',[C.POINTER(SearchParams),C.POINTER(DeviceInfo)],HANDLE)
        _signature(self.bt,'BluetoothFindNextDevice',[HANDLE,C.POINTER(DeviceInfo)],BOOL)
        _signature(self.bt,'BluetoothFindDeviceClose',[HANDLE],BOOL)
        _signature(self.bt,'BluetoothAuthenticateDevice',[HANDLE,HANDLE,C.POINTER(DeviceInfo),C.c_wchar_p,U32],U32)
        _signature(self.bt,'BluetoothRegisterForAuthenticationEx',[C.POINTER(DeviceInfo),C.POINTER(HANDLE),self.callback_type,C.c_void_p],U32)
        _signature(self.bt,'BluetoothUnregisterAuthentication',[HANDLE],BOOL)
        _signature(self.bt,'BluetoothSendAuthenticationResponseEx',[HANDLE,C.POINTER(AuthResponse)],U32)
        wsa=C.create_string_buffer(512)
        code=self.ws.WSAStartup(0x0202,wsa)
        if code:win_error(code,'WSAStartup')
    def check(self,result,operation):
        if result==-1:win_error(self.ws.WSAGetLastError(),operation)
        return result
    def devices(self,inquiry=False):
        params=SearchParams(C.sizeof(SearchParams),1,1,1,1,int(inquiry),8,None)
        info=DeviceInfo();info.size=C.sizeof(info)
        handle=self.bt.BluetoothFindFirstDevice(C.byref(params),C.byref(info))
        if not handle:
            error=C.get_last_error()
            if error==259:return []
            win_error(error,'BluetoothFindFirstDevice')
        result=[]
        try:
            while True:
                result.append(dict(address=address_text(info.address),name=info.name,paired=bool(info.authenticated),
                                   remembered=bool(info.remembered),connected=bool(info.connected),device_class=info.device_class))
                if not self.bt.BluetoothFindNextDevice(handle,C.byref(info)):
                    error=C.get_last_error()
                    if error!=259:win_error(error,'BluetoothFindNextDevice')
                    break
        finally:self.bt.BluetoothFindDeviceClose(handle)
        return result
    def close(self):self.ws.WSACleanup()

@contextmanager
def authentication(api:WindowsAPI,address:str,*,just_works=False,pin=None,confirm=None):
    """Register only for the explicitly requested remote address. No wizard.
    Just Works is accepted only when the remote declares no input/output.
    Other numeric comparisons need the explicit matching --confirm value.
    """
    remote=address_number(address);info=DeviceInfo();info.size=C.sizeof(info);info.address=remote
    handle=HANDLE();events=[]
    @api.callback_type
    def callback(context,params):
        p=params.contents
        if p.device.address!=remote:return 0
        answer=AuthResponse();answer.address=remote;answer.method=p.method;answer.negative=1
        if p.method==1 and pin:
            encoded=pin.encode('ascii');answer.value.pin.length=len(encoded)
            for i,value in enumerate(encoded):answer.value.pin.pin[i]=value
            answer.negative=0
        elif p.method==3 and ((just_works and p.io_capability==3) or (confirm is not None and confirm==p.value)):
            answer.value.numeric=p.value;answer.negative=0
        elif p.method==4: # notification requires no outgoing response
            events.append(dict(method=p.method,notification=p.value));return 1
        code=api.bt.BluetoothSendAuthenticationResponseEx(None,C.byref(answer))
        events.append(dict(method=p.method,remote_io=p.io_capability,accepted=not bool(answer.negative),result=code))
        return int(code==0)
    code=api.bt.BluetoothRegisterForAuthenticationEx(C.byref(info),C.byref(handle),callback,None)
    if code:win_error(code,'BluetoothRegisterForAuthenticationEx')
    try:yield events
    finally:api.bt.BluetoothUnregisterAuthentication(handle)

class BluetoothSocket:
    """Blocking socket with bounded send/receive timeouts; channel0 uses SDP.
    Owns its WinSock startup reference. Do not share one socket between writers.
    """
    def __init__(self,address=None,channel=0,timeout=10,*,api=None,handle=None):
        self.api=api or WindowsAPI();self.owns_api=api is None;self.handle=None
        try:
            self.handle=handle if handle is not None else self.api.ws.socket(32,1,3)
            if self.handle==INVALID_SOCKET:win_error(self.api.ws.WSAGetLastError(),'socket(AF_BTH)')
            self.settimeout(timeout)
            if address:
                if not 0<=channel<=30:raise ValueError('RFCOMM channel must be0..30')
                auth=U32(1)
                self.api.check(self.api.ws.setsockopt(self.handle,3,C.c_int(0x80000001).value,C.byref(auth),4),'SO_BTH_AUTHENTICATE')
                self.api.check(self.api.ws.setsockopt(self.handle,3,2,C.byref(auth),4),'SO_BTH_ENCRYPT')
                target=SockAddrBth(32,address_number(address),GUID.parse(SPP_UUID) if channel==0 else GUID(),channel)
                self.api.check(self.api.ws.connect(self.handle,C.byref(target),C.sizeof(target)),'RFCOMM connect')
        except BaseException:self.close();raise
    def settimeout(self,seconds):
        value=U32(max(1,int(seconds*1000)))
        for option in (0x1005,0x1006):self.api.check(self.api.ws.setsockopt(self.handle,0xffff,option,C.byref(value),4),'socket timeout')
    def sendall(self,data):
        data=bytes(data);offset=0
        while offset<len(data):
            chunk=C.create_string_buffer(data[offset:])
            count=self.api.check(self.api.ws.send(self.handle,chunk,len(data)-offset,0),'RFCOMM send')
            if count==0:raise ConnectionError('Remote disconnected while sending')
            offset+=count
    def recv(self,capacity):
        if not 1<=capacity<=1048576:raise ValueError('receive capacity outside1..1048576')
        data=C.create_string_buffer(capacity)
        count=self.api.ws.recv(self.handle,data,capacity,0)
        if count<0:
            code=self.api.ws.WSAGetLastError()
            if code in (10060,10035):raise TimeoutError('RFCOMM receive timed out')
            win_error(code,'RFCOMM recv')
        return data.raw[:count]
    def close(self):
        if self.handle is not None and self.handle!=INVALID_SOCKET:self.api.ws.closesocket(self.handle)
        self.handle=None
        if self.owns_api:self.api.close();self.owns_api=False
    def __enter__(self):return self
    def __exit__(self,*args):self.close()

@contextmanager
def spp_server(api:WindowsAPI,name:str):
    """Dynamically bind RFCOMM and register SPP SDP until the context exits."""
    server=BluetoothSocket(api=api,timeout=1)
    query=None
    try:
        local=SockAddrBth(32,0,GUID(),0xffffffff)
        api.check(api.ws.bind(server.handle,C.byref(local),C.sizeof(local)),'bind RFCOMM')
        size=C.c_int(C.sizeof(local));api.check(api.ws.getsockname(server.handle,C.byref(local),C.byref(size)),'getsockname')
        api.check(api.ws.listen(server.handle,1),'listen RFCOMM')
        service=GUID.parse(SPP_UUID)
        csa=CSAddr(SocketAddress(C.addressof(local),C.sizeof(local)),SocketAddress(None,0),1,3)
        query=QuerySet();query.size=C.sizeof(query);query.name=name;query.service=C.pointer(service)
        query.namespace=16;query.address_count=1;query.addresses=C.pointer(csa)
        api.check(api.ws.WSASetServiceW(C.byref(query),0,0),'register SDP')
        yield server,dict(address=address_text(local.address),channel=local.port,name=name,uuid=SPP_UUID)
    finally:
        if query is not None:api.ws.WSASetServiceW(C.byref(query),2,0)
        server.close()

def nmea_sentence(body:str)->bytes:
    checksum=0
    for byte in body.encode('ascii'):checksum^=byte
    return f'${body}*{checksum:02X}\r\n'.encode('ascii')
def elm_response(command:str)->bytes:
    command=command.strip().replace(' ','').upper()
    replies={'ATZ':'ELM327 v1.5','ATI':'ELM327 v1.5','ATDP':'AUTO, ISO 15765-4 (CAN 11/500)',
             'ATRV':'12.6V','0100':'41 00 BE 3E B8 13','010D':'41 0D 2A','010C':'41 0C 27 10',
             '0105':'41 05 7B','0111':'41 11 40'}
    if command in ('ATE0','ATE1','ATL0','ATL1','ATS0','ATS1','ATH0','ATH1','ATSP0'):answer='OK'
    else:answer=replies.get(command,'NO DATA' if command.startswith('01') else '?')
    return (answer+'\r>').encode('ascii')

def serve(args,api):
    remote=address_number(args.allow_address)
    with authentication(api,args.allow_address,just_works=args.just_works,pin=args.pin,confirm=args.confirm),spp_server(api,'Noodoe '+args.mode+' simulator') as (server,info):
        print(json.dumps(dict(event='listening',**info)),flush=True)
        # accept is intentionally an explicit server operation. Ctrl+C stops it;
        # duration bounds the session after accept, not Bluetooth pairing time.
        peer=SockAddrBth();size=C.c_int(C.sizeof(peer))
        handle=api.ws.accept(server.handle,C.byref(peer),C.byref(size))
        if handle==INVALID_SOCKET:win_error(api.ws.WSAGetLastError(),'accept')
        with BluetoothSocket(api=api,handle=handle,timeout=.25) as client:
            if peer.address!=remote:raise ConnectionError('Incoming address did not match --allow-address')
            print(json.dumps(dict(event='connected',address=address_text(peer.address))),flush=True)
            deadline=time.monotonic()+args.duration;next_gps=0;buffer=b'';rx=tx=0
            gps=Path(args.nmea_file).read_bytes() if args.nmea_file else nmea_sentence('GPRMC,120000.00,A,3730.0000,N,12700.0000,E,10.0,90.0,120926,,,A')+nmea_sentence('GPGGA,120000.00,3730.0000,N,12700.0000,E,1,08,0.9,30.0,M,0.0,M,,')
            while time.monotonic()<deadline:
                if args.mode=='nmea' and time.monotonic()>=next_gps:
                    client.sendall(gps);tx+=len(gps);next_gps=time.monotonic()+1
                try:data=client.recv(4096)
                except TimeoutError:continue
                if not data:break
                rx+=len(data)
                if args.mode=='echo':client.sendall(data);tx+=len(data)
                elif args.mode=='elm':
                    buffer+=data
                    if len(buffer)>4096:buffer=b'';client.sendall(b'?\r>');continue
                    while b'\r' in buffer:
                        command,buffer=buffer.split(b'\r',1);reply=elm_response(command.decode('ascii','replace'))
                        client.sendall(reply);tx+=len(reply)
            print(json.dumps(dict(event='finished',rx_bytes=rx,tx_bytes=tx)),flush=True)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='command',required=True)
    scan=sub.add_parser('discover');scan.add_argument('--inquiry',action='store_true')
    pair=sub.add_parser('pair');pair.add_argument('address');pair.add_argument('--pin',required=True,help='Explicit1..16 ASCII-byte legacy PIN; no wizard mode')
    conn=sub.add_parser('exchange');conn.add_argument('address');conn.add_argument('--channel',type=int,default=0)
    conn.add_argument('--hex',default='');conn.add_argument('--receive',type=int,default=4096);conn.add_argument('--timeout',type=float,default=5)
    conn.add_argument('--just-works',action='store_true');conn.add_argument('--pin');conn.add_argument('--confirm',type=int)
    server=sub.add_parser('serve');server.add_argument('--mode',choices=['echo','nmea','elm'],required=True)
    server.add_argument('--allow-address',required=True);server.add_argument('--duration',type=float,default=60)
    server.add_argument('--nmea-file');server.add_argument('--just-works',action='store_true');server.add_argument('--pin');server.add_argument('--confirm',type=int)
    args=parser.parse_args()
    if hasattr(args,'pin') and args.pin is not None:
        if not 1<=len(args.pin.encode('ascii'))<=16:parser.error('PIN must be1..16 ASCII bytes')
    api=WindowsAPI()
    try:
        if args.command=='discover':print(json.dumps(api.devices(args.inquiry),ensure_ascii=False,indent=2))
        elif args.command=='pair':
            info=DeviceInfo();info.size=C.sizeof(info);info.address=address_number(args.address)
            # Non-null PIN invokes documented blind mode. Never call Ex with
            # null OOB or this API with null PIN, both would launch a wizard.
            code=api.bt.BluetoothAuthenticateDevice(None,None,C.byref(info),args.pin,len(args.pin))
            if code:win_error(code,'BluetoothAuthenticateDevice blind pairing')
            print(json.dumps(dict(paired=True,address=args.address)))
        elif args.command=='exchange':
            with authentication(api,args.address,just_works=args.just_works,pin=args.pin,confirm=args.confirm) as events:
                with BluetoothSocket(args.address,args.channel,args.timeout,api=api) as client:
                    if args.hex:client.sendall(bytes.fromhex(args.hex))
                    try:data=client.recv(args.receive) if args.receive else b''
                    except TimeoutError:data=b''
                    print(json.dumps(dict(connected=True,received_hex=data.hex(),authentication=events)))
        elif args.command=='serve':serve(args,api)
    finally:api.close()
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,RuntimeError) as exc:
        print(json.dumps(dict(error=str(exc))),file=sys.stderr);sys.exit(1)
