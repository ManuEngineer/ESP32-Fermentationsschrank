#!/usr/bin/env python3
"""H-RAM-1 real-browser profile (headless Chromium via CDP), one client.
S1 login through the real form, S2 the page load itself, S3 the shell's own 5 s
polling observed for --seconds, S5 logout and browser close. Reads only; the only
POSTs are the shell's login/logout. Password only from a 600 file, never logged."""
import json, os, stat, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cdp import Chrome, Page
host, pwfile, out = sys.argv[1], sys.argv[2], sys.argv[3]
seconds = int(sys.argv[sys.argv.index("--seconds") + 1]) if "--seconds" in sys.argv else 600
assert not stat.S_IMODE(os.stat(pwfile).st_mode) & 0o077
pw = open(pwfile, encoding="utf-8").read().split("\n")[0]
log = open(out, "a", buffering=1)
t0 = time.time()
def emit(**k):
    k["t"] = round(time.time() - t0, 3); k["wall"] = time.strftime("%Y-%m-%dT%H:%M:%S%z"); log.write(json.dumps(k) + "\n")
SAMPLE = """({G:G,LT:LT,P:P!==null,LP:LP!==null,st:document.getElementById('connection').dataset.state,
 mode:document.getElementById('mode').textContent,A:A,U:U,
 run:(LP&&LP[0]&&('run' in LP[0]))?(LP[0].run===null?'null':'object'):'absent'})"""
ch = Chrome(); rc = 0
try:
    ctx = ch.call("Target.createBrowserContext", {"disposeOnDetach": True})["browserContextId"]
    pg = Page(ch, 1280, 800, False, "en-US", ctx)
    start = time.time()
    pg.goto(host, 3.0)
    emit(event="S2_anonymous_load", auth=pg.ev("A"), login_visible=pg.ev("!document.getElementById('login').hidden"))
    pg.ev("document.getElementById('password').value=" + json.dumps(pw))
    pg.ev("document.querySelector('#login button').click()")
    time.sleep(8.0)
    s = pg.ev(SAMPLE); emit(event="S1_after_login", **s)
    if not s["U"]:
        emit(event="STOP", reason="not authenticated after login"); rc = 2
    else:
        end = time.time() + seconds; nxt = time.time()
        while time.time() < end:
            if time.time() >= nxt:
                s = pg.ev(SAMPLE); emit(event="S3_sample", **s)
                nxt += 30.0
            time.sleep(1.0)
        ev = pg.events(start)
        status = {}
        failed = []
        for e in ev:
            m = e.get("method")
            if m == "Network.responseReceived":
                r = e["params"]["response"]
                if "/api/v1/" in r["url"] or r["url"].rstrip("/").endswith("192.168.1.68"):
                    status[str(r["status"])] = status.get(str(r["status"]), 0) + 1
            elif m == "Network.loadingFailed":
                failed.append(e["params"].get("errorText"))
        emit(event="S3_network_summary", responses_by_status=status, loading_failed=failed)
        pg.ev("fetch('/api/v1/logout',{method:'POST',headers:{'Content-Type':'application/json','X-CSRF-Token':window.__WEB_CSRF__||''}}).then(r=>r.status)")
        emit(event="S5_logout_sent")
finally:
    ch.close(); emit(event="S5_browser_closed", rc=rc)
sys.exit(rc)
