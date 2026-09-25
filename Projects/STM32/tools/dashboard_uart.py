"""AK550 bench peer: actual115200 8N1 UART, not target RAM speed injection.

Use the isolated UNO<->Noodoe harness already verified on COM11. All bytes
other than explicit speed/ODO retain the recorded fuel1 frame. A worker keeps
10Hz TX/RX alive while the caller captures/reads the target through ST-LINK.
"""
from pathlib import Path
from collections import deque
import argparse,json,os,sys,threading,time
import serial
log_write_failures=0

def frame(speed,odo=36475):
    if not 0<=speed<=255 or not 0<=odo<=0xffffffff:raise ValueError('speed0..255, ODO uint32')
    b=bytearray.fromhex('f5 21 09 00 00 00 51 7b 8e 00 00 00 79')
    b[3]=speed;b[7:11]=odo.to_bytes(4,'little');b[-1]=0
    for x in b[:-1]:b[-1]^=x
    return bytes(b)

class DashboardPeer:
    def __init__(self,port='COM11',speed=0,odo=36475,ramp=None):
        # Ramp advances only after a complete UART write, so every step is sent.
        if ramp is not None:
            if not 0<=ramp[0]<=ramp[1]<=255:raise ValueError('ramp must fit0..255')
            speed=ramp[0]
        frame(speed,odo)
        self.port=port;self.speed=speed;self.odo=odo;self.events=deque(maxlen=6000);self.error=None
        self.ramp=ramp;self.tx_count=0;self.rx_bytes=0;self.last_speed=None;self.cycles=0
        self.stop=threading.Event();self.lock=threading.Lock();self.started=time.monotonic()
        self.ser=serial.Serial(port=None,baudrate=115200,timeout=.005,write_timeout=2)
        self.ser.dtr=False;self.ser.rts=False;self.ser.port=port;self.ser.open()
        self.thread=threading.Thread(target=self._run,daemon=True);self.thread.start()
    def set_speed(self,value):
        if self.ramp is not None:raise ValueError('fixed speed cannot override a ramp')
        frame(value,self.odo)
        with self.lock:self.speed=value
    def status(self):
        # Snapshot counters under the same lock as TX; no serial access here.
        with self.lock:
            return {'pid':os.getpid(),'port':self.port,'baud':115200,'format':'8N1',
                    'running':self.thread.is_alive() and not self.stop.is_set() and self.error is None,
                    'error':self.error,'log_write_failures':log_write_failures,'tx_count':self.tx_count,'rx_bytes':self.rx_bytes,
                    'last_speed':self.last_speed,'completed_cycles':self.cycles,
                    'ramp':self.ramp,'interval_seconds':.1,'elapsed_seconds':time.monotonic()-self.started}
    def _run(self):
        deadline=0
        try:
            while not self.stop.is_set():
                now=time.monotonic()
                if now>=deadline:
                    with self.lock:b=frame(self.speed,self.odo)
                    if self.ser.write(b)!=len(b):raise OSError('Short UART write')
                    with self.lock:
                        self.events.append({'s':now-self.started,'direction':'TX','hex':b.hex(' ')})
                        self.last_speed=b[3];self.tx_count+=1
                        if self.ramp is not None:
                            if self.speed==self.ramp[1]:self.speed=self.ramp[0];self.cycles+=1
                            else:self.speed+=1
                    deadline=now+.1
                b=self.ser.read(4096)
                if b:
                    with self.lock:
                        self.rx_bytes+=len(b)
                        self.events.append({'s':time.monotonic()-self.started,'direction':'RX','hex':b.hex(' ')})
        except Exception as e:self.error=str(e)
    def close(self):
        self.stop.set();self.thread.join(3)
        try:
            if self.ser.is_open:self.ser.write(frame(0,self.odo));self.ser.flush()
        finally:self.ser.close()
    def save(self,path):
        # Bounded recent history prevents an unattended ramp from growing forever.
        record=self.status()
        with self.lock:record['events']=list(self.events)
        record['source']='PC dashboard emulator; real physical UART'
        write_json(path,record)

def write_json(path,value):
    # Windows readers may temporarily deny replacing an open destination.
    # Preserve the previous complete record and retry on the next1s report;
    # diagnostic-file contention must never close an otherwise healthy UART.
    # Real serial exceptions still propagate through peer.error independently.
    global log_write_failures
    path=Path(path);temporary=path.with_suffix(path.suffix+'.tmp')
    try:
        temporary.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8');temporary.replace(path)
        return True
    except OSError as error:
        log_write_failures+=1
        if log_write_failures==1 or log_write_failures%60==0:
            print(f'Diagnostic write deferred ({log_write_failures}): {error}',file=sys.stderr,flush=True)
        return False

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--port',default='COM11');ap.add_argument('--speed',type=int,default=0)
    ap.add_argument('--odo',type=int,default=36475);ap.add_argument('--seconds',type=float,default=30)
    ap.add_argument('--ramp',type=int,nargs=2,metavar=('MIN','MAX'),help='inclusive rising speed range, repeats every100ms; e.g.1 200')
    ap.add_argument('--stop-file',type=Path,help='stop cleanly when this file exists; final frame has speed0')
    ap.add_argument('--status-file',type=Path,help='atomic progress JSON, refreshed once a second')
    ap.add_argument('--speed-file',type=Path,help='optional JSON file with {"speed":73}, read every100ms')
    ap.add_argument('--output',type=Path,default=Path('dashboard-uart.json'));ap.add_argument('--execute',action='store_true');a=ap.parse_args()
    if not 0<=a.seconds<=1800:ap.error('seconds must be0..1800;0 means until stopped')
    if a.ramp and not 0<=a.ramp[0]<=a.ramp[1]<=255:ap.error('ramp must fit0..255')
    if a.ramp and a.speed_file:ap.error('ramp and speed-file are mutually exclusive')
    if a.stop_file and a.stop_file.exists():ap.error('stop file already exists')
    print(frame(a.ramp[0] if a.ramp else a.speed,a.odo).hex(' '),flush=True)
    if not a.execute:return
    peer=DashboardPeer(a.port,a.speed,a.odo,a.ramp)
    try:
        end=time.monotonic()+a.seconds if a.seconds else float('inf');next_report=0
        while time.monotonic()<end and not(a.stop_file and a.stop_file.exists()):
            if peer.error:raise RuntimeError(peer.error)
            if a.speed_file:peer.set_speed(int(json.loads(a.speed_file.read_text())['speed']))
            if time.monotonic()>=next_report:
                if a.status_file:write_json(a.status_file,peer.status())
                peer.save(a.output);next_report=time.monotonic()+1
            time.sleep(.1)
    finally:
        peer.close();peer.save(a.output)
        if a.status_file:write_json(a.status_file,peer.status())
if __name__=='__main__':main()
