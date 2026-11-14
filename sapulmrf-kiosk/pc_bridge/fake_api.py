"""A stand-in for SapulMRF, to test the kiosk and bridge with no Docker, database or web API.

    py fake_api.py              (window 1)
    $env:SAPULMRF_API="http://127.0.0.1:8001"; $env:SAPULMRF_KEY="fake"
    py kiosk_bridge.py --model best.pt --cam 0 --dev     (window 2)

It answers the routes kiosk_bridge.py calls, with the reply fields it reads, from demo data kept in memory
(each start is fresh). Operators: Operator 1 (PIN 1234), Operator 2 (PIN 5678).
A scale drop-off needs a steady reading of the same weight in the last 10 minutes, as SapulMRF requires.
Not faked: the Live camera websocket (the bridge just keeps retrying it) and camera suggestions.
"""
import argparse
import json
import re
import secrets
import threading
from datetime import datetime, timedelta
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

READING_MAX_AGE = timedelta(minutes=10)
SLACK_KG = 0.5   # a compost batch may use this much more than the wet waste waiting, like SapulMRF
try:  # the drop-off photo the sort screen shows: a plain grey picture (the bridge needs OpenCV anyway)
    import cv2
    import numpy as np
    GREY_JPEG = cv2.imencode(".jpg", np.full((480, 640, 3), 150, np.uint8))[1].tobytes()
except ImportError:
    GREY_JPEG = None


class AppError(Exception):
    def __init__(self, status, detail):
        super().__init__(detail)
        self.status, self.detail = status, detail


def kg(x):
    return round(float(x), 2)


