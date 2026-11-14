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
                     {"type":"recent"}  {"type":"stats"}  {"type":"lock"}
                     {"type":"reading","kg":4.2,"stable":true}   (HX711 scale, when fitted)
        operator mode (an MRF operator signed in with their own SapulMRF kiosk PIN):
        kiosk -> PC  {"type":"operators"}  {"type":"pin","user_id":3,"pin":"2468"}  {"type":"lock"}
                     {"type":"op","do":"today"|"waiting"|"materials"|"areas"|"segregate"|"sort"|"compost"|"compost_start"|"compost_log"|"compost_finish"|"stock"|"compost_on_hand"|"recipients"|"outgoing"|
                      "fixable"|"undo"|"area_stock"|"recount",...}  (a sort may carry
                      the camera's suggestion_id + guess_material_id: the bridge then confirms or corrects that suggestion)
                     {"type":"photo","delivery_id":12}
        PC -> kiosk  {"type":"operators","items":[{"id":3,"name":"Juan"}]}  {"type":"auth","ok":true,"name":"Juan"}
                     {"type":"op","do":...,"ok":true,"data":...} or {"ok":false,"error":"SapulMRF's message"}
  'P'  PC -> kiosk   a drop-off photo for camera review: [4 byte LE delivery id][JPEG, fits PHOTO_W x PHOTO_H]
UDP port 5001: the kiosk broadcasts "SAPULMRF <ip> <port>" every 2 s so this script can find it.

Install:  pip install ultralytics opencv-python requests websockets
Run:      python kiosk_bridge.py --model best.pt --cam 1             (finds the kiosk by itself)
          python kiosk_bridge.py --model best.pt --cam 1 192.168.1.77  (or give the IP on the kiosk screen)
"""

import argparse
import base64
import csv
import glob
import hashlib
import json
import os
import re
import socket
import struct
import threading
import time
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse
from collections import Counter, deque
from datetime import datetime
from pathlib import Path
from statistics import median

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
CLASS_CONF = {}                  # per-class minimum confidence, e.g. {"sachet": 0.6}; others use CONF
STABLE_FRAMES = 12               # the window the decision is made over
STABLE_SHARE = 10                # this many of those frames agreeing = "stable" (a flicker doesn't reset it)
SAVE_DATASET = True              # each drop-off's photo + the model's boxes go to dataset/raw for labelling
DATASET = os.path.join("dataset", "raw")
LOCAL_LOG = "kiosk_intake.csv"                 # always written, even with a backend

# SapulMRF API. The kiosk is registered there once as a weighing station (e.g. KIOSK-01);
# its key is shown once in Settings > IoT devices. Leave SAPULMRF_API empty to run offline (CSV only).
API_URL = os.environ.get("SAPULMRF_API", "").rstrip("/")   # e.g. http://127.0.0.1:8001
API_KEY = os.environ.get("SAPULMRF_KEY", "")
SHOTS = "kiosk_shots"         # kiosk screenshots (design review, thesis figures)
DEV_PORT = 8091              # --dev: localhost-only control for screenshots
OUTBOX = "outbox"            # drop-offs SapulMRF hasn't accepted yet, one JSON file each
STATE_EVERY = 5              # seconds between kiosk-state polls
RETRY_EVERY = 15             # seconds between outbox retries
COOKIE = "sapulmrf_session"  # an operator's SapulMRF session, from their kiosk PIN
PHOTO_W, PHOTO_H = 320, 180  # camera-review photo on the kiosk
OUTGOING_KINDS = ("sale", "compost", "disposal")
LIST_MAX = 8                 # rows per list sent to the kiosk: what its screen and JSON buffer hold

# Map YOUR model's class names to the three kiosk buckets (lower-case). A class in none of them is ignored, so the
# room behind the tray (chairs, a TV, people) never decides the category; start-up lists the ignored classes.
# The stock YOLO model's waste-like classes are included until a waste-trained model replaces it.
RECYCLABLE = {"bottle", "pet bottle", "plastic bottle", "can", "tin can", "aluminum can",
              "glass bottle", "carton", "cardboard", "paper", "cup", "wine glass", "book",
              "plastic_bottle", "glass_bottle"}
BIODEGRADABLE = {"food", "food waste", "leaves", "peel", "organic",
                 "banana", "apple", "orange", "broccoli", "carrot", "sandwich", "hot dog", "pizza", "donut", "cake",
                 "food_waste"}
RESIDUAL = {"sachet", "wrapper", "candy wrapper", "styro", "styrofoam", "plastic bag", "diaper", "tissue", "straw",
            "plastic_bag"}
# The kiosk camera is also the sorting camera: these classes become material suggestions in SapulMRF
# (its materials: Plastic, Paper, Cardboard, Metal). Classes not listed send no suggestion.
MATERIALS = {"bottle": "Plastic", "pet bottle": "Plastic", "plastic bottle": "Plastic",
             "can": "Metal", "tin can": "Metal", "aluminum can": "Metal",
             "paper": "Paper", "cardboard": "Cardboard", "carton": "Cardboard", "cup": "Plastic", "book": "Paper",
             "plastic_bottle": "Plastic"}
# The waste model's classes (pc_bridge/tools/data.yaml). Pre-labels map the current model's names onto them.
DATASET_CLASSES = ["plastic_bottle", "can", "glass_bottle", "paper", "cardboard", "carton", "sachet", "plastic_bag",
                   "styro", "food_waste", "leaves"]
PRELABEL = {"bottle": "plastic_bottle", "wine glass": "glass_bottle", "book": "paper",
            **{f: "food_waste" for f in ("banana", "apple", "orange", "broccoli", "carrot", "sandwich", "hot dog",
                                         "pizza", "donut", "cake")}}
IGNORED = {"person"}  # the resident's hand/arm is always in view; it must not decide the category

COLORS = {"recyclable": (122, 195, 76), "residual": (58, 163, 242), "bio": (46, 90, 122)}  # BGR


def bucket(name: str):
    """recyclable / bio / residual, or None for a class that isn't waste (ignored)."""
    n = name.lower()
    if n in RECYCLABLE:
        return "recyclable"
    if n in BIODEGRADABLE:
        return "bio"
    return "residual" if n in RESIDUAL else None


BUCKETS = ("recyclable", "residual", "bio")


def vote(window):
    """The decision over the steady window: [(category, counts, mean conf)] per frame ->
    (majority category, frames agreeing, median counts, mean confidence). A brief flicker doesn't change it."""
    seen = Counter(c for c, _, _ in window if c != "Nothing detected")
    if not seen:
        return "Nothing detected", 0, dict.fromkeys(BUCKETS, 0), None
    cat, n = seen.most_common(1)[0]
    agree = [(cnt, cf) for c, cnt, cf in window if c == cat]
    counts = {k: int(median(cnt.get(k, 0) for cnt, _ in agree)) for k in BUCKETS}
    confs = [cf for _, cf in agree if cf is not None]
    return cat, n, counts, round(sum(confs) / len(confs), 2) if confs else None


def yolo_labels(boxes, w, h):
    """YOLO-format pre-labels in DATASET_CLASSES, from the model's boxes; boxes with no waste class are left out."""
    lines = []
    for x1, y1, x2, y2, name, _conf, _b in boxes:
        cls = PRELABEL.get(name.lower(), name.lower())
        if cls in DATASET_CLASSES:
            lines.append(f"{DATASET_CLASSES.index(cls)} {(x1 + x2) / 2 / w:.6f} {(y1 + y2) / 2 / h:.6f} "
                         f"{(x2 - x1) / w:.6f} {(y2 - y1) / h:.6f}")
    return "\n".join(lines)


def model_version(path):
    """'best:1a2b3c4d': the file's name and the start of its hash, so SapulMRF can tell models apart."""
    with open(path, "rb") as f:
        return f"{Path(path).stem}:{hashlib.sha1(f.read()).hexdigest()[:8]}"[:40]


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


def api_error(r):
    """SapulMRF's own message, for the kiosk to show as-is (a 422's first field error)."""
    try:
        d = r.json().get("detail")
    except (ValueError, AttributeError):
        d = None
    if isinstance(d, list) and d:
        d = d[0].get("msg")
    return str(d or f"SapulMRF error {r.status_code}")


def photo_id(url):
    m = re.search(r"/deliveries/(\d+)/photo", url or "")
    return int(m.group(1)) if m else None


def _rows(items, keys):
    return [{k: i[k] for k in keys} for i in items[:LIST_MAX]]


def op_request(do, m):
    """One operator task from the kiosk -> (method, path, body, shape of the reply). Same routes as the website."""
    if do == "today":
        return "GET", "/overview", None, lambda o: {
            "drops": o["today_deliveries"], "kg": o["today_kg"], "segregate": o["awaiting_segregation"],
            "waiting": o.get("wet_waiting_kg", 0),
            "areas": [{"name": a["name"], "pct": a["pct"]} for a in o["areas"][:LIST_MAX]]}
    if do == "waiting":
        waiting = "sorting" if m.get("for") == "sorting" else "segregation"
        return "GET", f"/operations/intake?waiting={waiting}&limit={LIST_MAX}", None, lambda items: [
            {**{k: i[k] for k in ("id", "code", "source_area", "category", "weight_kg")},
             "dry_kg": (i.get("segregation") or {}).get("dry_kg", 0)} for i in items[:LIST_MAX]]
    if do in ("materials", "areas"):
        kind = "materials" if do == "materials" else "storage-areas"
        return "GET", f"/masterdata/{kind}?active=true", None, lambda items: _rows(items, ("id", "name"))
    if do == "segregate":
        return "POST", f"/deliveries/{int(m['delivery_id'])}/segregation", {
            "condition": "mixed" if m.get("condition") == "mixed" else "segregated",
            "wet_kg": float(m["wet_kg"]), "dry_kg": float(m["dry_kg"])}, None
    if do == "sort":
        return "POST", "/sorting", {"delivery_id": int(m["delivery_id"]), "rows": [{
            "material_id": int(m["material_id"]) if m.get("material_id") else None,  # none = residual
            "storage_area_id": int(m["area_id"]), "kg": float(m["kg"]), "bags": int(m.get("bags") or 0)}]}, None
    if do == "compost":  # wet waste waiting, and the open batches
        return "GET", "/operations/compost", None, lambda v: {
            "waiting": v["wet_waiting_kg"],
            "batches": [{k: b[k] for k in ("id", "code", "day", "start_kg")}
                        for b in v["batches"] if not b["finished_at"]][:LIST_MAX]}
    if do == "compost_start":
        return "POST", "/compost/batches", {"start_kg": float(m["kg"]), "sensor_id": None}, None
    if do in ("compost_log", "compost_finish"):
        body = ({"type": "inspection", "finished_kg": float(m["kg"])} if do == "compost_finish" else {"type": m.get("activity")})  # "type" is the message type
        if body["type"] not in ("turning", "watering", "inspection"):
            raise ValueError(f"unknown compost activity {body['type']!r}")
        return "POST", f"/compost/batches/{int(m['batch_id'])}/activities", body, None
    if do == "stock":  # what can go out for that kind, biggest first: a sale is a material, disposal is residual
        residual = m.get("kind") == "disposal"
        return "GET", "/operations/inventory", None, lambda v: [
            {"material_id": s["material_id"], "material": s["material"], "area_id": s["storage_area_id"],
             "area": s["area"], "kg": s["kg"]}
            for s in sorted(v["stock"], key=lambda s: -s["kg"])
            if s["kg"] > 0 and (s["material_id"] is None) == residual][:LIST_MAX]
    if do == "compost_on_hand":
        return "GET", "/operations/outgoing", None, lambda v: {"kg": v["compost_on_hand_kg"]}
    if do in ("recipients", "outgoing"):
        kind = m.get("kind")  # "type" is the message type
        if kind not in OUTGOING_KINDS:
            raise ValueError(f"unknown outgoing kind {kind!r}")
        if do == "recipients":
            return "GET", f"/outgoing/recipients?type={kind}", None, lambda names: names[:LIST_MAX]
        return "POST", "/outgoing", {
            "type": kind,
            "material_id": int(m["material_id"]) if kind == "sale" else None,
            "storage_area_id": int(m["area_id"]) if kind != "compost" else None,
            "kg": float(m["kg"]), "recipient": str(m["recipient"]).strip(),
            "price_per_kg": float(m["price"]) if kind == "sale" and m.get("price") else None,
            "client_key": str(m["key"]),  # made once per sale on the kiosk: a resend can't sell twice
        }, lambda o: {k: o[k] for k in ("code", "kg", "recipient", "total")}
    if do == "fixable":  # today's deliveries, to fix one
        return "GET", "/corrections/fixable", None, lambda rows: [
            {"id": x["id"], "code": x["code"], "stage": x["stage"],
             "label": f"{x['code']}  ·  {x['source_area']}  ·  {x['weight_kg']:g} kg"} for x in rows[:LIST_MAX]]
    if do == "undo":
        if m.get("to") not in ("sort", "segregation", "delivery"):
            raise ValueError(f"unknown step {m.get('to')!r}")
        return "POST", "/corrections/undo", {"delivery_id": int(m["delivery_id"]), "to": m["to"],
                                             "reason": str(m["reason"]).strip()},             lambda c: {"code": c["record_code"], "to": c["kind"]}
    if do == "area_stock":  # one area's materials (residual included), to recount one
        area = int(m["area_id"])
        return "GET", "/operations/inventory", None, lambda v: [
            {"material_id": s["material_id"], "material": s["material"], "kg": s["kg"]}
            for s in v["stock"] if s["storage_area_id"] == area][:LIST_MAX]
    if do == "recount":
        return "POST", "/inventory/adjustments", {
            "material_id": int(m["material_id"]) if m.get("material_id") else None, "storage_area_id": int(m["area_id"]),
            "actual_kg": float(m["kg"]), "reason": str(m["reason"]).strip()}, None
    raise ValueError(f"unknown operator task {do!r}")


def replaces_body(msg):
    """A drop-off recorded again in place of one an operator undid: SapulMRF links the fix to it."""
    code = str(msg.get("replaces") or "").strip()
    return {"replaces": code[:12]} if code else {}


def confirm_body(guess_material_id, material_id):
    """Sorting answers the camera's suggestion: same material = keep it, another = correct it, residual = not recyclable."""
    if not material_id:
        return {"not_recyclable": True}
    return {} if material_id == guess_material_id else {"final_material_id": material_id}


def best_guesses(pending, materials):
    """delivery id -> its top pending camera suggestion, with the suggested material's id for the kiosk."""
    ids = {x["name"].lower(): x["id"] for x in materials}
    out = {}
    for c in pending:
        did = photo_id(c["image_url"])
        if did and (did not in out or c["confidence"] > out[did]["conf"]):
            out[did] = {"suggestion_id": c["id"], "guess": c["suggested"], "conf": c["confidence"],
                        "guess_material_id": ids.get(c["suggested"].lower())}
    return out


def fit_jpeg(jpeg, w, h, quality=80):
    img = cv2.imdecode(np.frombuffer(jpeg, np.uint8), cv2.IMREAD_COLOR)
    s = min(w / img.shape[1], h / img.shape[0])
    img = cv2.resize(img, (max(1, int(img.shape[1] * s)), max(1, int(img.shape[0] * s))), interpolation=cv2.INTER_AREA)
    return cv2.imencode(".jpg", img, [cv2.IMWRITE_JPEG_QUALITY, quality])[1].tobytes()


# ---------------------------------------------------------------- state
class Bridge:
    def __init__(self, model_path, cam_index):
        self.model = YOLO(model_path) if YOLO and model_path else None
        self.model_version = model_version(model_path) if self.model else None
        self.device = gpu_device() if self.model else "cpu"
        self.cap = open_camera(cam_index) if self.model else None
        self.frame, self.frame_id = None, 0   # newest camera frame, full resolution, clean
        self.boxes = []                       # newest YOLO boxes: (x1, y1, x2, y2, name, conf, bucket)
        self.infer_ms = 0.0
        self.viewers = 0                      # people watching the live view on SapulMRF
        self.shot_name, self.shot_label, self.shot_path = "", "", ""
        self.shot_done = threading.Event()
        self.sock = None
        self.send_lock = threading.Lock()
        self.window = threading.BoundedSemaphore(IN_FLIGHT)  # free slots for frames the kiosk hasn't shown yet
        self.alive = False
        self.mode = "idle"
        self.token = None     # the signed-in operator's SapulMRF session (memory only)
        self.history = deque(maxlen=STABLE_FRAMES)
        self.last = {"recyclable": 0, "residual": 0, "bio": 0, "category": "Nothing detected"}
        self.fps = 0.0
        self.snapshot = None  # (det, clean full-res frame, boxes): the clearest steady moment of this drop-off
        self.best = None      # (mean conf, frame, boxes) of the clearest frame agreeing with the decision so far
        self.frame_info = ("Nothing detected", None)   # the newest frame's own category and mean confidence
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
        floor = min([CONF, *CLASS_CONF.values()])
        res = self.model(frame, conf=floor, imgsz=YOLO_IMGSZ, device=self.device, verbose=False)[0]
        self.infer_ms = 0.8 * self.infer_ms + 0.2 * (time.time() - t0) * 1000
        counts, confs, boxes = Counter(), [], []
        for box in res.boxes:
            name, conf = self.model.names[int(box.cls)], float(box.conf)
            b = None if name in IGNORED else bucket(name)
            if b is None or conf < CLASS_CONF.get(name.lower(), CONF):
                continue
            counts[b] += 1
            confs.append(conf)
            boxes.append((*map(int, box.xyxy[0]), name, conf, b))
        frame_conf = round(sum(confs) / len(confs), 2) if confs else None
        self.frame_info = (category_from(counts), frame_conf)
        self.history.append((self.frame_info[0], dict(counts), frame_conf))
        cat, n, voted, conf = vote(self.history)
        progress = min(1.0, n / STABLE_SHARE)
        det = {"type": "det", **voted, "category": cat, "stable": n >= STABLE_SHARE, "progress": round(progress, 2),
               "fps": round(self.fps), "conf": conf}
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
                self.best = None
                continue
            fcat, fconf = self.frame_info
            if fcat == det["category"] and fconf is not None and (self.best is None or fconf >= self.best[0]):
                self.best = (fconf, frame, self.boxes)   # the clearest frame that agrees with the decision
            if det["stable"] and self.best:
                self.snapshot = (det, self.best[1], self.best[2])  # sent with the drop-off

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
                "client_key": str(uuid.uuid4()), **replaces_body(msg)}  # same key on every retry: stored once
        if self.snapshot:
            det, frame, boxes = self.snapshot
            body["cv"] = {"category": det["category"], "confidence": det["conf"],
                          "counts": {k: det[k] for k in ("recyclable", "residual", "bio")}}
            best = {}  # one suggestion per material, with its most confident box
            for *_, name, conf, _b in boxes:
                if (m := MATERIALS.get(name.lower())) and conf > best.get(m, 0):
                    best[m] = conf
            body["materials"] = [{"label": m, "confidence": round(c, 3)} for m, c in best.items()]
            if self.model_version:
                body["model_version"] = self.model_version
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
        if SAVE_DATASET and self.snapshot:
            self.save_example(code, *self.snapshot[1:])
        new = not os.path.exists(LOCAL_LOG)
        with open(LOCAL_LOG, "a", newline="") as f:
            w = csv.DictWriter(f, fieldnames=["code"] + list(record))
            if new:
                w.writeheader()
            w.writerow({"code": code, **record})
        return code, actions

    @staticmethod
    def save_example(code, frame, boxes):
        """The drop-off photo and the model's boxes as YOLO pre-labels, to correct in a labelling tool later."""
        try:
            os.makedirs(os.path.join(DATASET, "images"), exist_ok=True)
            os.makedirs(os.path.join(DATASET, "labels"), exist_ok=True)
            cv2.imwrite(os.path.join(DATASET, "images", f"{code}.jpg"), frame, [cv2.IMWRITE_JPEG_QUALITY, PHOTO_QUALITY])
            with open(os.path.join(DATASET, "labels", f"{code}.txt"), "w") as f:
                f.write(yolo_labels(boxes, frame.shape[1], frame.shape[0]))
            with open(os.path.join(DATASET, "classes.txt"), "w") as f:
                f.write("\n".join(DATASET_CLASSES))
        except OSError as e:  # a full disk must not stop a drop-off
            print("dataset example not saved:", e)

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
            self.sign_out()  # a dropped kiosk must not leave an operator signed in
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
                elif kind == b"S":
                    self.save_shot(payload)
                elif kind == b"M":
                    try:
                        self.on_message(json.loads(payload))
                    except Exception as e:  # one bad message must not drop the kiosk link
                        print("message error:", type(e).__name__, e)
        except (OSError, ConnectionError, ValueError) as e:
            if self.alive:
                print("link error:", e)
        self.alive = False

    def save_shot(self, raw):
        """The kiosk's screen as it really looks: 800x480 RGB565 little-endian -> PNG."""
        px = np.frombuffer(raw, "<u2").reshape(480, 800).astype(np.uint16)
        bgr = np.dstack([((px & 0x1F) << 3), ((px >> 5) & 0x3F) << 2, ((px >> 11) & 0x1F) << 3]).astype(np.uint8)
        os.makedirs(SHOTS, exist_ok=True)
        n = len([f for f in os.listdir(SHOTS) if f.endswith(".png")]) + 1
        name = "-".join(x for x in (f"{n:02d}", self.shot_name or "screen", self.shot_label) if x)
        self.shot_path = os.path.abspath(os.path.join(SHOTS, name + ".png"))
        cv2.imwrite(self.shot_path, bgr)
        print("screenshot saved:", self.shot_path)
        self.shot_done.set()

    def request_shot(self, label=""):
        self.shot_label, self.shot_done = label, threading.Event()
        self.send_json({"type": "snapshot"})
        return self.shot_path if self.shot_done.wait(10) else None

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
                self.snapshot = self.best = None  # a fresh scan: forget the previous camera result
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
            self.snapshot = self.best = None
            self.send_json({"type": "receipt", "ok": True, "code": code})
            if actions.get("buzz"):
                self.send_json({"type": "actuate", "buzz": actions["buzz"]})
            self.send_json(self.stats())
        elif t == "shot":
            self.shot_name = str(msg.get("screen", ""))
        elif t == "operators":
            self.send_operators()
        elif t == "pin":
            self.sign_in(msg)
        elif t == "op":
            self.operator(msg)
        elif t == "photo":
            self.send_photo(msg)
        elif t == "recent" and self.token:
            self.send_json({"type": "recent", "rows": self.recent_rows()})
        elif t == "stats":
            self.send_json(self.stats())
        elif t == "lock":
            self.sign_out()
        elif t == "reading" and API_URL and requests:  # HX711 scale: SapulMRF checks scale weights against it
            try:
                api("POST", "/devices/readings", json={"kg": float(msg["kg"]), "stable": bool(msg.get("stable"))})
            except requests.RequestException as e:
                print("scale reading not sent:", type(e).__name__)

    # ----- operator mode: their own SapulMRF session, so the website's rules and Activity log apply
    def send_operators(self):
        r, error = None, "Operator sign-in needs SapulMRF, which isn't set up on this PC."
        if API_URL and requests:
            try:
                r = api("GET", "/devices/kiosk-operators")
                error = None if r.ok else api_error(r)
            except requests.RequestException:
                error = "SapulMRF isn't reachable."
        if error:
            self.send_json({"type": "operators", "items": [], "error": error})
        else:
            self.send_json({"type": "operators", "items": r.json()[:LIST_MAX]})

    def sign_in(self, msg):
        if not (API_URL and requests):
            return self.send_json({"type": "auth", "ok": False, "error": "Operator sign-in needs SapulMRF."})
        try:
            r = api("POST", "/devices/kiosk-login", json={"user_id": int(msg["user_id"]), "pin": str(msg["pin"])})
        except (KeyError, ValueError, TypeError):
            return self.send_json({"type": "auth", "ok": False, "error": "Pick your name again."})
        except requests.RequestException:
            return self.send_json({"type": "auth", "ok": False, "error": "SapulMRF isn't reachable."})
        if not r.ok:
            return self.send_json({"type": "auth", "ok": False, "locked": r.status_code == 423, "error": api_error(r)})
        self.sign_out()
        body = r.json()
        self.token = body["token"]
        print("operator signed in:", body["name"])
        self.send_json({"type": "auth", "ok": True, "name": body["name"]})

    def sign_out(self):
        token, self.token = self.token, None
        if token:
            try:
                requests.post(API_URL + "/auth/logout", cookies={COOKIE: token}, timeout=5)
                print("operator signed out")
            except requests.RequestException:
                pass  # the session ends by itself after 30 minutes

    def as_operator(self, method, path, body=None):
        """Returns (response, None) or (None, error message). A 401 means the session ended: the kiosk locks."""
        if not self.token:
            return None, "Sign in first."
        try:
            r = requests.request(method, API_URL + path, json=body, cookies={COOKIE: self.token}, timeout=8)
        except requests.RequestException:
            return None, "SapulMRF isn't reachable."
        if r.status_code == 401:
            self.token = None
            return None, "Signed out. Sign in again."
        return (r, None) if r.ok else (None, api_error(r))

    def operator(self, msg):
        do = msg.get("do")
        try:
            method, path, body, shape = op_request(do, msg)
        except (KeyError, ValueError, TypeError) as e:
            print("bad operator task:", e, msg)
            return self.send_json({"type": "op", "do": do, "ok": False, "error": "The kiosk sent an incomplete form."})
        r, error = self.as_operator(method, path, body)
        reply = {"type": "op", "do": do, "ok": r is not None}
        if r is None:
            reply.update(error=error, signed_out=self.token is None)
        elif shape:
            reply["data"] = shape(r.json())
            if do == "today":
                reply["data"]["sort"] = self.count_waiting("sorting")
            elif do == "waiting" and msg.get("for") == "sorting":
                self.add_guesses(reply["data"])
        if do == "sort" and r is not None and msg.get("suggestion_id"):
            reply["note"] = self.answer_suggestion(msg)
        if method == "POST":
            print("operator", do, "->", "saved" if r is not None else error, reply.get("note") or "")
        self.send_json(reply)

    def count_waiting(self, what):
        # shortcut: counts at most 200 waiting drop-offs; SapulMRF has no count route yet
        r, _ = self.as_operator("GET", f"/operations/intake?waiting={what}&limit=200")
        return len(r.json()) if r is not None else 0

    def add_guesses(self, rows):
        """Each drop-off waiting to be sorted gets the camera's suggestion, if it made one."""
        pending, _ = self.as_operator("GET", "/classifications/pending")
        mats, _ = self.as_operator("GET", "/masterdata/materials?active=true")
        guesses = best_guesses(pending.json(), mats.json()) if pending is not None and mats is not None else {}
        for row in rows:
            g = guesses.get(row["id"])
            if g:
                row.update(suggestion_id=g["suggestion_id"], guess=g["guess"], pct=round(g["conf"] * 100),
                           guess_material_id=g["guess_material_id"])

    def answer_suggestion(self, msg):
        """After a sort, confirm or correct the camera's suggestion, so SapulMRF keeps its accuracy record.
        The sort is already saved; a failure here is only reported."""
        mid = int(msg["material_id"]) if msg.get("material_id") else None
        gid = int(msg["guess_material_id"]) if msg.get("guess_material_id") else None
        r, error = self.as_operator("POST", f"/classifications/{int(msg['suggestion_id'])}/confirm",
                                    confirm_body(gid, mid))
        return None if r is not None else f"camera suggestion not updated: {error}"

    def send_photo(self, msg):
        try:
            did = int(msg["delivery_id"])
        except (KeyError, ValueError, TypeError):
            return
        r, error = self.as_operator("GET", f"/deliveries/{did}/photo")
        if r is None:
            return self.send_json({"type": "photo", "delivery_id": did, "ok": False, "error": error})
        self.send_packet(b"P", struct.pack("<I", did) + fit_jpeg(r.content, PHOTO_W, PHOTO_H))

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


