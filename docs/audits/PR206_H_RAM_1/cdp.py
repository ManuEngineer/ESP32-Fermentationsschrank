#!/usr/bin/env python3
"""Minimal Chrome DevTools Protocol driver over --remote-debugging-pipe (stdlib only).
Test tooling for PR #203 evidence; never prints the web password."""
import base64, glob, json, os, subprocess, tempfile, threading, time

CHROME = glob.glob(os.path.expanduser("~/.cache/ms-playwright/chromium-*/chrome-linux*/chrome"))[0]

import base64 as _b64, hashlib, socket, struct

class _WS:
    def __init__(self, host, port, path):
        self.s = socket.create_connection((host, port), 10)
        key = _b64.b64encode(os.urandom(16)).decode()
        self.s.sendall((f"GET {path} HTTP/1.1\r\nHost: {host}:{port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                        f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n").encode())
        buf = b""
        while b"\r\n\r\n" not in buf: buf += self.s.recv(4096)
        assert b" 101 " in buf.split(b"\r\n")[0], buf[:200]
        self.rest = buf.split(b"\r\n\r\n", 1)[1]
    def send(self, text):
        data = text.encode(); n = len(data); mask = os.urandom(4)
        hdr = bytes([0x81])
        if n < 126: hdr += bytes([0x80 | n])
        elif n < 65536: hdr += bytes([0x80 | 126]) + struct.pack(">H", n)
        else: hdr += bytes([0x80 | 127]) + struct.pack(">Q", n)
        self.s.sendall(hdr + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))
    def _read(self, n):
        out = self.rest[:n]; self.rest = self.rest[n:]
        while len(out) < n:
            c = self.s.recv(n - len(out))
            if not c: raise EOFError
            out += c
        return out
    def recv(self):
        msg = b""
        while True:
            b1, b2 = self._read(2)
            n = b2 & 0x7F
            if n == 126: n = struct.unpack(">H", self._read(2))[0]
            elif n == 127: n = struct.unpack(">Q", self._read(8))[0]
            payload = self._read(n)
            op = b1 & 0x0F
            if op == 8: raise EOFError
            if op in (1, 0): msg += payload
            if b1 & 0x80 and op in (1, 0): return msg.decode()

class Chrome:
    def __init__(self):
        self.tmp = tempfile.mkdtemp(prefix="pr203chrome")
        self.p = subprocess.Popen([CHROME, "--headless=new", "--no-sandbox", "--disable-gpu", "--no-first-run",
            "--disable-extensions", f"--user-data-dir={self.tmp}", "--remote-debugging-port=0", "about:blank"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        port_file = os.path.join(self.tmp, "DevToolsActivePort")
        for _ in range(100):
            if os.path.exists(port_file) and len(open(port_file).read().split()) >= 2: break
            time.sleep(0.1)
        port, path = open(port_file).read().split()[:2]
        self.ws = _WS("127.0.0.1", int(port), path)
        self.id = 0; self.waiters = {}; self.events = []; self.lock = threading.Lock()
        threading.Thread(target=self._read, daemon=True).start()
    def _read(self):
        while True:
            try: m = self.ws.recv()
            except Exception: return
            msg = json.loads(m)
            if "id" in msg:
                ev = self.waiters.pop(msg["id"], None)
                if ev: ev[1].append(msg); ev[0].set()
            else:
                msg["_t"] = time.time()
                with self.lock: self.events.append(msg)
    def call(self, method, params=None, session=None, timeout=60):
        with self.lock:
            self.id += 1; i = self.id
        ev = (threading.Event(), [])
        self.waiters[i] = ev
        m = {"id": i, "method": method, "params": params or {}}
        if session: m["sessionId"] = session
        self.ws.send(json.dumps(m))
        if not ev[0].wait(timeout): raise TimeoutError(method)
        res = ev[1][0]
        if "error" in res: raise RuntimeError(f"{method}: {res['error']}")
        return res.get("result", {})
    def close(self):
        try: self.call("Browser.close", timeout=5)
        except Exception: pass
        try: self.p.wait(5)
        except Exception: self.p.kill()

class Page:
    def __init__(self, chrome, width=1280, height=800, mobile=False, lang="en", context=None):
        self.c = chrome
        p = {"url": "about:blank"}
        if context: p["browserContextId"] = context
        tid = chrome.call("Target.createTarget", p)["targetId"]
        self.s = chrome.call("Target.attachToTarget", {"targetId": tid, "flatten": True})["sessionId"]
        for d in ("Page", "Runtime", "Network", "Log"): self.call(f"{d}.enable")
        self.set_view(width, height, mobile, lang)
    def call(self, m, p=None, timeout=60): return self.c.call(m, p, self.s, timeout)
    def set_view(self, w, h, mobile, lang):
        self.call("Emulation.setDeviceMetricsOverride", {"width": w, "height": h, "deviceScaleFactor": 2 if mobile else 1, "mobile": mobile})
        self.call("Emulation.setUserAgentOverride", {"userAgent": "Mozilla/5.0 (X11; Linux x86_64) PR203Test" + (" Mobile" if mobile else ""), "acceptLanguage": lang})
        self.call("Emulation.setLocaleOverride", {"locale": lang})
    def goto(self, url, wait=1.5):
        self.call("Page.navigate", {"url": url}); time.sleep(wait)
    def ev(self, expr, timeout=30):
        r = self.call("Runtime.evaluate", {"expression": expr, "returnByValue": True, "awaitPromise": True}, timeout)
        if "exceptionDetails" in r: raise RuntimeError(r["exceptionDetails"].get("exception", {}).get("description", "js error")[:300])
        return r["result"].get("value")
    def shot(self, path, full=True):
        d = self.call("Page.captureScreenshot", {"format": "png", "captureBeyondViewport": full})["data"]
        open(path, "wb").write(base64.b64decode(d))
    def events(self, since=0.0):
        with self.c.lock: return [e for e in self.c.events if e.get("sessionId") == self.s and e["_t"] >= since]