class Store:
    """Everything the fake knows. One lock: requests from the bridge's threads never interleave."""

    def __init__(self):
        self.lock = threading.Lock()
        today = datetime.now()
        self.operators = {1: ("Operator 1", "1234"), 2: ("Operator 2", "5678")}
        self.sessions = {}                       # token -> operator id
        self.materials = {1: "Plastic", 2: "Paper", 3: "Metal", 4: "Glass"}
        self.areas = {351: "Demo Area A", 352: "Demo Area B", 353: "Residual bin"}
        self.capacity = 500.0
        self.buyers = ["Junk shop Sapul"]
        self.stock = {(1, 351): [40.0, 12.0], (2, 351): [25.0, 4.0], (3, 352): [10.0, 30.0], (None, 353): [15.0, None]}
        self.deliveries = []                     # dicts: id, code, source_area, category, weight_kg, at, status, ...
        self.batches = [{"id": 1, "code": "C-0001", "day": today.date().isoformat(), "start_kg": 30.0,
                         "finished_at": None, "finished_kg": None}]
        self.wet_used = 30.0                     # kg of wet waste put into batches
        self.wet_in = 50.0                       # kg of wet waste from segregation checks (20 kg waiting)
        self.compost_out = 0.0
        self.outgoing = {}                       # client_key -> reply
        self.reading = None                      # the scale's last reading: {"kg", "stable", "at"}
        self.counters = {"D": 0, "O": 0, "C": 1}
        for area, cat, w, status in (("Purok 1", "Biodegradable", 6.5, "awaiting"),
                                     ("Purok 3", "Mixed household", 4.0, "awaiting"),
                                     ("Purok 2", "Recyclable", 3.2, "separated")):
            d = self.new_delivery(area, cat, w)
            if status == "separated":
                d.update(status="separated", wet_kg=0.0, dry_kg=w)

    def code(self, prefix):
        self.counters[prefix] += 1
        return f"{prefix}-{self.counters[prefix]:04d}"

    def new_delivery(self, area, cat, w):
        d = {"id": len(self.deliveries) + 1, "code": self.code("D"), "source_area": area, "category": cat,
             "weight_kg": kg(w), "at": datetime.now(), "status": "awaiting", "wet_kg": None, "dry_kg": None,
             "moves": []}
        self.deliveries.append(d)
        return d

    def delivery(self, did):
        for d in self.deliveries:
            if d["id"] == did:
                return d
        raise AppError(404, "Delivery not found.")

    def area(self, aid):
        if aid not in self.areas:
            raise AppError(422, "Pick an active storage area.")
        return aid

    def have(self, mat, area):
        return self.stock.get((mat, area), [0.0, None])[0]

    def add_stock(self, mat, area, amount, price=None):
        row = self.stock.setdefault((mat, area), [0.0, None])
        row[0] = kg(row[0] + amount)
        if price is not None:
            row[1] = price

    def wet_waiting(self):
        return kg(self.wet_in - self.wet_used)

    def compost_on_hand(self):
        made = sum(b["finished_kg"] or 0 for b in self.batches)
        return kg(made - self.compost_out)

    def area_kg(self, aid):
        return sum(v[0] for (m, a), v in self.stock.items() if a == aid)

    # ----- device routes (the kiosk's key)
    def kiosk_state(self):
        today = [d for d in self.deliveries if d["at"].date() == datetime.now().date()]
        return {"today": {"kg": round(sum(d["weight_kg"] for d in today), 1), "drops": len(today)},
                "alerts": [], "actuators": {"light": "green", "buzz": 0}}

    def kiosk_recent(self):
        return [{"code": d["code"], "time": d["at"].strftime("%I:%M %p").lstrip("0"), "purok": d["source_area"],
                 "cat": d["category"], "kg": f"{d['weight_kg']:.1f} kg"} for d in reversed(self.deliveries[-6:])]

    def login(self, b):
        op = self.operators.get(int(b.get("user_id", 0)))
        if not op or str(b.get("pin")) != op[1]:
            raise AppError(401, "Wrong PIN.")
        token = secrets.token_hex(16)
        self.sessions[token] = int(b["user_id"])
        print("signed in:", op[0])
        return {"token": token, "name": op[0]}

    def reading_in(self, b):
        self.reading = {"kg": float(b["kg"]), "stable": bool(b.get("stable")), "at": datetime.now()}
        print(f"scale {self.reading['kg']:.2f} kg {'steady' if self.reading['stable'] else 'settling'}")
        return {"ok": True}

    def drop_off(self, b):
        if b.get("weighed_by") == "scale":  # SapulMRF's rule: the weight is the scale's latest steady reading
            r = self.reading
            if not r or datetime.now() - r["at"] > READING_MAX_AGE:
                raise AppError(422, "KIOSK has no recent reading. Weigh it again, or record it as manual.")
            if not r["stable"]:
                raise AppError(422, "KIOSK hasn't settled. Wait for a stable reading.")
            if abs(r["kg"] - float(b["weight_kg"])) > 0.001:
                raise AppError(422, "Say why you corrected the scale reading.")
        for d in self.deliveries:
            if b.get("client_key") and d.get("client_key") == b["client_key"]:
                return {"code": d["code"]}   # a retry: stored once
        if not re.fullmatch(r"Purok \d+", str(b.get("source_area", ""))):
            raise AppError(422, f"Unknown purok: {b.get('source_area')}.")
        d = self.new_delivery(b["source_area"], b.get("category", "Mixed household"), b["weight_kg"])
        d["client_key"] = b.get("client_key")
        print(f"drop-off {d['code']}: {d['source_area']} · {d['category']} · {d['weight_kg']} kg "
              f"({b.get('weighed_by')}){' · photo' if b.get('photo') else ''}")
        return {"code": d["code"]}

    # ----- operator routes (their session)
    def overview(self):
        today = [d for d in self.deliveries if d["at"].date() == datetime.now().date()]
        return {"today_deliveries": len(today), "today_kg": round(sum(d["weight_kg"] for d in today), 1),
                "awaiting_segregation": sum(d["status"] == "awaiting" for d in self.deliveries),
                "wet_waiting_kg": self.wet_waiting(),
                "areas": [{"name": n, "pct": round(100 * self.area_kg(a) / self.capacity)} for a, n in self.areas.items()]}

    def intake(self, q):
        want = "separated" if q.get("waiting") == "sorting" else "awaiting"
        return [{"id": d["id"], "code": d["code"], "source_area": d["source_area"], "category": d["category"],
                 "weight_kg": d["weight_kg"],
                 "segregation": {"wet_kg": d["wet_kg"], "dry_kg": d["dry_kg"]} if d["dry_kg"] is not None else None}
                for d in self.deliveries if d["status"] == want]

    def segregate(self, did, b):
        d = self.delivery(did)
        if d["status"] != "awaiting":
            raise AppError(409, f"{d['code']} already has its wet/dry check.")
        wet, dry = kg(b["wet_kg"]), kg(b["dry_kg"])
        if wet < 0 or dry < 0 or abs(wet + dry - d["weight_kg"]) > 0.05:
            raise AppError(422, f"Wet + dry must add up to {d['weight_kg']} kg.")
        d.update(status="separated", wet_kg=wet, dry_kg=dry)
        self.wet_in += wet
        return {"ok": True}

    def sort(self, b):
        d = self.delivery(int(b["delivery_id"]))
        if d["status"] != "separated":
            raise AppError(409, f"{d['code']} isn't waiting to be sorted.")
        for r in b["rows"]:
            mat = r.get("material_id")
            if mat is not None and mat not in self.materials:
                raise AppError(422, "Pick a recyclable material, or leave it empty for residual.")
            area = self.area(int(r["storage_area_id"]))
            if self.area_kg(area) + float(r["kg"]) > self.capacity:
                raise AppError(409, f"{self.areas[area]} is full.")
        for r in b["rows"]:
            self.add_stock(r.get("material_id"), int(r["storage_area_id"]), float(r["kg"]))
            d["moves"].append((r.get("material_id"), int(r["storage_area_id"]), float(r["kg"])))
        d["status"] = "sorted"
        return {"ok": True}

    def compost(self):
        return {"wet_waiting_kg": self.wet_waiting(), "batches": self.batches}

    def start_batch(self, b):
        start = kg(b["start_kg"])
        if start <= 0 or start > self.wet_waiting() + SLACK_KG:
            raise AppError(422, f"Only {self.wet_waiting()} kg of wet waste is waiting.")
        self.wet_used += start
        c = {"id": len(self.batches) + 1, "code": self.code("C"), "day": datetime.now().date().isoformat(),
             "start_kg": start, "finished_at": None, "finished_kg": None}
        self.batches.append(c)
        return c

    def activity(self, bid, b):
        batch = next((x for x in self.batches if x["id"] == bid), None)
        if not batch or batch["finished_at"]:
            raise AppError(409, "That batch isn't open.")
        if b.get("finished_kg") is not None:
            batch.update(finished_at=datetime.now().isoformat(), finished_kg=kg(b["finished_kg"]))
        return {"ok": True}

    def inventory(self):
        return {"stock": [{"material_id": m, "material": self.materials.get(m, "Residual"), "storage_area_id": a,
                           "area": self.areas[a], "kg": v[0], "last_price_per_kg": v[1]}
                          for (m, a), v in self.stock.items()]}

    def outgoing_in(self, b):
        key = b.get("client_key")
        if key in self.outgoing:
            return self.outgoing[key]   # a resend: not counted twice
        kind, amount, price = b["type"], kg(b["kg"]), b.get("price_per_kg")
        if amount <= 0:
            raise AppError(422, "Enter the kg.")
        if kind == "giveaway" and price is not None:
            raise AppError(422, "A give-away has no price.")
        if kind == "sale" and not price:
            raise AppError(422, "Enter the price per kg.")
        if kind == "compost":
            if amount > self.compost_on_hand() + 0.001:
                raise AppError(409, f"Only {self.compost_on_hand()} kg of compost is on hand.")
            self.compost_out += amount
        else:
            mat = b.get("material_id") if kind in ("sale", "giveaway") else None
            area = self.area(int(b["storage_area_id"]))
            if amount > self.have(mat, area) + 0.001:
                raise AppError(409, f"Only {self.have(mat, area)} kg of {self.materials.get(mat, 'residual')} "
                                    f"is in {self.areas[area]}.")
            self.add_stock(mat, area, -amount, price if kind == "sale" else None)
        o = {"code": self.code("O"), "kg": amount, "recipient": b["recipient"],
             "total": kg(amount * price) if price else None}
        self.outgoing[key] = o
        print(f"outgoing {o['code']}: {kind} {amount} kg to {b['recipient']}" + (f" at {price}/kg" if price else ""))
        return o

    def fixable(self):
        stage = {"awaiting": "recorded", "separated": "separated", "sorted": "sorted"}
        return [{"id": d["id"], "code": d["code"], "source_area": d["source_area"], "category": d["category"],
                 "weight_kg": d["weight_kg"], "stage": stage[d["status"]]}
                for d in self.deliveries if d["at"].date() == datetime.now().date()]

    def undo(self, b):
        d, to = self.delivery(int(b["delivery_id"])), b["to"]
        if not str(b.get("reason", "")).strip():
            raise AppError(422, "Say why.")
        if to == "sort" and d["status"] != "sorted":
            raise AppError(409, f"{d['code']} isn't sorted.")
        if to == "segregation" and d["dry_kg"] is None:
            raise AppError(409, f"{d['code']} has no wet/dry check.")
        for mat, area, amount in d["moves"]:
            if self.have(mat, area) < amount - 0.001:
                raise AppError(409, f"{self.materials.get(mat, 'Residual')} in {self.areas[area]} has only "
                                    f"{self.have(mat, area)} kg left. Recount it instead.")
        for mat, area, amount in d["moves"]:
            self.add_stock(mat, area, -amount)
        d.update(moves=[], status="separated" if d["dry_kg"] is not None else "awaiting")
        if to in ("segregation", "delivery") and d["dry_kg"] is not None:
            self.wet_in -= d["wet_kg"]
            d.update(status="awaiting", wet_kg=None, dry_kg=None)
        if to == "delivery":
            self.deliveries.remove(d)
        print(f"undo {d['code']} to {to}: {b['reason']}")
        return {"kind": to, "record_code": d["code"]}

    def recount(self, b):
        area = self.area(int(b["storage_area_id"]))
        self.stock.setdefault((b.get("material_id"), area), [0.0, None])[0] = kg(b["actual_kg"])
        return {"ok": True}


