#!/usr/bin/env python3
"""H-RAM-1 one-client web profile (PR 206, plan 3376a19), stdlib only.

Replays the request pattern of the shipped web shell for ONE client:
S1 login, S2 one shell GET, S3 5 s polling groups (status, temperatures,
alerts; single flight: a group starts only when the previous one finished),
S5 logout. It does not touch the UART, flash or reset anything and sends no
mutation other than login/logout. Password only from a file (mode 600); neither
password, cookie nor CSRF token is ever written to the log.

Usage: hram1_client.py BASE_URL PASSWORD_FILE OUT_JSONL [--poll-seconds 600]
Stops at the first non-200 answer, the first incomplete/invalid body or a
timeout, logs out and exits non-zero.
"""
import http.client, json, os, stat, sys, threading, time, urllib.parse

DEADLINE = 30.0
API = ("status", "temperatures", "alerts")


def main(argv):
    base, pwfile, out = argv[1], argv[2], argv[3]
    poll_seconds = 600
    if "--poll-seconds" in argv:
        poll_seconds = int(argv[argv.index("--poll-seconds") + 1])
    mode = stat.S_IMODE(os.stat(pwfile).st_mode)
    if mode & 0o077:
        sys.exit("password file must not be group/world accessible")
    password = open(pwfile, encoding="utf-8").read().rstrip("\n")
    url = urllib.parse.urlparse(base)
    host, port = url.hostname, url.port or 80
    origin = f"http://{url.netloc}"
    log = open(out, "a", buffering=1, encoding="utf-8")
    t0 = time.monotonic()

    def emit(**kw):
        kw["t"] = round(time.monotonic() - t0, 3)
        kw["wall"] = time.strftime("%Y-%m-%dT%H:%M:%S%z")
        log.write(json.dumps(kw) + "\n")

    def request(method, path, headers=None, body=None):
        conn = http.client.HTTPConnection(host, port, timeout=DEADLINE)
        start = time.monotonic()
        try:
            h = {"Origin": origin, "Sec-Fetch-Site": "same-origin", "Connection": "close"}
            h.update(headers or {})
            conn.request(method, path, body=body, headers=h)
            response = conn.getresponse()
            data = response.read()
            return response.status, data, response.getheader("Set-Cookie"), time.monotonic() - start, None
        except Exception as exc:  # incomplete chunked body, reset, timeout
            return None, b"", None, time.monotonic() - start, type(exc).__name__
        finally:
            conn.close()

    def fail(step, **kw):
        emit(event="STOP", step=step, **kw)
        sys.exit(2)

    # S1 login
    status, data, cookie, dt, err = request(
        "POST", "/api/v1/login", {"Content-Type": "application/json"},
        json.dumps({"password": password}))
    emit(event="S1_login", status=status, seconds=round(dt, 3), error=err)
    if status != 200 or not cookie:
        fail("S1", status=status, error=err)
    session = cookie.split(";", 1)[0]
    csrf = json.loads(data).get("csrfToken", "")
    if not csrf:
        fail("S1", reason="no csrf in answer")
    headers = {"Cookie": session}

    # S2 one shell GET
    status, data, _, dt, err = request("GET", "/", headers)
    complete = status == 200 and data.rstrip().endswith(b"</html>")
    emit(event="S2_shell", status=status, bytes=len(data), complete=complete,
         seconds=round(dt, 3), error=err)
    if not complete:
        fail("S2", status=status, error=err)

    # S3 polling groups
    end = time.monotonic() + poll_seconds
    next_tick = time.monotonic()
    group = 0
    while time.monotonic() < end:
        now = time.monotonic()
        if now < next_tick:
            time.sleep(min(0.2, next_tick - now))
            continue
        group += 1
        results = {}

        def one(name):
            results[name] = request("GET", f"/api/v1/{name}", headers)

        threads = [threading.Thread(target=one, args=(n,)) for n in API]
        started = time.monotonic()
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        record = {"event": "S3_group", "group": group,
                  "seconds": round(time.monotonic() - started, 3)}
        for name in API:
            st, body, _, secs, e = results[name]
            valid = False
            if st == 200:
                try:
                    valid = json.loads(body).get("version") == 1
                except ValueError:
                    valid = False
            record[name] = {"status": st, "bytes": len(body), "seconds": round(secs, 3),
                            "valid": valid, "error": e}
            if name == "status" and valid:
                record["run"] = json.loads(body).get("run") is not None
        emit(**record)
        if any(not record[n]["valid"] for n in API):
            fail("S3", group=group)
        next_tick += 5.0
        if next_tick < time.monotonic():  # single flight: do not catch up
            next_tick = time.monotonic() + 5.0

    # S5 logout
    status, _, _, dt, err = request(
        "POST", "/api/v1/logout",
        {"Content-Type": "application/json", "X-CSRF-Token": csrf, "Cookie": session}, "{}")
    emit(event="S5_logout", status=status, seconds=round(dt, 3), error=err)
    emit(event="DONE", groups=group)
    return 0 if status == 200 else 3


if __name__ == "__main__":
    sys.exit(main(sys.argv))
