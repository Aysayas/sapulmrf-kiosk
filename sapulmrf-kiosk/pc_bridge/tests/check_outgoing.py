"""Bridge selling actions: stock, compost on hand, recent buyers, and recording outgoing with a resend-safe key."""
import sys

sys.path.insert(0, r"C:\dev\sapulmrf-kiosk\sapulmrf-kiosk\pc_bridge")
import kiosk_bridge as kb

method, path, body, shape = kb.op_request("stock", {"kind": "sale"})
assert (method, path, body) == ("GET", "/operations/inventory", None)
inv = {"areas": [], "stock": [
    {"material": "Paper", "area": "Area B", "kg": 3.0, "bags": 1, "material_id": 2, "storage_area_id": 6, "last_change": ""},
    {"material": "Plastic", "area": "Area A", "kg": 12.5, "bags": 4, "material_id": 1, "storage_area_id": 5, "last_change": "",
     "last_price_per_kg": 12.0},
    {"material": "Residual", "area": "Area D", "kg": 0.0, "bags": 0, "material_id": None, "storage_area_id": 8, "last_change": ""}]}
assert shape(inv) == [{"material_id": 1, "material": "Plastic", "area_id": 5, "area": "Area A", "kg": 12.5, "price": 12.0},
                      {"material_id": 2, "material": "Paper", "area_id": 6, "area": "Area B", "kg": 3.0, "price": None}], shape(inv)
# residual is listed for disposal only, and is never pushed off the list by bigger recyclable rows
inv["stock"] += [{"material": "Plastic", "area": f"Area {i}", "kg": 100.0 + i, "bags": 1, "material_id": 1,
                  "storage_area_id": 20 + i, "last_change": ""} for i in range(10)]
inv["stock"].append({"material": "Residual", "area": "Area D", "kg": 0.5, "bags": 1, "material_id": None,
                     "storage_area_id": 8, "last_change": ""})
assert kb.op_request("stock", {"kind": "disposal"})[3](inv) == [
    {"material_id": None, "material": "Residual", "area_id": 8, "area": "Area D", "kg": 0.5, "price": None}]
assert all(r["material_id"] for r in kb.op_request("stock", {"kind": "sale"})[3](inv))

method, path, body, shape = kb.op_request("compost_on_hand", {})
assert (method, path) == ("GET", "/operations/outgoing") and shape({"compost_on_hand_kg": 7.5}) == {"kg": 7.5}

method, path, body, shape = kb.op_request("buyers", {})
assert (method, path) == ("GET", "/masterdata/buyers?active=true")
assert shape([{"id": 1, "name": "Junkshop Reyes", "active": True}, {"id": 2, "name": "ENRO", "active": True}]) == ["Junkshop Reyes", "ENRO"]
try:
    kb.op_request("outgoing", {"kind": "gift", "kg": 1, "recipient": "x", "key": "k"})
    raise AssertionError("an unknown kind must be refused")
except ValueError:
    pass

method, path, body, shape = kb.op_request("outgoing", {"type": "op", "kind": "sale", "material_id": 1, "area_id": 5,
                                                       "kg": "2", "recipient": " Junkshop Reyes ", "price": 12,
                                                       "key": "k-1234-3"})
assert (method, path) == ("POST", "/outgoing")
assert body == {"type": "sale", "material_id": 1, "storage_area_id": 5, "kg": 2.0, "recipient": "Junkshop Reyes",
                "price_per_kg": 12.0, "client_key": "k-1234-3"}, body
assert shape({"code": "O-0123", "kg": 2.0, "recipient": "Junkshop Reyes", "total": 24.0, "status": "approved"}) == \
    {"code": "O-0123", "kg": 2.0, "recipient": "Junkshop Reyes", "total": 24.0}
compost = kb.op_request("outgoing", {"kind": "compost", "kg": 5, "recipient": "Purok 2", "key": "k-9"})[2]
assert compost == {"type": "compost", "material_id": None, "storage_area_id": None, "kg": 5.0, "recipient": "Purok 2",
                   "price_per_kg": None, "client_key": "k-9"}, compost
disposal = kb.op_request("outgoing", {"kind": "disposal", "area_id": 8, "kg": 4, "recipient": "ENRO", "key": "k-8"})[2]
assert disposal["material_id"] is None and disposal["storage_area_id"] == 8 and disposal["price_per_kg"] is None
print("check_outgoing: all passed")

# give away free: material and area, never a price; compost may carry a price when sold
give = kb.op_request("outgoing", {"kind": "giveaway", "material_id": 1, "area_id": 5, "kg": 2, "recipient": "School",
                                  "price": 9, "key": "k-g"})[2]
assert give == {"type": "giveaway", "material_id": 1, "storage_area_id": 5, "kg": 2.0, "recipient": "School",
                "price_per_kg": None, "client_key": "k-g"}, give
sold = kb.op_request("outgoing", {"kind": "compost", "kg": 5, "recipient": "Farm", "price": 4, "key": "k-c"})[2]
assert sold["price_per_kg"] == 4.0 and sold["storage_area_id"] is None, sold
assert kb.op_request("stock", {"kind": "giveaway"})[3](inv)[0]["material"] == "Plastic"  # recyclables, like a sale
print("check_outgoing (give away): all passed")
