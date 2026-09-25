"""NDCP v1 phone-role SPP control client; no device access without --port.

Examples: --port COM8 info | bt | capabilities | vehicle | gnss
          --port COM8 gps --latitude 37.5 --longitude 127.0
          --port COM8 authorize-update --backup-manifest <verified full A/B>
Authorization alone does not erase/program/install/reset. It is valid only for
the current PHONE link and must precede the independent updater protocol.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import secrets
import struct
import time
import zlib

HEADER=struct.Struct("<4sBBHIHH")
MAX_PAYLOAD=1024
ROLES={"phone":0,"elm":1,"phone2":2}


def encode(opcode:int,sequence:int,payload:bytes=b"",flags:int=0) -> bytes:
    if not 0<=opcode<=255 or len(payload)>MAX_PAYLOAD or flags&~3:
        raise ValueError("Invalid NDCP frame")
    packet=HEADER.pack(b"NDCP",1,opcode,flags,sequence,len(payload),0)+payload
    return packet+struct.pack("<I",zlib.crc32(packet))


class ControlError(RuntimeError):
    pass


class Client:
    def __init__(self,port,timeout:float=15):
        self.port,self.timeout=port,timeout
        self.sequence=secrets.randbits(32)

    def exact(self,length:int,deadline:float) -> bytes:
        result=bytearray()
        while len(result)<length:
            if time.monotonic()>=deadline:raise ControlError("NDCP response timed out")
            result.extend(self.port.read(length-len(result)))
        return bytes(result)

    def receive(self,deadline:float) -> tuple[int,int,int,bytes]:
        window=bytearray()
        while window!=b"NDCP":
            window.extend(self.exact(1,deadline))
            if len(window)>4:del window[0]
        header=b"NDCP"+self.exact(HEADER.size-4,deadline)
        _,version,opcode,flags,sequence,length,reserved=HEADER.unpack(header)
        if version!=1 or reserved or flags&~3 or length>MAX_PAYLOAD:
            raise ControlError("Invalid NDCP response header")
        payload=self.exact(length,deadline)
        crc=struct.unpack("<I",self.exact(4,deadline))[0]
        if zlib.crc32(header+payload)!=crc:raise ControlError("NDCP response CRC mismatch")
        return opcode,flags,sequence,payload

    def request(self,opcode:int,payload:bytes=b"") -> bytes:
        self.sequence=(self.sequence+1)&0xFFFFFFFF
        packet=encode(opcode,self.sequence,payload)
        if self.port.write(packet)!=len(packet):raise ControlError("Partial NDCP request write")
        deadline=time.monotonic()+self.timeout
        for _ in range(32):
            op,flags,sequence,response=self.receive(deadline)
            if sequence!=self.sequence:continue
            if op!=opcode or not flags&1 or len(response)<4:raise ControlError("Unexpected NDCP response")
            result=struct.unpack_from("<i",response)[0]
            if result or flags&2:raise ControlError(f"Device rejected opcode0x{opcode:02X}: result={result}, payload={response.hex()}")
            return response[4:]
        raise ControlError("Too many unrelated NDCP responses")

    def info(self) -> dict:
        payload=self.request(1)
        if len(payload)!=68:raise ControlError("INFO layout mismatch")
        words=struct.unpack_from("<9I",payload)
        return {"protocol":words[0],"uid_words":list(words[1:4]),
                "usb_serial":"".join(f"{v:08X}" for v in words[1:4]),
                "boot_metadata_words":list(words[4:9]),
                "build":payload[36:68].split(b"\0",1)[0].decode("ascii",errors="replace")}

    def authorize_update(self,manifest:Path) -> dict:
        # Reuse the USB backup proof logic. Both whole files are freshly read
        # and hashed before a UID-bound explicit1F authorization is transmitted.
        from storage_backup import verify_unlock_manifest
        identity=self.info()
        proof=verify_unlock_manifest(manifest,identity)
        self.request(0x1F,struct.pack("<4I",*identity["uid_words"],0x42414B32))
        return {"authorized":True,"scope":"Current PHONE connection only","identity":identity,"backup_proof":proof}


def address(text:str) -> bytes:
    try:result=bytes.fromhex(text.replace(":","").replace("-",""))
    except ValueError as error:raise argparse.ArgumentTypeError("Use six Bluetooth address octets") from error
    if len(result)!=6:raise argparse.ArgumentTypeError("Use six Bluetooth address octets")
    return result


def words(payload:bytes,count:int) -> list[int]:
    if len(payload)<count*4:raise ControlError("Truncated diagnostic snapshot")
    return list(struct.unpack_from(f"<{count}I",payload))


def signed(value:int) -> int:return value-(1<<32) if value&(1<<31) else value


def decode_snapshot(opcode:int,payload:bytes) -> dict:
    if opcode==0x0A:
        names="sequence valid_fields stale telemetry_ms age_ms speed_kph odometer_km fuel_observed payload2_raw status_raw status_high status_low temperature_candidate_c extended_raw extended_present frame_ms frame_command frame_length raw_length".split()
        out=dict(zip(names,words(payload,19)));out["temperature_candidate_c"]=signed(out["temperature_candidate_c"])
        if len(payload)!=76+out["raw_length"]:raise ControlError("Vehicle raw length mismatch")
        out["raw_hex"]=payload[76:].hex();return out
    if opcode==0x0B:
        data=words(payload,27);names="source external_connected valid stale age_ms fields sample_ms has_sample".split();out=dict(zip(names,data[:8]));out["field_ms"]=data[8:16]
        names="latitude_e7 longitude_e7 speed_mm_s course_mdeg utc_ms date_yyyymmdd altitude_mm satellites hdop_milli quality raw_length".split();out.update(zip(names,data[16:]))
        for name in ("latitude_e7","longitude_e7","altitude_mm"):out[name]=signed(out[name])
        if len(payload)!=108+out["raw_length"]:raise ControlError("GNSS raw length mismatch")
        out["raw"]=payload[108:].decode("ascii",errors="replace");return out
    if opcode==0x0C:
        data=words(payload,40);out=dict(zip("connected phase last_result last_pid last_result_ms valid_fields stale_fields".split(),data[:7]))
        out["value_ms"]=data[7:15];out["values"]=[signed(x) for x in data[15:23]];out["raw_values"]=data[23:31]
        out.update(zip("supported_01_20 support_known completed timeouts adapter_errors malformed overflows multiple_replies raw_length".split(),data[31:]))
        if len(payload)!=176+out["raw_length"]:raise ControlError("OBD raw length mismatch")
        out["command"]=payload[160:176].split(b"\0",1)[0].decode("ascii",errors="replace");out["raw"]=payload[176:].decode("ascii",errors="replace");return out
    raise ValueError("Not a snapshot opcode")


def decode_bt(payload:bytes) -> dict:
    if len(payload)!=208:raise ControlError("Bluetooth diagnostic layout mismatch")
    names="magic version state last_error heartbeat manufacturer lmp_subversion hci_revision patch_bytes baud commands command_rejected hci_errors pairings key_generation key_persisted_generation stack_low_words".split()
    out=dict(zip(names,words(payload,17)));out["local_address"]=payload[68:74].hex(":");out["discovered_count"]=payload[74];out["links"]={}
    if out["version"] not in (1,2):raise ControlError("Unknown Bluetooth role version")
    roles={"phone":0,"elm":1,"gps":2} if out["version"]==1 else ROLES
    for role,index in roles.items():
        start=76+index*44;data=payload[start:start+44];cid,mtu=struct.unpack_from("<HH",data,8)
        link={"address":data[:6].hex(":"),"status":data[6],"server_channel":data[7],"cid":cid,"mtu":mtu}
        link.update(zip("rx_bytes tx_bytes rx_overflow tx_rejected reconnects last_error rx_queued tx_queued".split(),struct.unpack_from("<8I",data,12)));out["links"][role]=link
    return out


def main(argv=None) -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port",required=True,help="Computer's paired SPP COM port to PHONE service")
    parser.add_argument("--timeout",type=float,default=15)
    sub=parser.add_subparsers(dest="command",required=True)
    for name in ("ping","info","bt","capabilities","discover","results","vehicle","gnss","obd","update-status"):sub.add_parser(name)
    connect=sub.add_parser("connect");connect.add_argument("--role",choices=("elm",),required=True);connect.add_argument("--address",type=address,required=True);connect.add_argument("--channel",type=int,default=0)
    disconnect=sub.add_parser("disconnect");disconnect.add_argument("--role",choices=tuple(ROLES),required=True)
    pair=sub.add_parser("pair");pair.add_argument("--address",type=address,required=True);pair.add_argument("--pin",default="1234")
    window=sub.add_parser("pairwindow");window.add_argument("--seconds",type=int,required=True)
    gps=sub.add_parser("gps");gps.add_argument("--latitude",type=float,required=True);gps.add_argument("--longitude",type=float,required=True);gps.add_argument("--speed-mm-s",type=int);gps.add_argument("--course-mdeg",type=int);gps.add_argument("--altitude-mm",type=int);gps.add_argument("--utc-ms",type=int);gps.add_argument("--date",type=int);gps.add_argument("--satellites",type=int);gps.add_argument("--hdop-milli",type=int);gps.add_argument("--quality",type=int,default=1)
    authorize=sub.add_parser("authorize-update");authorize.add_argument("--backup-manifest",type=Path,required=True)
    args=parser.parse_args(argv)
    if args.timeout<=0:parser.error("--timeout must be positive")
    import serial
    with serial.Serial(args.port,115200,timeout=0.2,write_timeout=args.timeout) as port:
        client=Client(port,args.timeout);command=args.command
        if command=="info":result=client.info()
        elif command=="authorize-update":result=client.authorize_update(args.backup_manifest)
        elif command=="bt":result=decode_bt(client.request(2))
        elif command=="capabilities":
            payload=client.request(0x0D)
            if len(payload)!=12:raise ControlError("Capability length mismatch")
            schema,phones,features=words(payload,3)
            result={"schema":schema,"max_phones":phones,"feature_bits":features,
                    "inbound_spp":bool(features&1),"phone_gps":bool(features&2),"swd_backup":bool(features&4)}
        elif command in ("vehicle","gnss","obd"):
            opcode={"vehicle":0x0A,"gnss":0x0B,"obd":0x0C}[command];result=decode_snapshot(opcode,client.request(opcode))
        elif command=="results":
            payload=client.request(4);count=words(payload,1)[0]
            if count>12 or len(payload)!=4+count*12:raise ControlError("Discovery result length mismatch")
            result=[]
            for i in range(count):
                p=payload[4+i*12:16+i*12];result.append({"address":p[:6].hex(":"),"rssi":struct.unpack_from("<b",p,6)[0],"class_of_device":struct.unpack_from("<I",p,8)[0]})
        elif command=="connect":
            if not 0<=args.channel<=30:parser.error("SPP channel must be0..30")
            client.request(5,bytes([ROLES[args.role],args.channel])+args.address);result={"queued":True,"role":args.role}
        elif command=="disconnect":client.request(6,bytes([ROLES[args.role]]));result={"queued":True,"role":args.role}
        elif command=="pair":
            pin=args.pin.encode("ascii")
            if len(pin)>16 or b"\0" in pin:parser.error("PIN must contain0..16 non-NUL ASCII bytes")
            client.request(7,args.address+bytes([len(pin)])+pin);result={"queued":True}
        elif command=="pairwindow":
            if not 0<=args.seconds<=120:parser.error("Pairing window must be0..120seconds")
            client.request(8,struct.pack("<I",args.seconds));result={"queued":True,"seconds":args.seconds}
        elif command=="gps":
            if not -90<=args.latitude<=90 or not -180<=args.longitude<=180:parser.error("Latitude/longitude outside Earth coordinates")
            flags=1;values=[1,round(args.latitude*1e7),round(args.longitude*1e7),0,0,0,0,0,0,0,args.quality]
            for attr,index,flag in (("speed_mm_s",3,8),("course_mdeg",4,16),("altitude_mm",5,32),("utc_ms",6,2),("date",7,4),("satellites",8,64),("hdop_milli",9,128)):
                value=getattr(args,attr)
                if value is not None:values[index]=value;flags|=flag
            values[0]=flags;client.request(9,struct.pack("<11I",*[v&0xFFFFFFFF for v in values]));result={"accepted":True,"fields":flags}
        elif command=="update-status":
            payload=client.request(0x45);result={"state_transaction_received_verified":words(payload,4),"extra_hex":payload[16:].hex()}
        else:
            opcode={"ping":0,"discover":3}[command];result={"accepted":True,"reply_hex":client.request(opcode).hex()}
        print(json.dumps(result,indent=2));return 0


if __name__=="__main__":
    raise SystemExit(main())
