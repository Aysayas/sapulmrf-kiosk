"""Bridge operator mode against a fake SapulMRF: sign-in, lockout, tasks carry the cookie, photo packet, Lock logs out."""
import json, os, socket, struct, sys, tempfile, threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import cv2
import numpy as np

sys.path.insert(0, r"C:\dev\sapulmrf-kiosk\sapulmrf-kiosk\pc_bridge")
import kiosk_bridge as kb

os.chdir(tempfile.mkdtemp())
SEEN = []  # (method, path, cookie, body)
SESSIONS = set()
PHOTO = cv2.imencode(".jpg", np.full((1080, 1920, 3), 90, np.uint8))[1].tobytes()


class Fake(BaseHTTPRequestHandler):
    def log_message(self, *a): pass

    def reply(self, code, obj=None, raw=None, ctype="application/json"):
        b = raw if raw is not None else json.dumps(obj).encode()
        self.send_response(code); self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)

    def cookie(self):
        c = self.headers.get("Cookie", "")
        return c.split("sapulmrf_session=")[1].split(";")[0] if "sapulmrf_session=" in c else None

    def handle_any(self, method):
        n = int(self.headers.get("Content-Length") or 0)
        body = json.loads(self.rfile.read(n)) if n else None
        path, tok = self.path, self.cookie()
        SEEN.append((method, path, tok, body))
        if path == "/devices/kiosk-state":
            return self.reply(200, {"today": {"kg": 1, "drops": 1}, "alerts": [], "actuators": {"light": "green"}})
        if path == "/devices/kiosk-operators":
            assert self.headers["X-Device-Key"] == "k1"
            return self.reply(200, [{"id": 3, "name": "Juan Dela Cruz"}])
        if path == "/devices/kiosk-login":
            if body["pin"] == "9999":
                return self.reply(423, {"detail": "Too many wrong PINs. Try again in 5 minutes."})
            if body["pin"] != "2468":
                return self.reply(401, {"detail": "Wrong PIN."})
            SESSIONS.add("tok1")
            return self.reply(200, {"token": "tok1", "name": "Juan Dela Cruz", "expires_in_s": 1800})
        if path == "/auth/logout":
            SESSIONS.discard(tok)
            return self.reply(204, raw=b"")
        if tok not in SESSIONS:
            return self.reply(401, {"detail": "Log in first."})
        if path == "/overview":
            return self.reply(200, {"today_deliveries": 4, "today_kg": 12.5, "awaiting_segregation": 2,
                                    "areas": [{"name": "Area A", "pct": 102, "id": 1}], "devices": ["big"]})
        if path == "/classifications/pending":
            return self.reply(200, [{"id": 7, "suggested": "Plastic", "confidence": 0.861,
                                     "image_url": "/api/deliveries/12/photo", "reason": ""}])
        if path == "/deliveries/12/photo":
            return self.reply(200, raw=PHOTO, ctype="image/jpeg")
        if path.startswith("/masterdata/materials"):
            return self.reply(200, [{"id": 1, "name": "Plastic", "active": True}, {"id": 2, "name": "Metal", "active": True}])
        if path.startswith("/operations/intake?waiting=sorting"):
            return self.reply(200, [{"id": 12, "code": "D-0012", "source_area": "Purok 1", "category": "Recyclable",
                                     "weight_kg": 3.5, "segregation": {"dry_kg": 2.5}, "status": "separated"},
                                    {"id": 13, "code": "D-0013", "source_area": "Purok 2", "category": "Residual",
                                     "weight_kg": 1.0, "segregation": {"dry_kg": 1.0}, "status": "separated"}])
        if path == "/sorting":
            if body["rows"][0]["kg"] > 10:
                return self.reply(422, {"detail": "Area A is full: 20 kg more would make it 104%."})
            return self.reply(201, {})
        if method == "POST":
            return self.reply(201, {})
        self.reply(404, {"detail": "Not Found"})

    def do_GET(self): self.handle_any("GET")
    def do_POST(self): self.handle_any("POST")


srv = ThreadingHTTPServer(("127.0.0.1", 0), Fake); threading.Thread(target=srv.serve_forever, daemon=True).start()
kb.API_URL, kb.API_KEY = f"http://127.0.0.1:{srv.server_port}", "k1"

b = kb.Bridge(None, 0)
pc, kiosk = socket.socketpair(); kiosk.settimeout(10)
b.sock, b.alive = pc, True
threading.Thread(target=b.read_loop, daemon=True).start()


def send(o):
    p = json.dumps(o).encode(); kiosk.sendall(struct.pack("<cI", b"M", len(p)) + p)


