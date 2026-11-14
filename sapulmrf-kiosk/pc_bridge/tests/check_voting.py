"""Camera decision over the steady window, the clearest photo, per-class thresholds, dataset pre-labels."""
import os, sys, tempfile, threading, time
import numpy as np

sys.path.insert(0, r"C:\dev\sapulmrf-kiosk\sapulmrf-kiosk\pc_bridge")
import kiosk_bridge as kb

os.chdir(tempfile.mkdtemp())

# --- vote(): majority, median counts, flicker tolerance
W = [("Recyclable", {"recyclable": 2}, 0.8)] * 9 + [("Mixed household", {"recyclable": 2, "residual": 1}, 0.5),
                                                     ("Nothing detected", {}, None), ("Recyclable", {"recyclable": 3}, 0.9)]
cat, n, counts, conf = kb.vote(W)
assert (cat, n, counts["recyclable"], counts["residual"]) == ("Recyclable", 10, 2, 0), (cat, n, counts)
assert conf == 0.81, conf
assert kb.vote([("Nothing detected", {}, None)] * 12)[:2] == ("Nothing detected", 0)

# --- yolo_labels(): COCO names mapped to the waste classes, normalised, unknown classes left out
lab = kb.yolo_labels([(100, 50, 300, 250, "bottle", 0.9, "recyclable"), (0, 0, 10, 10, "chair", 0.9, None)], 400, 300)
assert lab == "0 0.500000 0.500000 0.500000 0.666667", lab


# --- the bridge with a fake model: frames 0-11, one flickers, frame 7 is the clearest
class Box:
    def __init__(self, cls, conf, xyxy):
        self.cls, self.conf, self.xyxy = cls, conf, [np.array(xyxy, float)]


class Res:
    def __init__(self, boxes):
        self.boxes = boxes


NAMES = {0: "bottle", 1: "banana", 2: "sachet"}
PLAN = {i: [Box(0, 0.7, (10, 10, 50, 90))] for i in range(12)}
PLAN[3] = [Box(1, 0.8, (5, 5, 20, 20))]            # a banana flickers in once: Biodegradable for one frame
PLAN[7] = [Box(0, 0.95, (12, 12, 52, 92))]           # the clearest bottle
PLAN[5] = [Box(0, 0.7, (10, 10, 50, 90)), Box(2, 0.5, (60, 60, 80, 80))]   # sachet under its own 0.6 bar: skipped


class FakeModel:
    names = NAMES

    def __call__(self, frame, **kw):
        return [Res(PLAN[int(frame[0, 0, 0])])]


kb.CLASS_CONF = {"sachet": 0.6}
kb.API_URL = ""                                      # offline: the drop-off gets a K- code and the CSV
b = kb.Bridge(None, 0)
b.model, b.model_version, b.mode = FakeModel(), "best:deadbeef", "scan"
threading.Thread(target=b.infer_loop, daemon=True).start()
for i in range(12):
    b.frame, b.frame_id = np.full((90, 120, 3), i, np.uint8), b.frame_id + 1
    time.sleep(0.05)
assert b.last["category"] == "Recyclable" and b.last["stable"], b.last
assert b.last["residual"] == 0, "the sachet under its class threshold must not count"
det, frame, boxes = b.snapshot
assert int(frame[0, 0, 0]) == 7 and boxes[0][5] == 0.95, "the clearest agreeing frame is the photo"

sent = {}
kb.post_delivery = lambda body: sent.update(body) or None
kb.API_URL, kb.requests = "http://x", kb.requests
code, _ = b.save_intake({"purok": 1, "category": "Recyclable", "weight": 2, "manual": True})
assert sent["model_version"] == "best:deadbeef" and sent["materials"] == [{"label": "Plastic", "confidence": 0.95}], sent
assert sent["cv"]["category"] == "Recyclable"
img, lbl = os.path.join("dataset", "raw", "images", f"{code}.jpg"), os.path.join("dataset", "raw", "labels", f"{code}.txt")
assert os.path.exists(img) and open(lbl).read().startswith("0 "), "photo and pre-label saved for labelling"
print("check_voting: all passed")
