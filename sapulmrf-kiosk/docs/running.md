# Running the kiosk: with the real SapulMRF or the fake API

The kiosk never talks to SapulMRF itself. The **camera bridge** (`pc_bridge/kiosk_bridge.py`) runs on the laptop, talks to the kiosk over Wi-Fi, and sends everything to whatever API `SAPULMRF_API` points at:

| Mode | `SAPULMRF_API` points at | Use it for |
|---|---|---|
| **Real** | SapulMRF in Docker (`C:\dev\sapulmrf`) | Real use, demos, anything that should show on the website |
| **Fake** | `pc_bridge/fake_api.py` | Testing the kiosk or the load cell without Docker or the website. Nothing is saved |
| **Offline** | nothing (not set) | Drop-offs only, saved to `kiosk_intake.csv`; no operator mode |

Both real and fake use port **8001**, so run only one of them at a time.

## Before either: the kiosk
1. Turn the kiosk on. It joins the Wi-Fi in `SapulMRF_Kiosk/config.h`. The laptop must be on the **same 2.4 GHz network**.
2. The welcome screen shows the network and the kiosk's IP. The IP can change after a restart; the bridge finds it by its UDP 5001 beacon, so **don't type the IP** unless it can't find it.

## A. With the real SapulMRF
1. Start **Docker Desktop** and wait for "Engine running".
2. Start the website and API (PowerShell):
   ```
   cd C:\dev\sapulmrf
   docker compose up -d --wait web
   ```
   Check http://127.0.0.1:8001/health shows `"status":"ok"`. The website is http://127.0.0.1:5174.
3. **Once only:** register the kiosk in the website (Settings › IoT devices, a **weighing station**, e.g. `KIOSK-01`). Copy its key (shown once) and save it for your Windows user:
   ```
   setx SAPULMRF_KEY "mrfk_..."
   ```
   Open a new PowerShell window afterwards so it's picked up. Each operator sets a kiosk PIN on the website (Profile › Kiosk PIN).
4. Start the bridge in its own window and leave it open:
   ```
   cd C:\dev\sapulmrf-kiosk\sapulmrf-kiosk\pc_bridge
   $env:SAPULMRF_API = "http://127.0.0.1:8001"
   py kiosk_bridge.py --model best.pt --cam 0
   ```
   It prints `SapulMRF API: http://127.0.0.1:8001 (key set)` and `Connected to kiosk …`.
5. Use the kiosk. Drop-offs show a `D-` code and appear in the website's Intake. Operators sign in with Staff → name → PIN.

More detail and troubleshooting: `C:\dev\sapulmrf\docs\start-up.md`.

## B. With the fake API (no Docker, no website)
1. If SapulMRF is running, free port 8001 first:
   ```
   cd C:\dev\sapulmrf
   docker compose stop api
   ```
   Or leave it running and give the fake another port (step 2 with `--port 8011`, step 3 with `:8011`).
2. Window 1, the fake API:
   ```
   cd C:\dev\sapulmrf-kiosk\sapulmrf-kiosk\pc_bridge
   py fake_api.py
   ```
3. Window 2, the bridge pointed at it:
   ```
   cd C:\dev\sapulmrf-kiosk\sapulmrf-kiosk\pc_bridge
   $env:SAPULMRF_API = "http://127.0.0.1:8001"; $env:SAPULMRF_KEY = "fake"
   py kiosk_bridge.py --model best.pt --cam 0 --dev
   ```