def dev_server(bridge):
    """--dev only, localhost only: /shot?label=x saves a kiosk screenshot, /goto?screen=weigh switches
    the kiosk's screen (needs DEV_TOOLS 1 in config.h), /signin?user_id=3&pin=1234 signs an operator in like the
    PIN pad, /send?json={...} passes a message to the kiosk."""
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def do_GET(self):
            u = urlparse(self.path)
            q = {k: v[0] for k, v in parse_qs(u.query).items()}
            try:
                if not bridge.alive:
                    raise RuntimeError("kiosk not connected")
                if u.path == "/shot":
                    path = bridge.request_shot(q.get("label", ""))
                    out = {"ok": bool(path), "path": path}
                elif u.path == "/goto":
                    bridge.send_json({"type": "goto", "screen": q.get("screen", "")})
                    time.sleep(0.6)
                    out = {"ok": True}
                elif u.path == "/signin":  # sign an operator in as the PIN pad would: /signin?user_id=3&pin=1234
                    bridge.sign_in(q)
                    time.sleep(1.5)
                    out = {"ok": bridge.token is not None}
                elif u.path == "/op":  # run an operator task as if the kiosk sent it: /op?json={"do":"stock"}
                    bridge.operator({"type": "op", **json.loads(q["json"])})
                    time.sleep(1.0)
                    out = {"ok": True}
                elif u.path == "/send":
                    bridge.send_json(json.loads(q["json"]))
                    time.sleep(0.4)
                    out = {"ok": True}
                else:
                    out = {"ok": False, "error": "use /shot, /goto, /signin, /op or /send"}
            except Exception as e:
                out = {"ok": False, "error": str(e)}
            body = json.dumps(out).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    srv = ThreadingHTTPServer(("127.0.0.1", DEV_PORT), Handler)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    print(f"Dev control: http://127.0.0.1:{DEV_PORT}/shot  /goto?screen=weigh  /send?json=...")


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
    ap.add_argument("--dev", action="store_true", help="localhost control for kiosk screenshots (design review)")
    args = ap.parse_args()

    if YOLO is None:
        raise SystemExit("Ultralytics isn't installed: pip install ultralytics")
    bridge = Bridge(args.model, args.cam)
    h, w = bridge.cap.read()[1].shape[:2]
    bridge.infer(np.zeros((h, w, 3), np.uint8))  # warm-up: the first YOLO call is slow
    bridge.history.clear()
    where = f"GPU {bridge.device}" if bridge.device != "cpu" else "CPU"
    print(f"Camera {args.cam}: {w}x{h} MJPG | YOLO on {where} | warm-up {bridge.infer_ms:.0f} ms")
    skipped = [n for n in bridge.model.names.values() if bucket(n) is None]
    if skipped:
        print(f"Not waste, ignored ({len(skipped)} of {len(bridge.model.names)} classes):", ", ".join(skipped[:12]),
              "..." if len(skipped) > 12 else "")
    if args.dev:
        dev_server(bridge)
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
