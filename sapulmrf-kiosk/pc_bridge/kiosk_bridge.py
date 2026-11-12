"""
SapulMRF Kiosk bridge  (runs on the PC with the USB camera)

Works like the SapulMRF stream: the KIOSK (ESP32-S3 7") is the server and this
script connects to it. The kiosk joins your Wi-Fi or a phone hotspot, or makes
its own hotspot "SapulMRF-Kiosk" (then it is always 192.168.4.1).
The PC just has to be on the same network as the kiosk.

  USB cam -> YOLO -> annotated JPEG frames + detection summary -> kiosk
  kiosk  -> intake records / PIN / recent list -> this script -> SapulMRF API (+ CSV log, outbox when offline)
  SapulMRF API -> today's totals, alerts, light/buzzer -> this script -> kiosk   (polled every STATE_EVERY s)

TCP port 5000, packets both ways:  [1 byte type][4 byte little-endian length][payload]
  'J'  PC -> kiosk   JPEG frame (FRAME_W x FRAME_H), only while the kiosk is on the scan screen
  'A'  kiosk -> PC   ACK, frame shown: send the next one (same flow control as the stream)
  'M'  both ways     JSON:
        PC -> kiosk  {"type":"det","recyclable":2,"residual":1,"bio":0,"category":"Mixed household","stable":true,"fps":8}
                     {"type":"receipt","ok":true,"code":"K-2854"} (ok:false = not saved)  {"type":"auth","ok":true}
                     {"type":"recent","rows":[...]}      {"type":"stats","kg":128.4,"drops":17}
                     {"type":"alerts","items":[{"kind":"storage","title":"...","details":"..."}]}
                     {"type":"actuate","light":"green"|"amber"|"red","buzz":1}
        kiosk -> PC  {"type":"mode","mode":"scan"|"idle"}  {"type":"intake",...}
                     {"type":"pin","pin":"1234"}  {"type":"recent"}  {"type":"stats"}  {"type":"lock"}
                     {"type":"reading","kg":4.2,"stable":true}   (HX711 scale, when fitted)
UDP port 5001: the kiosk broadcasts "SAPULMRF <ip> <port>" every 2 s so this script can find it.

Install:  pip install ultralytics opencv-python requests websockets
Run:      python kiosk_bridge.py --model best.pt --cam 1             (finds the kiosk by itself)
          python kiosk_bridge.py --model best.pt --cam 1 192.168.1.77  (or give the IP on the kiosk screen)
"""

import argparse
import base64
import csv
import glob
import json
import os
import socket
import struct
import threading
import time
import uuid
from collections import Counter, deque
from datetime import datetime

import cv2
import numpy as np

try:
    import requests
except ImportError:
    requests = None

try:
    from websockets.sync.client import connect as ws_connect
except ImportError:          # live view on the website is optional
    ws_connect = None

try:
    from ultralytics import YOLO
except ImportError:          # lets the network part be tested without YOLO installed
    YOLO = None

STREAM_PORT = 5000
BEACON_PORT = 5001
HOTSPOT_IP = "192.168.4.1"   # kiosk's own hotspot

# ---------------------------------------------------------------- settings
FRAME_W, FRAME_H = 480, 360      # the video box on the kiosk
SEND_W, SEND_H = 240, 180        # what the kiosk gets: half size, it doubles each pixel (4x less to decode)
KIOSK_JPEG_QUALITY = 70          # the frame is tiny, so quality is cheap here
IN_FLIGHT = 2                    # frames sent before the kiosk's ACK: hides the Wi-Fi round trip
SEND_FPS = 15                    # frames per second sent to the kiosk
PHOTO_QUALITY = 90               # drop-off photo for SapulMRF: full camera resolution, high quality
CAPTURE_MODES = [(1920, 1080), (1280, 720), (640, 480)]   # best first; the first the camera delivers wins
CAM_FPS = 30
LIVE_QUALITY = 90                # live view on SapulMRF's website: full resolution, high quality
LIVE_FPS = 30                    # cap; the camera's own rate is usually the limit
YOLO_IMGSZ = 640
CONF = 0.45
STABLE_FRAMES = 12               # same category this many frames in a row = "stable"
OPERATOR_PINS = {"1234": "Operator"}          # change these!
LOCAL_LOG = "kiosk_intake.csv"                 # always written, even with a backend

