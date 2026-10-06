#!/usr/bin/env python3
"""PR170 session gate. Reads the password from ~/pw, never prints it.
Usage: session_gate.py login | poll SECONDS | logout1 | relogin
Sessions are kept in /tmp/pr170-evidence/sessions.json (cookie + csrf only)."""
import http.client, json, os, sys, time, threading

HOST = "192.168.1.68"
STATE = "/tmp/pr170-evidence/sessions.json"
PW = open(os.path.expanduser("~/pw"), encoding="utf-8").read().split("\n")[0]
ORIGIN = f"http://{HOST}"


def req(method, path, body=None, cookie=None, csrf=None, timeout=10):
    c = http.client.HTTPConnection(HOST, 80, timeout=timeout)
    h = {"Origin": ORIGIN, "Host": HOST}
    data = None
    if body is not None:
        data = json.dumps(body).encode()
        h["Content-Type"] = "application/json"
    elif method == "POST":
        h["Content-Type"] = "application/json"
        data = None  # logout requires an empty body
    if cookie:
        h["Cookie"] = cookie
    if csrf:
        h["X-CSRF-Token"] = csrf
    t0 = time.monotonic()
    try:
        c.request(method, path, body=data, headers=h)
        r = c.getresponse()
        raw = r.read()
        dt = (time.monotonic() - t0) * 1000
        sc = r.getheader("Set-Cookie")
        return r.status, raw, sc, dt
    except Exception as e:  # noqa
        return -1, repr(e).encode(), None, (time.monotonic() - t0) * 1000
    finally:
        c.close()


def load():
    try:
        return json.load(open(STATE))
    except Exception:
        return []


def save(s):
    json.dump(s, open(STATE, "w"))
    os.chmod(STATE, 0o600)


def login_one():
    st, raw, sc, dt = req("POST", "/api/v1/login", {"password": PW})
    if st != 200 or not sc:
        return st, None, dt, raw[:80]
    cookie = sc.split(";")[0]
    csrf = json.loads(raw).get("csrfToken", "")
    return st, {"cookie": cookie, "csrf": csrf}, dt, b""


cmd = sys.argv[1]
if cmd == "login":
    sessions = []
    for i in range(1, 6):
        st, s, dt, err = login_one()
        print(f"session {i}: login HTTP {st} in {dt:.0f} ms", err.decode(errors="replace") if err else "")
        if s:
            sessions.append(s)
            stc, _, _, _ = req("GET", "/api/v1/status", cookie=s["cookie"])
            print(f"   status with session {i}: HTTP {stc}")
        time.sleep(0.5)
    save(sessions)
    print("active sessions held:", len(sessions))
elif cmd == "poll":
    secs = int(sys.argv[2])
    sessions = load()
    stats = {"ok": 0, "fail": 0, "lat": []}
    lock = threading.Lock()
    stop = time.monotonic() + secs

    def worker(s):
        while time.monotonic() < stop:
            for p in ("/api/v1/status", "/api/v1/temperatures", "/api/v1/alerts"):
                st, _, _, dt = req("GET", p, cookie=s["cookie"])
                with lock:
                    if st == 200:
                        stats["ok"] += 1
                        stats["lat"].append(dt)
                    else:
                        stats["fail"] += 1
                        print("poll failure HTTP", st)
            time.sleep(1.0)

    ts = [threading.Thread(target=worker, args=(s,)) for s in sessions]
    [t.start() for t in ts]
    [t.join() for t in ts]
    lat = sorted(stats["lat"]) or [0]
    print(f"poll {secs}s sessions={len(sessions)} ok={stats['ok']} fail={stats['fail']} "
          f"lat_ms p50={lat[len(lat)//2]:.0f} max={lat[-1]:.0f}")
elif cmd == "logout1":
    sessions = load()
    s = sessions[0]
    st, _, _, _ = req("POST", "/api/v1/logout", cookie=s["cookie"], csrf=s["csrf"])
    print("logout of one session: HTTP", st)
    if st == 200:
        sessions.pop(0)
    save(sessions)
    st, s2, dt, err = login_one()
    print("new login after logout: HTTP", st)
    if s2:
        sessions.append(s2)
    save(sessions)
    st, s3, dt, err = login_one()
    print("another login (should be rejected, capacity full): HTTP", st)
    if s3:
        sessions.append(s3)
        save(sessions)
