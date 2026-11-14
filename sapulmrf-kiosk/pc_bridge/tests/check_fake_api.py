"""fake_api.py answers every route the bridge calls, in the shapes the bridge reads, with SapulMRF's rules."""
import socket
import sys

import requests

sys.path.insert(0, r"C:\dev\sapulmrf-kiosk\sapulmrf-kiosk\pc_bridge")
import fake_api
import kiosk_bridge as kb

s = socket.socket()
s.bind(("127.0.0.1", 0))
port = s.getsockname()[1]
s.close()
fake_api.serve(port)
URL = f"http://127.0.0.1:{port}"
kb.API_URL, kb.API_KEY = URL, "fake"


def call(method, path, body=None, token=None):
    return requests.request(method, URL + path, json=body, timeout=5,
                            cookies={kb.COOKIE: token} if token else None)


def op(do, token, **msg):
    """One operator task exactly as the bridge sends it; returns the bridge's shaped reply, or the error."""
    method, path, body, shape = kb.op_request(do, msg)
    r = call(method, path, body, token)
    if not r.ok:
        return r.status_code, kb.api_error(r)
    return r.status_code, shape(r.json()) if shape else None


# device routes
assert kb.api("GET", "/devices/kiosk-state").json()["actuators"]["light"] == "green"
ops = kb.api("GET", "/devices/kiosk-operators").json()
assert ops[0] == {"id": 1, "name": "Operator 1"}, ops
bad = kb.api("POST", "/devices/kiosk-login", json={"user_id": 1, "pin": "0000"})
assert bad.status_code == 401 and kb.api_error(bad) == "Wrong PIN."
tok = kb.api("POST", "/devices/kiosk-login", json={"user_id": 1, "pin": "1234"}).json()["token"]
assert call("GET", "/overview").status_code == 401  # no session, no operator routes

manual = {"source_area": "Purok 4", "category": "Recyclable", "weight_kg": 2.5, "weighed_by": "manual",
          "client_key": "a1"}
assert kb.post_delivery(manual)["code"] == "D-0004"
assert kb.post_delivery(manual)["code"] == "D-0004"  # a retry is stored once
recent = kb.api("GET", "/devices/kiosk-recent").json()
assert recent[0]["code"] == "D-0004" and recent[0]["kg"] == "2.5 kg", recent

# load cell: SapulMRF's rule for a scale weight
scale = {**manual, "weighed_by": "scale", "weight_kg": 3.1, "client_key": "s1"}
r = kb.api("POST", "/devices/deliveries", json=scale)
assert r.status_code == 422 and "no recent reading" in kb.api_error(r)
kb.api("POST", "/devices/readings", json={"kg": 3.1, "stable": False})
assert "hasn't settled" in kb.api_error(kb.api("POST", "/devices/deliveries", json=scale))
kb.api("POST", "/devices/readings", json={"kg": 3.0, "stable": True})
assert kb.api("POST", "/devices/deliveries", json=scale).status_code == 422  # not the scale's weight
kb.api("POST", "/devices/readings", json={"kg": 3.1, "stable": True})
assert kb.post_delivery(scale)["code"] == "D-0005"

# every operator task, through the bridge's own request and reply shapes
code, today = op("today", tok)
assert code == 200 and today["drops"] == 5 and today["segregate"] == 4 and today["waiting"] == 20.0, today
assert len(op("materials", tok)[1]) == 4 and op("areas", tok)[1][0] == {"id": 351, "name": "Demo Area A"}
waiting = op("waiting", tok, **{"for": "segregation"})[1]
d1 = waiting[0]
assert d1["code"] == "D-0001" and d1["dry_kg"] == 0, waiting
assert op("segregate", tok, delivery_id=d1["id"], wet_kg=4.5, dry_kg=2.0)[0] == 200
assert op("segregate", tok, delivery_id=d1["id"], wet_kg=4.5, dry_kg=2.0)[0] == 409  # already done
sorting = op("waiting", tok, **{"for": "sorting"})[1]
assert {x["code"]: x["dry_kg"] for x in sorting} == {"D-0001": 2.0, "D-0003": 3.2}, sorting
assert op("sort", tok, delivery_id=d1["id"], material_id=1, area_id=351, kg=2.0)[0] == 200