# SapulMRF API. The kiosk is registered there once as a weighing station (e.g. KIOSK-01);
# its key is shown once in Settings > IoT devices. Leave SAPULMRF_API empty to run offline (CSV only).
API_URL = os.environ.get("SAPULMRF_API", "").rstrip("/")   # e.g. http://127.0.0.1:8001
API_KEY = os.environ.get("SAPULMRF_KEY", "")
OUTBOX = "outbox"            # drop-offs SapulMRF hasn't accepted yet, one JSON file each
STATE_EVERY = 5              # seconds between kiosk-state polls
RETRY_EVERY = 15             # seconds between outbox retries

# Map YOUR model's class names to the three kiosk buckets (lower-case).
RECYCLABLE = {"bottle", "pet bottle", "plastic bottle", "can", "tin can", "aluminum can",
              "glass bottle", "carton", "cardboard", "paper"}
BIODEGRADABLE = {"food", "food waste", "leaves", "peel", "organic"}
# anything else = residual (sachets, wrappers, styro, etc.)
# The kiosk camera is also the sorting camera: these classes become material suggestions in SapulMRF
# (its materials: Plastic, Paper, Cardboard, Metal). Classes not listed send no suggestion.
MATERIALS = {"bottle": "Plastic", "pet bottle": "Plastic", "plastic bottle": "Plastic",
             "can": "Metal", "tin can": "Metal", "aluminum can": "Metal",
             "paper": "Paper", "cardboard": "Cardboard", "carton": "Cardboard"}
IGNORED = {"person"}  # the resident's hand/arm is always in view; it must not decide the category

COLORS = {"recyclable": (122, 195, 76), "residual": (58, 163, 242), "bio": (46, 90, 122)}  # BGR


def bucket(name: str) -> str:
    n = name.lower()
    if n in RECYCLABLE:
        return "recyclable"
    if n in BIODEGRADABLE:
        return "bio"
    return "residual"


def category_from(counts: Counter) -> str:
    kinds = [k for k in ("recyclable", "residual", "bio") if counts[k] > 0]
    if not kinds:
        return "Nothing detected"
    if kinds == ["recyclable"]:
        return "Recyclable"
    if kinds == ["bio"]:
        return "Biodegradable"
    if kinds == ["residual"]:
        return "Residual"
    return "Mixed household"


def gpu_device():
    """CUDA GPU 0 when torch can see one, else the CPU."""
    try:
        import torch
        return 0 if torch.cuda.is_available() else "cpu"
    except ImportError:
        return "cpu"


def open_camera(index):
    """Open the camera in MJPG at the best mode in CAPTURE_MODES it really delivers."""
    backend = cv2.CAP_DSHOW if os.name == "nt" else cv2.CAP_ANY
    for w, h in CAPTURE_MODES:
        cap = cv2.VideoCapture(index, backend)
        cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))  # without MJPG, HD modes crawl over USB
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, w)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, h)
        cap.set(cv2.CAP_PROP_FPS, CAM_FPS)
        ok, frame = cap.read()
        if ok and frame.shape[1] == w and frame.shape[0] == h:
            return cap
        cap.release()
    raise SystemExit(f"Camera {index} gives no picture. Try another --cam number.")


class Rejected(Exception):
    """SapulMRF refused a drop-off for good (422): sending it again won't help."""


def api(method, path, **kw):
    """Call SapulMRF with the kiosk's key."""
    return requests.request(method, API_URL + path, headers={"X-Device-Key": API_KEY}, timeout=5, **kw)


def post_delivery(body):
    """Send one drop-off. Returns the receipt, or None when it should be retried later."""
    try:
        r = api("POST", "/devices/deliveries", json=body)
    except requests.RequestException as e:
        print("SapulMRF unreachable:", type(e).__name__)
        return None
    if r.status_code == 201:
        return r.json()
    if r.status_code in (401, 403):
        print("SapulMRF refused the kiosk key; check SAPULMRF_KEY:", r.text)
        return None
    if r.status_code >= 500:
        return None
    raise Rejected(r.text)