STORE = Store()


def route(method, path, q, body, operator):
    """(method, path) -> reply. Operator routes need a signed-in session."""
    s = STORE
    device = {
        ("GET", "/devices/kiosk-state"): s.kiosk_state,
        ("GET", "/devices/kiosk-recent"): s.kiosk_recent,
        ("GET", "/devices/kiosk-operators"): lambda: [{"id": i, "name": n} for i, (n, _) in s.operators.items()],
        ("POST", "/devices/kiosk-login"): lambda: s.login(body),
        ("POST", "/devices/deliveries"): lambda: s.drop_off(body),
        ("POST", "/devices/readings"): lambda: s.reading_in(body),
    }
    if (method, path) in device:
        return device[(method, path)]()
    if (method, path) == ("POST", "/auth/logout"):
        return {"ok": True}
    if operator is None:
        raise AppError(401, "Sign in first.")
    if m := re.fullmatch(r"/deliveries/(\d+)/segregation", path):
        return s.segregate(int(m[1]), body)
    if m := re.fullmatch(r"/compost/batches/(\d+)/activities", path):
        return s.activity(int(m[1]), body)
    if m := re.fullmatch(r"/classifications/(\d+)/confirm", path):
        return {"ok": True}
    ops = {
        ("GET", "/overview"): s.overview,
        ("GET", "/operations/intake"): lambda: s.intake(q),
        ("GET", "/masterdata/materials"): lambda: [{"id": i, "name": n} for i, n in s.materials.items()],
        ("GET", "/masterdata/storage-areas"): lambda: [{"id": i, "name": n} for i, n in s.areas.items()],
        ("GET", "/masterdata/buyers"): lambda: [{"id": i, "name": n} for i, n in enumerate(s.buyers, 1)],
        ("POST", "/sorting"): lambda: s.sort(body),
        ("GET", "/operations/compost"): s.compost,
        ("POST", "/compost/batches"): lambda: s.start_batch(body),
        ("GET", "/operations/inventory"): s.inventory,
        ("GET", "/operations/outgoing"): lambda: {"compost_on_hand_kg": s.compost_on_hand()},
        ("POST", "/outgoing"): lambda: s.outgoing_in(body),
        ("GET", "/corrections/fixable"): s.fixable,
        ("POST", "/corrections/undo"): lambda: s.undo(body),
        ("POST", "/inventory/adjustments"): lambda: s.recount(body),
        ("GET", "/classifications/pending"): lambda: [],
    }
    if (method, path) in ops:
        return ops[(method, path)]()
    raise AppError(404, f"fake API has no {method} {path}")


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def reply(self, code, obj=None, raw=None, ctype="application/json"):
        b = raw if raw is not None else json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(b)))
        self.end_headers()
        self.wfile.write(b)

    def handle_any(self, method):
        u = urlparse(self.path)
        n = int(self.headers.get("Content-Length") or 0)
        cookie = self.headers.get("Cookie", "")
        token = cookie.split("sapulmrf_session=")[1].split(";")[0] if "sapulmrf_session=" in cookie else None
        try:
            body = json.loads(self.rfile.read(n)) if n else {}
            with STORE.lock:
                operator = STORE.sessions.get(token)
                if method == "POST" and u.path == "/auth/logout":
                    STORE.sessions.pop(token, None)
                if method == "GET" and re.fullmatch(r"/deliveries/\d+/photo", u.path):
                    if operator is None:
                        raise AppError(401, "Sign in first.")
                    if GREY_JPEG is None:
                        raise AppError(404, "fake API has no photo without OpenCV")
                    return self.reply(200, raw=GREY_JPEG, ctype="image/jpeg")
                out = route(method, u.path, {k: v[0] for k, v in parse_qs(u.query).items()}, body, operator)
            created = method == "POST" and u.path in ("/devices/deliveries", "/outgoing", "/compost/batches")
            self.reply(201 if created else 200, out)
        except AppError as e:
            if e.status == 404:
                print(e.detail)  # a route the bridge needs that the fake lacks
            elif e.status != 401:
                print(f"refused ({e.status}): {e.detail}")
            self.reply(e.status, {"detail": e.detail})
        except (KeyError, ValueError, TypeError) as e:
            print(f"bad request {method} {u.path}: {e!r}")
            self.reply(422, {"detail": f"Bad request: {e}"})

    def do_GET(self):
        self.handle_any("GET")

    def do_POST(self):
        self.handle_any("POST")


def serve(port):
    srv = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="Fake SapulMRF for testing the kiosk without the web API")
    ap.add_argument("--port", type=int, default=8001)
    args = ap.parse_args()
    srv = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print(f"Fake SapulMRF on http://127.0.0.1:{args.port}  (Operator 1 PIN 1234, Operator 2 PIN 5678)")
    print("Now, in another window:")
    print(f'  $env:SAPULMRF_API="http://127.0.0.1:{args.port}"; $env:SAPULMRF_KEY="fake"')
    print("  py kiosk_bridge.py --model best.pt --cam 0 --dev")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
