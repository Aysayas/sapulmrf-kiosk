import json, os, socket, struct, sys, tempfile, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
sys.path.insert(0, r"C:\dev\sapulmrf-kiosk\sapulmrf-kiosk\pc_bridge")
import kiosk_bridge as kb

os.chdir(tempfile.mkdtemp())          # outbox/ and the CSV land in a throwaway folder
API = {"mode": "up", "got": [], "codes": {}}

class Fake(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def reply(self, code, obj):
        b = json.dumps(obj).encode(); self.send_response(code)
        self.send_header("Content-Type", "application/json"); self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        assert self.headers["X-Device-Key"] == "k1"
        if self.path == "/devices/kiosk-state":
            return self.reply(200, {"today": {"kg": 12.5, "drops": 3}, "alerts": [{"kind": "storage", "title": "Area A over capacity", "details": "102%"}],
                                    "actuators": {"light": "red", "buzz": 0}})
        self.reply(200, [{"code": "D-0007", "time": "9:41 AM", "purok": "Purok 1", "cat": "Recyclable", "kg": "1.0 kg"}])
    def do_POST(self):
        body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        if API["mode"] == "down": return self.reply(503, {})
        if API["mode"] == "reject": return self.reply(422, {"detail": "Unknown purok: Purok 9."})
        API["got"].append(body)
        code = API["codes"].setdefault(body["client_key"], f"D-{len(API['codes']) + 1:04d}")
        self.reply(201, {"code": code, "actions": {"buzz": 1}})

srv = ThreadingHTTPServer(("127.0.0.1", 0), Fake); threading.Thread(target=srv.serve_forever, daemon=True).start()
kb.API_URL, kb.API_KEY, kb.RETRY_EVERY, kb.STATE_EVERY = f"http://127.0.0.1:{srv.server_port}", "k1", 0.2, 0.2

b = kb.Bridge(None, 0)
pc, kiosk = socket.socketpair(); kiosk.settimeout(10)
b.sock, b.alive = pc, True
threading.Thread(target=b.read_loop, daemon=True).start()
def send(o): p = json.dumps(o).encode(); kiosk.sendall(struct.pack("<cI", b"M", len(p)) + p)
def recv():
    kind, n = struct.unpack("<cI", kiosk.recv(5)); buf = b""
    while len(buf) < n: buf += kiosk.recv(n - len(buf))
    return json.loads(buf)
def until(t):
    while True:
        m = recv()
        if m["type"] == t: return m

drop = {"type": "intake", "weight": 2.5, "category": "Recyclable", "purok": 1, "manual": True, "recyclables": 2}
# 1. online, with a camera snapshot
import numpy as np
b.snapshot = ({"category": "Recyclable", "recyclable": 2, "residual": 0, "bio": 0, "conf": 0.8}, np.zeros((1080, 1920, 3), np.uint8),
              [(0, 0, 9, 9, "bottle", 0.8, "recyclable"), (0, 0, 9, 9, "bottle", 0.9, "recyclable"), (0, 0, 9, 9, "banana", 0.7, "bio")])
send(drop)
r = until("receipt"); assert r == {"type": "receipt", "ok": True, "code": "D-0001"}, r
assert until("actuate") == {"type": "actuate", "buzz": 1}
sent = API["got"][-1]
import base64, cv2
ph = cv2.imdecode(np.frombuffer(base64.b64decode(sent["photo"]), np.uint8), 1); assert ph.shape == (1080, 1920, 3), ph.shape
assert sent["materials"] == [{"label": "Plastic", "confidence": 0.9}], sent["materials"]
assert sent["cv"] == {"category": "Recyclable", "confidence": 0.8, "counts": {"recyclable": 2, "residual": 0, "bio": 0}} and sent["photo"]
assert sent["weighed_by"] == "manual" and sent["source_area"] == "Purok 1" and b.snapshot is None
# 2. API down: K- code and an outbox file; back up: sent once by the retry loop
API["mode"] = "down"; send(drop)
r = until("receipt"); assert r["code"].startswith("K-"), r
assert len(os.listdir("outbox")) == 1
API["mode"] = "up"; threading.Thread(target=b.outbox_loop, daemon=True).start()
for _ in range(50):
    if not [f for f in os.listdir("outbox") if f.endswith(".json")]: break
    time.sleep(0.1)
assert not [f for f in os.listdir("outbox") if f.endswith(".json")] and len(API["got"]) == 2
# 3. rejected for good: kept in outbox/rejected, kiosk still gets a K- receipt
API["mode"] = "reject"; send(drop)
assert until("receipt")["code"].startswith("K-") and len(os.listdir("outbox/rejected")) == 1
API["mode"] = "up"
# 4. state relay: totals, alerts and light from SapulMRF
threading.Thread(target=b.state_loop, daemon=True).start()
assert until("alerts")["items"][0]["title"] == "Area A over capacity"
assert until("actuate")["light"] == "red"
send({"type": "stats"})
while (m := until("stats")) != {"type": "stats", "kg": 12.5, "drops": 3}: pass  # older local stats may be queued first
b.token = "t"; send({"type": "recent"}); assert until("recent")["rows"][0]["code"] == "D-0007"
print("OK: online D- receipt + beep + cv/photo, offline K- then retried once, rejected kept, state/alerts/light/recent relayed")
