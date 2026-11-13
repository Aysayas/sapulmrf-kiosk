# SapulMRF Kiosk

7" ESP32-S3 touch kiosk (800×480) for Barangay Sapul MRF. The PC runs YOLO on the USB camera.

```
USB cam ─► PC: kiosk_bridge.py (YOLO, draws boxes, saves intake)
               │  connects to the kiosk: TCP 5000 (JPEG + ACK, like the SapulMRF stream)
               ▼
        ESP32-S3 7" kiosk (LVGL) = the server, shows its IP
        Welcome → Camera scan → Weigh & confirm → Receipt
        🔒 → Operator PIN → Operator station
        🔋 INA219 battery % on every screen
```

## How they find each other
At power-on the kiosk tries, in order:
1. **Your Wi-Fi** (`WIFI1_SSID`)
2. **A phone hotspot** (`WIFI2_SSID`)
3. If neither is found, it makes **its own hotspot** `SapulMRF-Kiosk` (password `sapulmrf123`) at **192.168.4.1**.

The welcome screen shows which network it's on, its IP, and whether the camera PC is connected.
Put the PC on that same network (for option 3, join `SapulMRF-Kiosk` from the PC's Wi-Fi list).
The kiosk broadcasts a beacon every 2 s, so the PC script finds it without typing the IP.

## 1. PC bridge
```
cd pc_bridge
pip install ultralytics opencv-python requests websockets
python kiosk_bridge.py --model path/to/best.pt --cam 1
```
- **NVIDIA GPU (recommended):** plain `pip install` gives a CPU-only torch. For YOLO on the GPU, install the CUDA build once:
  `pip install --force-reinstall --no-deps torch torchvision --index-url https://download.pytorch.org/whl/cu130`
  At start-up the bridge prints the camera mode and `YOLO on GPU 0` or `YOLO on CPU`.
- **How video flows:** the camera is captured once at its best MJPG mode (up to 1920×1080). YOLO runs on its own thread. The kiosk gets a light 480×360 JPEG at 15 fps (`SEND_FPS`, `KIOSK_JPEG_QUALITY`). The drop-off photo sent to SapulMRF is the clean full-resolution frame at JPEG quality 90.
- If auto-find doesn't work (some hotspots block broadcasts), add the IP from the kiosk screen:
  `python kiosk_bridge.py --model best.pt --cam 1 192.168.1.77`
- If the PC is on the kiosk's own hotspot and the beacon isn't heard, the script tries 192.168.4.1 automatically.
- It reconnects by itself if the kiosk restarts or Wi-Fi drops.
- Edit `RECYCLABLE` / `BIODEGRADABLE` / `RESIDUAL` near the top so they match your model's class names. A class in none of them is **ignored**, so the room behind the tray (chairs, a TV) never decides the category. At start-up the bridge lists the ignored classes.
- **SapulMRF link.** Register the kiosk once in SapulMRF (Settings › IoT devices) as a **weighing station**, e.g. `KIOSK-01`. Copy its key (shown once), then before starting the bridge:
  ```
  $env:SAPULMRF_API = "http://127.0.0.1:8001"     # SapulMRF API (this address if it runs on the same laptop)
  $env:SAPULMRF_KEY = "mrfk_..."                   # the KIOSK-01 key; never commit it
  ```
  - Each drop-off goes to `POST /devices/deliveries` with the camera's category, counts, confidence and a snapshot photo. The receipt shows SapulMRF's `D-` code.
  - Every 5 s the bridge polls `GET /devices/kiosk-state` and passes today's totals, open alerts and the status light to the kiosk.
  - **SapulMRF down:** the drop-off is saved in `outbox/`, the receipt shows a `K-` code, and the bridge retries every 15 s until SapulMRF accepts it, exactly once. Drop-offs SapulMRF refuses for good (e.g. an unknown purok name) are moved to `outbox/rejected/` for the operator.
  - Without `SAPULMRF_API` the bridge runs offline and only writes `kiosk_intake.csv`. That file is always written either way, as a local log.
  - **Operator mode:** each MRF operator sets a kiosk PIN on the SapulMRF website (Profile › Kiosk PIN). On the kiosk: **Staff**, tap your name, type your PIN.
    - **Home:** today's drop-offs and kg, how full each storage area is, and two tasks with what's waiting: **Segregation** and **Sort into storage**.
    - **Sorting answers the camera too:** the material step shows the drop-off's photo with the camera's guess highlighted ("Plastic · camera 86%"). Kg starts at the dry weight from the segregation check, and the area whose name mentions the material is preselected. Saving also confirms or corrects the camera's suggestion in SapulMRF.
    - **Segregation:** type the wet kg; dry fills in as total − wet until you type it yourself.
    - **How saving works:** the bridge holds that operator's SapulMRF session (memory only, 30 minutes at most) and makes each save through SapulMRF's own operator routes. The kiosk shows SapulMRF's answer, e.g. a full storage area.
    - **Signing out:** Lock, or 5 minutes idle, signs out. 5 wrong PINs lock that operator for 5 minutes. Without SapulMRF there is no operator mode.
- Windows Firewall: allow Python on **private** networks (it must receive UDP 5001 for auto-find).

## 2. Kiosk firmware (Arduino IDE)
1. Board: **ESP32S3 Dev Module**, PSRAM **OPI PSRAM**, Flash **16MB QIO**, Partition **Huge APP**.
2. Install the libraries listed at the top of `SapulMRF_Kiosk.ino`. Use **lvgl 8.3.x**, not 9. The WebSockets library is no longer needed.
3. Copy `SapulMRF_Kiosk/lv_conf.h` into your Arduino `libraries` folder, next to `lvgl/`.
4. In `config.h`, set `WIFI1_*` (router) and `WIFI2_*` (phone hotspot). Leave either as `""` to skip it.
5. Upload.

## 3. Battery indicator (INA219, same wiring as the Alangan translator)
| INA219 | Connect to |
|---|---|
| VCC | 3.3V |
| GND | GND, shared with battery − and the board |
| SDA / SCL | GPIO 19 / GPIO 20 (P3 connector, shared with touch) |
| VIN+ | battery + (after the switch) |
| VIN− | charger/boost module B+ |

- The icon and % sit top-right on every screen: green ⚡ while charging, red below 15%.
- If no INA219 is found, or no battery reaches VIN+, the icon stays hidden.
- The serial monitor prints `Battery: INA219 found (0x40)` at boot, then `Battery: 3912 mV, 480 mA -> 72%` every 10 s.
- Settings in `config.h`: `INA219_ADDR`, `INA219_SHUNT_OHM`, `BATTERY_INTERNAL_MOHM` (raise it if % drops when the video starts), `BATTERY_USE_INA219 0` to turn it off.
- If ⚡ shows while unplugged, VIN+ and VIN− are swapped.

## Troubleshooting
| Symptom | Fix |
|---|---|
| Kiosk always ends up on its own hotspot | Check SSID/password spelling; the ESP32 only sees **2.4 GHz** Wi-Fi (set the phone hotspot to 2.4 GHz / "maximize compatibility") |
| PC says "Looking for the kiosk" forever | Pass the IP shown on the kiosk; check the PC is on the same network and the firewall allows Python |
| Screen flickers / shifts while video streams | Lower `RGB_PCLK_HZ` to 12000000, or lower `JPEG_QUALITY` / `SEND_FPS` on the PC |
| Taps land in the wrong place | Change `tp.setRotation(ROTATION_INVERTED)` to `ROTATION_NORMAL` |
| Touch stops working after adding the INA219 | Remove the pull-up resistors from the INA219 module (the touch bus already has them) |
| "Next: weigh" never enables | The category must stay the same for `STABLE_FRAMES` frames. Lower it, or lower `CONF` |

## Optional load-cell scale
Set `USE_HX711 1` in `config.h`, wire the HX711 to two free GPIOs (defaults 17/18; check your board's headers), and calibrate `HX711_SCALE` with a known weight. Without it, residents type the weight on the keypad.