def recv():
    kind, n = struct.unpack("<cI", kiosk.recv(5)); buf = b""
    while len(buf) < n: buf += kiosk.recv(n - len(buf))
    return kind, buf


def msg():
    kind, buf = recv(); assert kind == b"M", kind
    return json.loads(buf)


# not signed in: tasks are refused without calling SapulMRF
send({"type": "op", "do": "today"})
assert msg() == {"type": "op", "do": "today", "ok": False, "error": "Sign in first.", "signed_out": True}

send({"type": "operators"})
assert msg() == {"type": "operators", "items": [{"id": 3, "name": "Juan Dela Cruz"}]}
send({"type": "pin", "user_id": 3, "pin": "1111"})
assert msg() == {"type": "auth", "ok": False, "locked": False, "error": "Wrong PIN."}
send({"type": "pin", "user_id": 3, "pin": "9999"})
m = msg(); assert m["locked"] and "5 minutes" in m["error"], m
send({"type": "pin", "user_id": 3, "pin": "2468"})
assert msg() == {"type": "auth", "ok": True, "name": "Juan Dela Cruz"} and b.token == "tok1"

send({"type": "op", "do": "today"})
assert msg()["data"] == {"drops": 4, "kg": 12.5, "segregate": 2, "areas": [{"name": "Area A", "pct": 102}], "waiting": 0, "sort": 2}
assert kb.bucket("chair") is None and kb.bucket("TV") is None and kb.bucket("banana") == "bio"
assert kb.bucket("bottle") == "recyclable" and kb.bucket("sachet") == "residual"
send({"type": "op", "do": "waiting", "for": "sorting"})
rows = msg()["data"]
assert rows[0] == {"id": 12, "code": "D-0012", "source_area": "Purok 1", "category": "Recyclable", "weight_kg": 3.5,
                   "dry_kg": 2.5, "suggestion_id": 7, "guess": "Plastic", "pct": 86, "guess_material_id": 1}, rows[0]
assert "guess" not in rows[1]
send({"type": "photo", "delivery_id": 12})
kind, buf = recv(); assert kind == b"P" and struct.unpack("<I", buf[:4])[0] == 12
img = cv2.imdecode(np.frombuffer(buf[4:], np.uint8), 1); assert img.shape == (180, 320, 3), img.shape

sort = {"type": "op", "do": "sort", "delivery_id": 12, "area_id": 1, "kg": 2.5, "bags": 1, "suggestion_id": 7,
        "guess_material_id": 1}
for chosen, expect in ((1, {}), (2, {"final_material_id": 2}), (None, {"not_recyclable": True})):
    send({**sort, "material_id": chosen})
    m = msg(); assert m["ok"] and "note" in m and m["note"] is None, m
    assert SEEN[-2][1] == "/sorting" and SEEN[-1] == ("POST", "/classifications/7/confirm", "tok1", expect), SEEN[-2:]
send({"type": "op", "do": "today"})
assert msg()["data"]["sort"] == 2
send({"type": "op", "do": "segregate", "delivery_id": 12, "condition": "mixed", "wet_kg": 1.5, "dry_kg": 2})
assert msg()["ok"] and SEEN[-1] == ("POST", "/deliveries/12/segregation", "tok1",
                                     {"condition": "mixed", "wet_kg": 1.5, "dry_kg": 2.0})
n = len(SEEN)
send({"type": "op", "do": "sort", "delivery_id": 12, "material_id": 1, "area_id": 1, "kg": 20, "bags": 2, "suggestion_id": 7})
m = msg(); assert len(SEEN) == n + 1, "a refused sort must not touch the suggestion"
assert not m["ok"] and m["error"] == "Area A is full: 20 kg more would make it 104%." and not m["signed_out"], m
send({"type": "op", "do": "sort", "delivery_id": 12})  # missing fields
assert msg()["error"] == "The kiosk sent an incomplete form."
send({"type": "op", "do": "materials"})
assert msg()["data"] == [{"id": 1, "name": "Plastic"}, {"id": 2, "name": "Metal"}]

# Lock logs out on SapulMRF; then the session is gone
send({"type": "lock"}); send({"type": "op", "do": "today"})
assert msg()["error"] == "Sign in first." and "tok1" not in SESSIONS
assert ("POST", "/auth/logout", "tok1", None) in SEEN

# a session that expired on the server: the kiosk is told it's signed out
send({"type": "pin", "user_id": 3, "pin": "2468"}); msg()
SESSIONS.clear()
send({"type": "op", "do": "today"})
m = msg(); assert m["signed_out"] and m["error"] == "Signed out. Sign in again." and b.token is None, m
assert all(c in (None, "tok1") for _, _, c, _ in SEEN)
print("check_operator: all passed")
