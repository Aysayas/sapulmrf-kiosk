# Training the waste model

`best.pt` is the stock YOLO model. It only knows everyday objects (bottle, cup, banana…), so it can't see sachets, cans, cardboard or styro. A model trained on the MRF's own waste fixes that. Everything here runs on this laptop (RTX 4050), for free.

## 1. Collect photos (automatic)
Every drop-off the kiosk records saves:
- its photo to `dataset/raw/images/<code>.jpg`
- the model's boxes, as YOLO **pre-labels**, to `dataset/raw/labels/<code>.txt`

The class list is in `dataset/raw/classes.txt` (the same as `tools/data.yaml`). Pre-labels only cover what the current model can see, so expect to add most boxes yourself at first.

More photos also come from SapulMRF. In `C:\dev\sapulmrf`, run:

```
docker compose exec api python -m app.cli export-photos --out exports/photos
```

It writes each drop-off photo plus `labels.csv`, which holds the confirmed category and the materials operators confirmed. Use it to know what is in each photo.

**Aim for:**
- **Quantity:** 50+ photos per class to start; 200+ for good results.
- **Variety:** different lighting, piles, and items touching each other.
- **Negatives:** include some photos of an empty tray. A photo with no label file teaches "nothing here".

## 2. Correct the labels (your part)
Use any labelling tool that reads and writes the YOLO format. Free options:
- **Label Studio:** `pip install label-studio`, then `label-studio`.
- **CVAT.**

1. Import `dataset/raw` with `classes.txt`.
2. Fix the pre-labelled boxes, add the missing ones, and delete wrong ones.
3. Export in YOLO format to `dataset/labelled/images` and `dataset/labelled/labels`.

**Draw one box per item,** tight around it. When unsure of an item's class, leave it out: a missing box is less harmful than a wrong one.

**Optional:** merge a public waste dataset (e.g. TACO, or Roboflow Universe "trash" sets). First rename its classes to the 11 in `tools/data.yaml`.

## 3. Train
```
cd pc_bridge
py tools/train_waste.py --epochs 1     # smoke test: the dataset and GPU work
py tools/train_waste.py                # the real run (stops early when it stops improving)
```
The script:
1. Splits `dataset/labelled` into 80% train and 20% check photos, the same split every time.
2. Fine-tunes `yolo11n.pt`.
3. Saves the result as `pc_bridge/waste.pt`.

If the GPU runs out of memory, add `--batch 8`.

**Read the result** in `runs/waste/`:
- **mAP50:** above 0.6 is usable and above 0.8 is good.
- **The per-class table:** a weak class means it needs more photos.
- **`confusion_matrix.png`:** shows which classes get mixed up.

## 4. Use it
```
py kiosk_bridge.py --model waste.pt --cam 0 <kiosk IP>
```
The bridge already maps these class names to Recyclable, Biodegradable and Residual, and to SapulMRF's materials.

The model's name and hash are sent with each drop-off, so on the website (Insights › Camera accuracy) you can see how the new model does against the old one.

**Tuning thresholds:** if one class gives false alarms, raise just that class with `CLASS_CONF` in `kiosk_bridge.py`, for example `{"sachet": 0.6}`.