# ---------------------------------------------------------------- state
class Bridge:
    def __init__(self, model_path, cam_index):
        self.model = YOLO(model_path) if YOLO and model_path else None
        self.device = gpu_device() if self.model else "cpu"
        self.cap = open_camera(cam_index) if self.model else None
        self.frame, self.frame_id = None, 0   # newest camera frame, full resolution, clean
        self.boxes = []                       # newest YOLO boxes: (x1, y1, x2, y2, name, conf, bucket)
        self.infer_ms = 0.0
        self.viewers = 0                      # people watching the live view on SapulMRF
        self.sock = None
        self.send_lock = threading.Lock()
        self.window = threading.BoundedSemaphore(IN_FLIGHT)  # free slots for frames the kiosk hasn't shown yet
        self.alive = False
        self.mode = "idle"
        self.auth = False
        self.history = deque(maxlen=STABLE_FRAMES)
        self.last = {"recyclable": 0, "residual": 0, "bio": 0, "category": "Nothing detected"}
        self.fps = 0.0
        self.snapshot = None  # (det, clean full-res frame, boxes) at the last stable moment of this drop-off
        self.state = None     # last kiosk-state from SapulMRF
        self.next_code = self._load_next_code()

    # ----- camera -> newest frame (own thread, full resolution)
    def capture_loop(self):
        while True:
            ok, frame = self.cap.read()
            if not ok:
                time.sleep(0.05)
                continue
            self.frame, self.frame_id = frame, self.frame_id + 1

    # ----- newest frame -> YOLO -> boxes + summary (own thread, as fast as the GPU goes)
    def infer(self, frame):
        """YOLO on one frame. Returns (det summary, boxes) and updates the stability history."""
        t0 = time.time()
        res = self.model(frame, conf=CONF, imgsz=YOLO_IMGSZ, device=self.device, verbose=False)[0]
        self.infer_ms = 0.8 * self.infer_ms + 0.2 * (time.time() - t0) * 1000
        counts, confs, boxes = Counter(), [], []
        for box in res.boxes:
            name = self.model.names[int(box.cls)]
            if name in IGNORED:
                continue
            b = bucket(name)
            counts[b] += 1
            confs.append(float(box.conf))
            boxes.append((*map(int, box.xyxy[0]), name, float(box.conf), b))
        cat = category_from(counts)
        self.history.append(cat)
        same = 0
        for c in reversed(self.history):
            if c != cat:
                break
            same += 1
        progress = 0.0 if cat == "Nothing detected" else same / STABLE_FRAMES
        det = {"type": "det", "recyclable": counts["recyclable"], "residual": counts["residual"],
               "bio": counts["bio"], "category": cat, "stable": progress >= 1, "progress": round(progress, 2),
               "fps": round(self.fps), "conf": round(sum(confs) / len(confs), 2) if confs else None}
        return det, boxes

    def infer_loop(self):
        seen = 0
        while True:
            frame, fid = self.frame, self.frame_id
            scanning = self.mode == "scan"
            if (not scanning and self.viewers <= 0) or frame is None or fid == seen:
                if not scanning:
                    self.history.clear()
                time.sleep(0.005)
                continue
            seen = fid
            det, self.boxes = self.infer(frame)
            self.last = det
            if not scanning:
                self.history.clear()  # only a resident's scan may become "stable", not someone watching online
            elif det["stable"]:
                self.snapshot = (det, frame, self.boxes)  # what the camera decided, sent with the drop-off

    def annotated(self, frame, boxes):
        out = frame.copy()
        k = max(1.0, out.shape[1] / 640)  # line and text size follow the resolution
        for x1, y1, x2, y2, name, conf, b in boxes:
            color = COLORS[b]
            cv2.rectangle(out, (x1, y1), (x2, y2), color, int(3 * k))
            label = f"{name} {conf:.2f}"
            (tw, th), _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.6 * k, int(2 * k))
            cv2.rectangle(out, (x1, y1 - th - int(8 * k)), (x1 + tw + int(8 * k), y1), color, -1)
            cv2.putText(out, label, (x1 + int(4 * k), y1 - int(5 * k)), cv2.FONT_HERSHEY_SIMPLEX, 0.6 * k,
                        (20, 20, 20), int(2 * k))
        return out

    def kiosk_jpeg(self, frame):
        """Centre-crop to the kiosk's 4:3 box, shrink to SEND_W x SEND_H, light JPEG the ESP32 decodes fast."""
        h, w = frame.shape[:2]
        cw = min(w, h * SEND_W // SEND_H)
        ch = cw * SEND_H // SEND_W
        x, y = (w - cw) // 2, (h - ch) // 2
        small = cv2.resize(frame[y:y + ch, x:x + cw], (SEND_W, SEND_H), interpolation=cv2.INTER_AREA)
        ok, jpg = cv2.imencode(".jpg", small, [cv2.IMWRITE_JPEG_QUALITY, KIOSK_JPEG_QUALITY])
        return jpg.tobytes() if ok else None

    # ----- intake records
    def _load_next_code(self):
        last = 2849
        if os.path.exists(LOCAL_LOG):
            with open(LOCAL_LOG, newline="") as f:
                for row in csv.DictReader(f):
                    try:
                        last = max(last, int(row["code"].split("-")[1]))
                    except (KeyError, ValueError, IndexError):
                        pass
        return last + 1

    def save_intake(self, msg):
        record = {
            "received": datetime.now().isoformat(timespec="seconds"),
            "source_area": f"Purok {int(msg.get('purok', 0))}",
            "category": msg.get("category", "Mixed household"),
            "weight_kg": round(float(msg.get("weight", 0)), 2),
            "manual": bool(msg.get("manual", False)),
            "recyclables_seen": int(msg.get("recyclables", 0)),
            "recorded_by": "Kiosk",
        }
        body = {"source_area": record["source_area"], "category": record["category"],
                "weight_kg": record["weight_kg"], "weighed_by": "manual" if record["manual"] else "scale",
                "client_key": str(uuid.uuid4())}  # same key on every retry, so SapulMRF stores it once
        if self.snapshot:
            det, frame, boxes = self.snapshot
            body["cv"] = {"category": det["category"], "confidence": det["conf"],
                          "counts": {k: det[k] for k in ("recyclable", "residual", "bio")}}
            best = {}  # one suggestion per material, with its most confident box
            for *_, name, conf, _b in boxes:
                if (m := MATERIALS.get(name.lower())) and conf > best.get(m, 0):
                    best[m] = conf
            body["materials"] = [{"label": m, "confidence": round(c, 3)} for m, c in best.items()]
            ok, jpg = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, PHOTO_QUALITY])
            if ok:
                body["photo"] = base64.b64encode(jpg.tobytes()).decode()
        code, actions = None, {}
        if API_URL and requests:
            try:
                receipt = post_delivery(body)
            except Rejected as e:  # e.g. a purok name SapulMRF doesn't know: keep it locally for the operator
                print("SapulMRF rejected the drop-off, kept in outbox/rejected:", e)
                self.to_outbox(body, os.path.join(OUTBOX, "rejected"))
                receipt = None
            else:
                if receipt is None:
                    self.to_outbox(body, OUTBOX)
            if receipt:
                code, actions = receipt["code"], receipt.get("actions", {})
        if not code:
            code = f"K-{self.next_code}"  # K = made by the kiosk; D- codes come from SapulMRF
            self.next_code += 1
        new = not os.path.exists(LOCAL_LOG)
        with open(LOCAL_LOG, "a", newline="") as f:
            w = csv.DictWriter(f, fieldnames=["code"] + list(record))
            if new:
                w.writeheader()
            w.writerow({"code": code, **record})
        return code, actions

    @staticmethod
    def to_outbox(body, folder):
        os.makedirs(folder, exist_ok=True)
        with open(os.path.join(folder, body["client_key"] + ".json"), "w") as f:
            json.dump(body, f)

    def live_loop(self):
        """Full-quality camera for SapulMRF's Live camera page, sent only while someone is watching."""
        url = API_URL.replace("http", "ws", 1) + "/devices/kiosk-live"
        wait, down = 2, False
        while True:
            try:
                with ws_connect(url, additional_headers={"X-Device-Key": API_KEY}, open_timeout=5) as ws:
                    if down:
                        print("Live view: connected to SapulMRF again")
                    wait, down = 2, False
                    closed = threading.Event()
                    threading.Thread(target=self._live_listen, args=(ws, closed), daemon=True).start()
                    sent = 0
                    while not closed.is_set():
                        frame, fid = self.frame, self.frame_id
                        if self.viewers <= 0 or frame is None or fid == sent:
                            time.sleep(0.01)
                            continue
                        t0, sent = time.time(), fid
                        ok, jpg = cv2.imencode(".jpg", self.annotated(frame, self.boxes),
                                               [cv2.IMWRITE_JPEG_QUALITY, LIVE_QUALITY])
                        if ok:
                            ws.send(jpg.tobytes())
                            ws.send(json.dumps({"det": self.last, "w": frame.shape[1], "h": frame.shape[0]}))
                        time.sleep(max(0.0, 1 / LIVE_FPS - (time.time() - t0)))
            except Exception as e:  # SapulMRF down or restarting, or the key is wrong
                if not down:
                    print("Live view: can't reach SapulMRF, retrying:", type(e).__name__, e)
                down = True
            self.viewers = 0
            time.sleep(wait)
            wait = min(wait * 2, 30)

    def _live_listen(self, ws, closed):
        """SapulMRF says how many people watch; frames are only encoded while that's above 0."""
        try:
            for msg in ws:
                n = json.loads(msg).get("viewers") if isinstance(msg, str) else None
                if isinstance(n, int) and n != self.viewers:
                    self.viewers = n
                    print(f"Live view: {n} watching")
        except Exception:
            pass
        self.viewers = 0
        closed.set()

    def outbox_loop(self):
        """Send drop-offs saved while SapulMRF was down, oldest first, until it accepts or rejects them."""
        while True:
            for path in sorted(glob.glob(os.path.join(OUTBOX, "*.json")), key=os.path.getmtime):
                with open(path) as f:
                    body = json.load(f)
                try:
                    receipt = post_delivery(body)
                except Rejected as e:
                    print("outbox: SapulMRF rejected", body["client_key"], e)
                    os.makedirs(os.path.join(OUTBOX, "rejected"), exist_ok=True)
                    os.replace(path, os.path.join(OUTBOX, "rejected", os.path.basename(path)))
                    continue
                if receipt is None:
                    break  # still offline: try again later, keeping the order
                os.remove(path)
                print("outbox: sent, SapulMRF code", receipt["code"])
            time.sleep(RETRY_EVERY)

    def recent(self, n=6):
        if not os.path.exists(LOCAL_LOG):
            return []
        with open(LOCAL_LOG, newline="") as f:
            rows = list(csv.DictReader(f))[-n:]
        out = []
        for r in reversed(rows):
            t = datetime.fromisoformat(r["received"])
            out.append({"code": r["code"], "time": t.strftime("%I:%M %p").lstrip("0"),
                        "purok": r["source_area"], "cat": r["category"], "kg": f"{float(r['weight_kg']):.1f} kg"})
        return out

    def stats_today(self):
        kg, drops = 0.0, 0
        if os.path.exists(LOCAL_LOG):
            today = datetime.now().date()
            with open(LOCAL_LOG, newline="") as f:
                for r in csv.DictReader(f):
                    if datetime.fromisoformat(r["received"]).date() == today:
                        kg += float(r["weight_kg"])
                        drops += 1
        return {"type": "stats", "kg": round(kg, 1), "drops": drops}

    def stats(self):
        """Today's totals from SapulMRF, or from the local CSV while it can't be reached."""
        return {"type": "stats", **self.state["today"]} if self.state else self.stats_today()

    def recent_rows(self):
        if API_URL and requests:
            try:
                r = api("GET", "/devices/kiosk-recent")
                if r.ok:
                    return r.json()
            except requests.RequestException:
                pass
        return self.recent()

    def state_loop(self):
        """Poll SapulMRF for the kiosk's totals, alerts and light, and pass them on."""
        while self.alive:
            try:
                r = api("GET", "/devices/kiosk-state")
                self.state = r.json() if r.ok else None
            except requests.RequestException:
                self.state = None
            try:
                self.send_json(self.stats())
                if self.state:
                    self.send_json({"type": "alerts", "items": self.state["alerts"]})
                    self.send_json({"type": "actuate", **self.state["actuators"]})
            except OSError:
                return  # the kiosk link dropped; connect_and_serve starts a new loop
            time.sleep(STATE_EVERY)


    # ----- link to the kiosk
    def connect_and_serve(self, host, port):
        sock = socket.create_connection((host, port), timeout=5)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        sock.settimeout(None)
        self.sock = sock
        self.mode = "idle"
        self.auth = False
        self.alive = True
        self.window = threading.BoundedSemaphore(IN_FLIGHT)
        print(f"Connected to kiosk {host}:{port}")
        reader = threading.Thread(target=self.read_loop, daemon=True)
        reader.start()
        if API_URL and requests:
            threading.Thread(target=self.state_loop, daemon=True).start()
        else:
            self.send_json(self.stats_today())
        try:
            self.stream_loop()
        finally:
            self.alive = False
            try:
                sock.close()
            except OSError:
                pass
            print("Kiosk disconnected")

    def send_packet(self, kind: bytes, payload: bytes):
        with self.send_lock:
            self.sock.sendall(kind + struct.pack("<I", len(payload)) + payload)

    def send_json(self, obj):
        self.send_packet(b"M", json.dumps(obj, separators=(",", ":")).encode())

    def recv_exact(self, n):
        buf = b""
        while len(buf) < n:
            chunk = self.sock.recv(n - len(buf))
            if not chunk:
                raise ConnectionError("kiosk closed the connection")
            buf += chunk
        return buf

    def read_loop(self):
        try:
            while self.alive:
                kind, length = struct.unpack("<cI", self.recv_exact(5))
                payload = self.recv_exact(length) if length else b""
                if kind == b"A":
                    self.free_slot()
                elif kind == b"M":
                    try:
                        self.on_message(json.loads(payload))
                    except Exception as e:  # one bad message must not drop the kiosk link
                        print("message error:", type(e).__name__, e)
        except (OSError, ConnectionError, ValueError) as e:
            if self.alive:
                print("link error:", e)
        self.alive = False

    def free_slot(self):
        try:
            self.window.release()
        except ValueError:  # an ACK for a frame from before a reset: the window is already full
            pass

    def on_message(self, msg):
        t = msg.get("type")
        if t == "mode":
            self.mode = "scan" if msg.get("mode") == "scan" else "idle"
            if self.mode == "scan":
                self.snapshot = None  # a fresh scan: forget the previous camera result
            self.window = threading.BoundedSemaphore(IN_FLIGHT)
            print("kiosk mode:", self.mode)
        elif t == "intake":
            try:
                code, actions = self.save_intake(msg)
            except Exception as e:  # e.g. kiosk_intake.csv open in Excel: tell the kiosk, keep the link
                print("intake NOT saved:", type(e).__name__, e, msg)
                self.send_json({"type": "receipt", "ok": False})
                return
            print("intake saved:", code, msg)
            self.snapshot = None
            self.send_json({"type": "receipt", "ok": True, "code": code})
            if actions.get("buzz"):
                self.send_json({"type": "actuate", "buzz": actions["buzz"]})
            self.send_json(self.stats())
        elif t == "pin":
            name = OPERATOR_PINS.get(str(msg.get("pin", "")))
            self.auth = bool(name)
            self.send_json({"type": "auth", "ok": bool(name), "name": name or ""})
        elif t == "recent" and self.auth:
            self.send_json({"type": "recent", "rows": self.recent_rows()})
        elif t == "stats":
            self.send_json(self.stats())
        elif t == "lock":
            self.auth = False
        elif t == "reading" and API_URL and requests:  # HX711 scale: SapulMRF checks scale weights against it
            try:
                api("POST", "/devices/readings", json={"kg": float(msg["kg"]), "stable": bool(msg.get("stable"))})
            except requests.RequestException as e:
                print("scale reading not sent:", type(e).__name__)

    def stream_loop(self):
        """Newest frame + newest boxes to the kiosk at SEND_FPS, whatever speed YOLO runs at."""
        period = 1.0 / SEND_FPS
        while self.alive:
            if self.mode != "scan" or self.frame is None:
                time.sleep(0.05)
                continue
            t0 = time.time()
            # at most IN_FLIGHT frames on their way: newest-frame-only, no lag build-up
            if not self.window.acquire(timeout=2.0):
                print("no ACK from kiosk in 2 s, sending anyway")
            if not self.alive or self.mode != "scan":
                continue
            jpg = self.kiosk_jpeg(self.annotated(self.frame, self.boxes))
            if jpg is None:
                continue
            self.send_packet(b"J", jpg)
            self.send_json({**self.last, "fps": round(self.fps)})
            time.sleep(max(0.0, period - (time.time() - t0)))
            self.fps = 0.8 * self.fps + 0.2 / max(time.time() - t0, 1e-3)


