"""Fine-tune YOLO on the labelled waste photos and put the result next to the bridge as waste.pt.

  py tools/train_waste.py                 # 100 epochs on GPU 0 (stops early when it stops improving)
  py tools/train_waste.py --epochs 1      # smoke test: proves the dataset and the GPU work
Then run the bridge with --model waste.pt.
"""
import argparse
import random
import shutil
from pathlib import Path

import yaml
from ultralytics import YOLO

HERE = Path(__file__).resolve().parent
BRIDGE = HERE.parent
DATASET = BRIDGE / "dataset"


def split(val_share=0.2, seed=0):
    """dataset/labelled/{images,labels} -> dataset/train and dataset/val (same split every run)."""
    src = DATASET / "labelled"
    images = sorted(p for p in (src / "images").glob("*") if p.suffix.lower() in (".jpg", ".jpeg", ".png"))
    if len(images) < 2:
        raise SystemExit(f"Need labelled photos in {src}/images and {src}/labels (see TRAINING.md).")
    random.Random(seed).shuffle(images)
    n_val = max(1, int(len(images) * val_share))
    for part, items in (("val", images[:n_val]), ("train", images[n_val:])):
        for sub in ("images", "labels"):
            shutil.rmtree(DATASET / part / sub, ignore_errors=True)
            (DATASET / part / sub).mkdir(parents=True)
        for img in items:
            shutil.copy2(img, DATASET / part / "images" / img.name)
            label = src / "labels" / f"{img.stem}.txt"
            if label.exists():   # no label file = a photo with no waste in it (a useful negative)
                shutil.copy2(label, DATASET / part / "labels" / label.name)
    return len(images) - n_val, n_val


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--epochs", type=int, default=100)
    ap.add_argument("--base", default="yolo11n.pt", help="starting weights (downloaded on first use)")
    ap.add_argument("--imgsz", type=int, default=640)
    ap.add_argument("--batch", type=int, default=16, help="lower it if the GPU runs out of memory")
    args = ap.parse_args()
    n_train, n_val = split()
    print(f"Training on {n_train} photos, checking on {n_val}.")
    data = yaml.safe_load((HERE / "data.yaml").read_text(encoding="utf-8"))
    data["path"] = str(DATASET)   # ultralytics reads a relative path from its own datasets folder, not this one
    (BRIDGE / "runs").mkdir(exist_ok=True)
    resolved = BRIDGE / "runs" / "data.yaml"
    resolved.write_text(yaml.safe_dump(data, sort_keys=False), encoding="utf-8")
    model = YOLO(args.base)
    model.train(data=str(resolved), epochs=args.epochs, imgsz=args.imgsz, batch=args.batch, device=0,
                patience=20, project=str(BRIDGE / "runs"), name="waste", exist_ok=True)
    best = BRIDGE / "runs" / "waste" / "weights" / "best.pt"
    shutil.copy2(best, BRIDGE / "waste.pt")
    print(f"Saved {BRIDGE / 'waste.pt'}. mAP50 and per-class results are in {best.parent.parent}.")


if __name__ == "__main__":
    main()
