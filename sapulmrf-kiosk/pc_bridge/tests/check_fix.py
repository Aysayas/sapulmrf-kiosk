"""Bridge fix and recount actions: today's records, undo with a reason, an area's stock, and a recount."""
import sys

sys.path.insert(0, r"C:\dev\sapulmrf-kiosk\sapulmrf-kiosk\pc_bridge")
import kiosk_bridge as kb

method, path, body, shape = kb.op_request("fixable", {})
assert (method, path, body) == ("GET", "/corrections/fixable", None)
rows = [{"id": 7, "code": "D-4502", "source_area": "Purok 3", "category": "Mixed household", "weight_kg": 2.4, "stage": "sorted"}]
assert shape(rows) == [{"id": 7, "code": "D-4502", "label": "D-4502  ·  Purok 3  ·  2.4 kg", "stage": "sorted"}], shape(rows)

method, path, body, shape = kb.op_request("undo", {"type": "op", "delivery_id": "7", "to": "sort", "reason": " Wrong sort "})
assert (method, path) == ("POST", "/corrections/undo")
assert body == {"delivery_id": 7, "to": "sort", "reason": "Wrong sort"}, body
assert shape({"id": 1, "kind": "sort", "record_code": "D-4502"}) == {"code": "D-4502", "to": "sort"}
try:
    kb.op_request("undo", {"delivery_id": 7, "to": "everything", "reason": "x"})
    raise AssertionError("an unknown step must be refused")
except ValueError:
    pass

method, path, body, shape = kb.op_request("area_stock", {"area_id": 5})
assert (method, path) == ("GET", "/operations/inventory")
inv = {"areas": [], "stock": [
    {"material": "Plastic", "area": "Area A", "kg": 12.5, "material_id": 1, "storage_area_id": 5},
    {"material": "Residual", "area": "Area A", "kg": 1.0, "material_id": None, "storage_area_id": 5},
    {"material": "Paper", "area": "Area B", "kg": 3.0, "material_id": 2, "storage_area_id": 6}]}
assert shape(inv) == {"area_id": 5, "rows": [{"material_id": 1, "material": "Plastic", "kg": 12.5},
                                             {"material_id": None, "material": "Residual", "kg": 1.0}]}, shape(inv)
# the reply says which area it is for: a late reply for another area is dropped by the kiosk

method, path, body, shape = kb.op_request("recount", {"material_id": 1, "area_id": 5, "kg": "11", "reason": "Spilled"})
assert (method, path) == ("POST", "/inventory/adjustments")
assert body == {"material_id": 1, "storage_area_id": 5, "actual_kg": 11.0, "reason": "Spilled"}, body
assert kb.op_request("recount", {"area_id": 5, "kg": 0, "reason": "Spilled"})[2]["material_id"] is None  # residual

# a drop-off recorded again in place of an undone one carries `replaces`
assert kb.replaces_body({"replaces": "D-4502"}) == {"replaces": "D-4502"}
assert kb.replaces_body({}) == {}
print("check_fix: all passed")