def discover(timeout=6.0):
    """Listen for the kiosk's UDP beacon. Returns (ip, port) or None."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        s.bind(("", BEACON_PORT))
    except OSError as e:
        print("can't listen for the kiosk beacon:", e)
        return None
    s.settimeout(timeout)
    try:
        while True:
            data, addr = s.recvfrom(128)
            parts = data.decode(errors="ignore").split()
            if len(parts) == 3 and parts[0] == "SAPULMRF":
                return parts[1], int(parts[2])
    except socket.timeout:
        return None
    finally:
        s.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("host", nargs="?", help="kiosk IP shown on its welcome screen (optional)")
    ap.add_argument("--model", default="best.pt", help="your trained YOLO weights")
    ap.add_argument("--cam", type=int, default=0, help="USB camera index (external cam is usually 1)")
    ap.add_argument("--port", type=int, default=STREAM_PORT)
    args = ap.parse_args()

    if YOLO is None:
        raise SystemExit("Ultralytics isn't installed: pip install ultralytics")
    bridge = Bridge(args.model, args.cam)
    h, w = bridge.cap.read()[1].shape[:2]
    bridge.infer(np.zeros((h, w, 3), np.uint8))  # warm-up: the first YOLO call is slow
    bridge.history.clear()
    where = f"GPU {bridge.device}" if bridge.device != "cpu" else "CPU"
    print(f"Camera {args.cam}: {w}x{h} MJPG | YOLO on {where} | warm-up {bridge.infer_ms:.0f} ms")
    threading.Thread(target=bridge.capture_loop, daemon=True).start()
    threading.Thread(target=bridge.infer_loop, daemon=True).start()
    if API_URL and requests:
        print("SapulMRF API:", API_URL, "(key set)" if API_KEY else "(SAPULMRF_KEY missing!)")
        threading.Thread(target=bridge.outbox_loop, daemon=True).start()
        if ws_connect:
            threading.Thread(target=bridge.live_loop, daemon=True).start()
        else:
            print("Live view off: pip install websockets")
    else:
        print("No SAPULMRF_API set: drop-offs are saved to", LOCAL_LOG, "only")
    while True:
        host, port = args.host, args.port
        if not host:
            print("Looking for the kiosk on this network...")
            found = discover()
            if found:
                host, port = found
                print(f"Found kiosk at {host}:{port}")
            else:
                host = HOTSPOT_IP
                print(f"No beacon heard; trying the kiosk hotspot address {host}")
        try:
            bridge.connect_and_serve(host, port)
        except (OSError, ConnectionError) as e:
            print(f"Can't reach kiosk at {host}:{port} ({e}). Retrying in 3 s... (Ctrl+C to stop)")
        time.sleep(3)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nStopped")