compost = op("compost", tok)[1]
assert compost["waiting"] == 24.5 and compost["batches"][0]["code"] == "C-0001", compost
assert op("compost_start", tok, kg=30)[0] == 422  # more than is waiting
assert op("compost_start", tok, kg=10)[0] == 201
assert op("compost_log", tok, batch_id=1, activity="turning")[0] == 200
assert op("compost_finish", tok, batch_id=1, kg=12)[0] == 200
assert op("compost_on_hand", tok)[1] == {"kg": 12.0}

stock = op("stock", tok, kind="sale")[1]
plastic = next(x for x in stock if x["material"] == "Plastic")
assert plastic == {"material_id": 1, "material": "Plastic", "area_id": 351, "area": "Demo Area A", "kg": 42.0,
                   "price": 12.0}, plastic
assert op("stock", tok, kind="disposal")[1][0]["material_id"] is None
assert op("buyers", tok)[1] == ["Junk shop Sapul"]
sale = dict(kind="sale", material_id=1, area_id=351, kg=2, recipient="Walk-in buyer", price=15, key="k1")
code, o = op("outgoing", tok, **sale)
assert code == 201 and o == {"code": "O-0001", "kg": 2.0, "recipient": "Walk-in buyer", "total": 30.0}, o
assert op("outgoing", tok, **sale)[1]["code"] == "O-0001"  # a resend can't sell twice
give = dict(kind="giveaway", material_id=1, area_id=351, kg=1, recipient="Walk-in buyer", key="k2")
code, o = op("outgoing", tok, **give)
assert code == 201 and o["total"] is None, o
code, err = op("outgoing", tok, **{**give, "kg": 500, "key": "k3"})
assert code == 409 and "Only 39.0 kg" in err, err
r = call("POST", "/outgoing", {"type": "giveaway", "material_id": 1, "storage_area_id": 351, "kg": 1,
                               "recipient": "x", "price_per_kg": 5, "client_key": "k4"}, tok)
assert r.status_code == 422  # a give-away never has a price
assert op("outgoing", tok, kind="compost", kg=5, recipient="Walk-in buyer", key="k5")[0] == 201
assert op("compost_on_hand", tok)[1] == {"kg": 7.0}

fix = op("fixable", tok)[1]
assert {x["code"]: x["stage"] for x in fix}["D-0001"] == "sorted", fix
assert op("undo", tok, delivery_id=d1["id"], to="sort", reason="Wrong material")[1] == {"code": "D-0001", "to": "sort"}
assert op("stock", tok, kind="sale")[1][0]["kg"] == 37.0  # the sort's 2 kg left Demo Area A
assert op("undo", tok, delivery_id=d1["id"], to="delivery", reason="Typed twice")[0] == 200
assert "D-0001" not in [x["code"] for x in op("fixable", tok)[1]]

area = op("area_stock", tok, area_id=351)[1]
assert area["area_id"] == 351 and len(area["rows"]) == 2, area
assert op("recount", tok, material_id=2, area_id=351, kg=20, reason="Counted")[0] == 200
assert next(x for x in op("area_stock", tok, area_id=351)[1]["rows"] if x["material_id"] == 2)["kg"] == 20.0

b_rows = op("area_stock", tok, area_id=352)[1]["rows"]  # Demo Area B holds only Metal


class Stub:  # just what the bridge's own method needs
    token = tok
    as_operator = kb.Bridge.as_operator


kb.Bridge.add_empty_materials(Stub(), b_rows)  # the rest join at 0 kg, so a recount can count them up
assert [(r["material"], r["kg"]) for r in b_rows] == [("Metal", 10.0), ("Plastic", 0.0), ("Paper", 0.0),
                                                      ("Glass", 0.0)], b_rows
assert call("GET", "/classifications/pending", token=tok).json() == []
assert call("POST", "/classifications/1/confirm", {}, tok).ok
photo = call("GET", "/deliveries/3/photo", token=tok)
assert photo.ok and photo.content[:2] == b"\xff\xd8" and kb.fit_jpeg(photo.content, kb.PHOTO_W, kb.PHOTO_H)
assert call("GET", "/nothing", token=tok).status_code == 404
assert call("POST", "/auth/logout", token=tok).ok and call("GET", "/overview", token=tok).status_code == 401
print("check_fake_api: OK")
