"""G1 capture/supervision. No flash. Port opening is an authorized boot action.
Importing this module never opens UART or starts requests.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import threading
import time
from types import SimpleNamespace
from c0_load import BAD, HEARTBEAT, run as load_run

# Read-only host behaviour after a supervised anomaly: keep the port open for a
# bounded time so the follow-up (further events, backtrace, panic, reset or
# recovery) is recorded. The load has already been stopped via ABORT.
POST_ANOMALY_TAIL_SECONDS = 5.0
POST_ANOMALY_TAIL_MAX_READS = 120  # bounds the tail even if the clock is frozen
NO_TAIL = {'host-interrupt', 'capture-failure'}
# The expected, single reset caused by opening the port (known FT232R behaviour): only this cause, only the
# first marker and only within this window after the real port open is accepted as the test boot.
BOOT_MARKER = re.compile(r'rst:0x1 \(POWERON_RESET\)')
BOOT_MARKER_WINDOW_SECONDS = 10.0
BAD_WITHOUT_RESET = re.compile(BAD.pattern.replace('|rst:0x', ''), re.I)  # an unterminated 'rst:0x' waits for its line end
MAX_RAW_BYTES = 256 * 1024 * 1024  # fail-closed volume limit; safety-relevant UART lines are never dropped


def post_anomaly_tail(board,raw,chunks,tail_log,pending,result):
    """Read-only, bounded. `pending` is the unterminated frame seen at detection time."""
    end=time.monotonic()+POST_ANOMALY_TAIL_SECONDS
    began=time.monotonic()
    total=0
    with tail_log.open('wb',buffering=0) as sink:
        for _ in range(POST_ANOMALY_TAIL_MAX_READS):
            if time.monotonic()>=end:break
            try:data=board.read(4096)
            except Exception:break
            if not data:continue
            raw.write(data)
            chunks.write(json.dumps({'epoch':time.time(),'monotonic':time.monotonic(),
                                     'bytes':len(data),'tail':True})+'\n')
            total+=len(data)
            pending+=data
            while b'\n' in pending:
                line,pending=pending.split(b'\n',1)
                sink.write(('[tail host_mono=%.6f] '%time.monotonic()).encode()+line+b'\n')
        if pending:
            sink.write(b'[tail-unterminated] '+pending+b'\n')
    return {'post_anomaly_tail_seconds':round(time.monotonic()-began,3),'post_anomaly_tail_bytes':total}


def capture(args):
    if not args.run or not args.gate_id or not args.allow_port_open_boot:
        raise SystemExit('No UART action without --run, approved --gate-id and --allow-port-open-boot')
    import serial
    args.output.mkdir(parents=True,exist_ok=False)
    uart=args.output/'uart.log'
    abort_file=args.output/'ABORT'
    result={'start_epoch':time.time(),'start_monotonic':time.monotonic(),
            'observation_mode':bool(getattr(args,'observe_network_timeouts',False)),
            'tool_sha256':{n:hashlib.sha256((Path(__file__).parent/n).read_bytes()).hexdigest()
                           for n in ('g1_capture.py','c0_load.py')}}
    ready=actors=profile=network=source=False
    last_heartbeat=started=time.monotonic()
    resets=0
    boot_marker=None   # evidence of the accepted fresh test boot
    pre_marker_lines=0
    opened=None
    last_uptime=None
    offset=b''
    raw_total=0
    failure=None
    loader=None
    next_socket_snapshot=0.0
    max_gap=0.0
    def socket_snapshot():
        check=subprocess.run(['ss','-tan','dst',args.host],capture_output=True,text=True,timeout=2)
        if check.returncode:
            raise RuntimeError('host-socket-evidence-unavailable')
        with (args.output/'host_sockets.jsonl').open('a') as sink:
            sink.write(json.dumps({'epoch':time.time(),'monotonic':time.monotonic(),
                                   'socket_states':check.stdout})+'\n')
    board=serial.Serial(port=None,baudrate=115200,timeout=0.05)
    board.port=args.port;board.dtr=False;board.rts=False
    try:
        with uart.open('wb',buffering=0) as log, (args.output/'uart_raw.bin').open('wb',buffering=0) as raw, \
                (args.output/'uart_chunks.jsonl').open('w') as chunks:
            board.open()  # explicitly gated; this FT232R is known to reset on open
            opened=time.monotonic()
            while True:
                now=time.monotonic()
                if now-last_heartbeat>5:
                    failure='uart-heartbeat-timeout';break
                if now>=next_socket_snapshot:
                    socket_snapshot()
                    next_socket_snapshot=now+30
                data=board.read(4096)
                raw.write(data)
                if data:
                    chunks.write(json.dumps({'epoch':time.time(),'monotonic':time.monotonic(),
                                             'bytes':len(data)})+'\n')
                offset+=data
                raw_total+=len(data)
                if raw_total>MAX_RAW_BYTES:
                    failure='log-volume-limit';break
                while b'\n' in offset:
                    line,offset=offset.split(b'\n',1)
                    text=line.decode(errors='replace')
                    log.write(('[host_mono=%.6f] '%time.monotonic()).encode()+line+b'\n')
                    if 'rst:0x' in text:
                        resets+=1
                        accepted=(boot_marker is None and BOOT_MARKER.search(text)
                                  and time.monotonic()-opened<=BOOT_MARKER_WINDOW_SECONDS)
                        if not accepted:
                            failure='uart-unexpected-reset';break
                        # Everything collected before the marker is buffered old data (kept in the log, never
                        # evidence of this boot): drop all boot/identity/liveness state.
                        boot_marker={'accepted':True,'cause':'POWERON_RESET',
                                     'seconds_after_open':round(time.monotonic()-opened,3),
                                     'lines_before_marker':pre_marker_lines}
                        last_uptime=None;last_heartbeat=time.monotonic()
                        source=ready=actors=profile=network=False
                        continue
                    if boot_marker is None:
                        pre_marker_lines+=1
                    if BAD.search(text):
                        failure='uart-anomaly';break
                    main=re.search(r'(?<!httpd_)stack_hwm_bytes=(\d+)',text)
                    http=re.search(r'httpd_stack_hwm=(\d+)',text)
                    if main and int(main[1])<11160:failure='main-stack-reserve';break
                    if http and int(http[1])<2080:failure='httpd-stack-reserve';break
                    if boot_marker is None:
                        continue  # old buffered line: logged and safety-checked only
                    if HEARTBEAT.search(text):
                        stamp=time.monotonic()
                        if stamp-last_heartbeat>5:
                            failure='uart-heartbeat-timeout';break
                        max_gap=max(max_gap,stamp-last_heartbeat)
                        uptime=int(re.search(r'uptime_ms=(\d+)',text)[1])
                        if last_uptime is not None and uptime<last_uptime:
                            failure='uart-uptime-regression';break
                        last_heartbeat=stamp
                        last_uptime=uptime
                    if 'source git sha:' in text:
                        source=args.expected_source in text
                        if not source:failure='source-mismatch';break
                    ready|='application: ready' in text
                    actors|='real actuators: disabled' in text
                    profile|='profile: esp32_release' in text
                    network|='network_mode=HOME_WIFI network_state=HomeConnected' in text
                if not failure and BAD_WITHOUT_RESET.search(offset.decode(errors='replace')):
                    failure='uart-anomaly'
                if failure:
                    if failure not in NO_TAIL:
                        abort_file.write_text(failure+'\n')  # stop the load first
                        result.update(post_anomaly_tail(board,raw,chunks,args.output/'uart_tail.log',offset,result))
                    break
                if now-started>30 and not all((boot_marker,source,ready,actors,profile,network)):
                    failure='boot-identity-state-not-proven';break
                if args.load and loader is None and (args.output/'START_LOAD').exists() and all((boot_marker,source,ready,actors,profile,network)) and last_uptime is not None and last_uptime>=180000:
                    def load():
                        result['load_exit']=load_run(SimpleNamespace(run=True,gate_id=args.gate_id,
                            host=args.host,uart=uart,output=args.output/'load',abort_file=abort_file,
                            observe=bool(getattr(args,'observe_network_timeouts',False))))
                    loader=threading.Thread(target=load)
                    loader.start()
                if loader is not None and not loader.is_alive():
                    if result.get('load_exit')!=0:
                        failure='load-failure';break
                    # Remain under UART supervision for manual coverage until
                    # the host STOP marker. Never restart the load or reset.
                if (args.output/'STOP').exists():
                    if loader is not None and loader.is_alive():failure='owner-stop'
                    break
    except KeyboardInterrupt:
        failure='host-interrupt'
    except Exception:
        failure='capture-failure'  # fixed class, no exception/body/credentials
    finally:
        if failure:abort_file.write_text(failure+'\n')
        if loader is not None:loader.join(timeout=9.0)
        if board.is_open:board.close()
        if loader is not None and loader.is_alive():
            result['runner_not_joined']=True
        result.update(end_epoch=time.time(),end_monotonic=time.monotonic(),max_heartbeat_gap_seconds=max_gap,gate=args.gate_id,source_proven=source,ready=ready,actors_disabled=actors,
                      release_profile=profile,network_home_connected=network,expected_initial_reset_lines=resets,
                      boot_marker=boot_marker,lines_before_marker=pre_marker_lines,
                      failure=failure,status='FAIL/HOLD' if failure else 'CAPTURE_COMPLETE_PENDING_REVIEW')
        (args.output/'capture.json').write_text(json.dumps(result,indent=2)+'\n')
    return 1 if failure else 0


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--run',action='store_true')
    p.add_argument('--gate-id')
    p.add_argument('--allow-port-open-boot',action='store_true')
    p.add_argument('--expected-source',required=True)
    p.add_argument('--port',default='/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_A5069RR4-if00-port0')
    p.add_argument('--host',default='192.168.1.68')
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--load',action='store_true')
    p.add_argument('--observe-network-timeouts',action='store_true',
                   help='diagnosis only: forwarded to the load driver (<= 20 s observation after the first genuine network timeout, hard stop at the 8th); heap_alloc_failed, panic, watchdog, brownout, reset, heartbeat gap and stack limits stay immediate hard stops')
    raise SystemExit(capture(p.parse_args()))
