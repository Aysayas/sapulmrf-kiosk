"""Bridge compost actions: the kiosk's taps map to SapulMRF's own compost routes, and replies are cut to fit."""
import sys

sys.path.insert(0, r"C:\dev\sapulmrf-kiosk\sapulmrf-kiosk\pc_bridge")
import kiosk_bridge as kb

method, path, body, shape = kb.op_request("compost", {})
assert (method, path, body) == ("GET", "/operations/compost", None)
view = {"wet_waiting_kg": 12.5, "batches": [
    {"id": 3, "code": "B-03", "day": 9, "start_kg": 18.0, "finished_at": None, "activities": []},
    {"id": 2, "code": "B-02", "day": 30, "start_kg": 20.0, "finished_at": "2026-11-01T00:00:00Z", "activities": []}]}
assert shape(view) == {"waiting": 12.5, "batches": [{"id": 3, "code": "B-03", "day": 9, "start_kg": 18.0}]}, shape(view)

assert kb.op_request("compost_start", {"kg": "7.5"})[:3] == ("POST", "/compost/batches", {"start_kg": 7.5, "sensor_id": None})
assert kb.op_request("compost_log", {"type": "op", "batch_id": 3, "activity": "watering"})[:3] == (
    "POST", "/compost/batches/3/activities", {"type": "watering"})
assert kb.op_request("compost_finish", {"batch_id": 3, "kg": 9})[:3] == (
    "POST", "/compost/batches/3/activities", {"type": "inspection", "finished_kg": 9.0})
try:
    kb.op_request("compost_log", {"type": "op", "batch_id": 3, "activity": "burning"})
    raise AssertionError("an unknown activity must be refused")
except ValueError:
    pass

# operator Home: the wet waiting kg rides along with today's numbers
o = {"today_deliveries": 4, "today_kg": 20.0, "awaiting_segregation": 1, "areas": [], "wet_waiting_kg": 6.25}
assert kb.op_request("today", {})[3](o)["waiting"] == 6.25
print("check_compost: all passed")