4. Use the kiosk:
   - Operators: **Operator 1, PIN 1234** or **Operator 2, PIN 5678**.
   - Demo data: Plastic / Paper / Metal / Glass in Demo Area A, Demo Area B and a Residual bin; two drop-offs waiting for segregation and one waiting to be sorted; one open compost batch with 20 kg of wet waste waiting.
   - Every task works: drop-off, segregate, sort, compost, sell / give free, fix, recount. The same rules as SapulMRF apply (no selling more than the stock, no price on a give-away, a resend isn't counted twice).
   - Window 1 prints each save and each refusal with its reason.
5. Stop with Ctrl+C in both windows. Everything entered is forgotten. Start SapulMRF's API again with `docker compose start api`.

### Testing the load cell (fake API)
- Set `USE_HX711 1` and calibrate `HX711_SCALE` in `config.h` (weigh something known, adjust until the screen agrees), then build and upload.
- **How the kiosk uses it:**
  - It reads the scale every 0.3 s.
  - The weight counts as **steady** once 5 readings in a row sit within ±0.05 kg.
  - Each new steady weight is sent to the PC as `{"type":"reading","kg":2.35,"stable":true}`, and sent again every 5 minutes.
  - When the weight moves, it sends `"stable":false`.
- **Confirm:** the screen shows the steady value. Confirm says "Wait for the scale..." until it settles, and the drop-off uses exactly the value that was sent.
- **The rule:** SapulMRF, and the fake, accept a scale drop-off only with a steady reading of the same weight from the last 10 minutes.
- **Watching it:** window 1 prints every reading as it arrives (`scale 2.35 kg steady` / `settling`).

### What the fake doesn't do
- No website: nothing appears in SapulMRF, and nothing is kept after it stops.
- No Live camera page (the bridge retries that connection quietly) and no camera suggestions on the sort screen.
- If window 1 prints `fake API has no GET /…`, the bridge called a route the fake lacks: add it to `fake_api.py`.

## Testing and screenshots (either mode)
- Start the bridge with `--dev`. Then, from this laptop only:

  | URL | Does |
  |---|---|
  | `http://127.0.0.1:8091/signin?user_id=1&pin=1234` | Signs an operator in, like the PIN pad |
  | `/goto?screen=sellform` | Shows a screen (`en` / `tl` switch the language); needs `DEV_TOOLS 1` |
  | `/shot?label=name` | Saves a screenshot to `pc_bridge/kiosk_shots/` |
  | `/op?json={...}` | Runs an operator task as if the kiosk sent it |
  | `/goto?screen=dropoff` | A test resident drop-off through the real Confirm (Purok 3, Recyclable, 1.5 kg) |

  Use the user id and PIN that match the mode: the fake's Operator 1 is id 1, while the real one is whatever SapulMRF gave them.
- Self-checks (no kiosk needed): `py tests\check_fake_api.py`, and the other `tests\check_*.py`.

## The kiosk key (PC_KEY)
`PC_KEY` in `SapulMRF_Kiosk/config.h` stops other devices on the Wi-Fi from taking over the kiosk.
- **The check:** a PC must first send `{"type":"hello","key":"<PC_KEY>"}`, within 3 seconds. Otherwise the kiosk drops it and sends it nothing.
- **Holding the link:** while the trusted PC is talking (anything in the last 10 s), a new connection is refused instead of replacing it.
- **Where the bridge gets the key:** from `KIOSK_PC_KEY` if set, else straight from `config.h` in this repo, so on this laptop there's nothing to set.
- **On another PC:** set `$env:KIOSK_PC_KEY = "<the key>"` before starting the bridge.
- `""` turns the check off (the committed `config.h` has `""`; the real key stays local, like the Wi-Fi password).

## Before deployment
- `DEV_TOOLS 0` in `config.h`, so the PC can't switch screens. The dev control's `/goto` and test drop-off stop working.
- A real `PC_KEY`, and a calibrated scale if one is fitted.

## When it doesn't work
| What you see | Fix |
|---|---|
| `py fake_api.py` fails with `WinError 10013` (or "address already in use") | SapulMRF's API is on 8001: `docker compose stop api`, or use `--port 8011` |
| Bridge says `SAPULMRF_KEY missing!` | Real mode: `setx SAPULMRF_KEY …` and open a new window. Fake mode: set it to `fake` |
| Kiosk receipts show `K-` codes | The API wasn't reachable. The bridge keeps them in `outbox/` and sends them when it is back. Against the fake, they're sent to the fake |
| Operator PIN says "no answer" | The bridge isn't running or can't reach the API |
| Bridge keeps saying "Can't reach kiosk at 192.168.x.x" | It was started with an old IP. Ctrl+C, start it again **without** the IP |
| Bridge says "Looking for the kiosk" forever | Different networks, or Windows Firewall blocks Python (allow it on private networks) |
| The bridge window closes by itself | The laptop ran low on memory. Close heavy apps and start it again |
| Bridge connects, then the kiosk drops it at once | The PC_KEY doesn't match: check `config.h`, or `KIOSK_PC_KEY` on another PC |
| The kiosk keeps dropping the link every few seconds | Two bridges are running. `Get-Process py, python`, and stop the older one |
