#!/usr/bin/env python3
"""Check manifest coverage, Rack widget placement and panel geometry."""
from collections import Counter
from itertools import combinations
import json
import re
import sys
import xml.etree.ElementTree as ET

import yaml

from panel import HP_MM, PANEL_H, ROOT

# Bounding rectangles in mm, rounded up from the stock Rack widget SVGs.
FOOTPRINTS = {
    "Davies1900hBlackKnob": (12.4, 12.4),
    "Davies1900hWhiteKnob": (12.4, 12.4),
    "CKSS": (5.6, 8.2),
    "RockerSwitch": (12, 6.4),
    "TimingSwitch": (12, 6.4),
    "VCVButton": (7, 7),
    "PJ301MPort": (8.4, 8.4),
    "MediumLight<GreenLight>": (3.6, 3.6),
    "MediumLight<RedLight>": (3.6, 3.6),
}
KINDS = {
    "params": ("Param", r"create(?:ParamCentered|SnapKnob)"),
    "inputs": ("Input", "createInputCentered"),
    "outputs": ("Output", "createOutputCentered"),
    "lights": ("Light", "createLightCentered"),
}


def check_panel(slug):
    layout = yaml.safe_load((ROOT / "layout" / f"{slug}.yaml").read_text())
    source = (ROOT / "src" / f"{slug}.cpp").read_text()
    header = (ROOT / "src" / "layout" / f"{slug}.hpp").read_text()
    svg = ET.parse(ROOT / "res" / f"{slug}.svg").getroot()
    width = layout["hp"] * HP_MM
    assert layout["slug"] == slug and type(layout["hp"]) is int and width > 0
    assert svg.get("width") == f"{width:.2f}mm" and svg.get("height") == f"{PANEL_H}mm"
    assert not any(node.tag.rsplit("}", 1)[-1] == "text" for node in svg.iter())
    assert re.search(rf'createModel<[^>]+>\("{slug}"\)', source)
    assert f'"res/{slug}.svg"' in source

    boxes = {}
    count = 0
    for kind, (enum, factory) in KINDS.items():
        definition = re.search(rf"enum {enum}Id\s*\{{([^}}]+)\}}", source)[1]
        definition = re.sub(r"//[^\n]*", "", definition)
        ids = [name.strip() for name in definition.split(",") if name.strip()]
        assert ids.pop() == f"{kind.upper()}_LEN", f"{kind} enum sentinel"
        positions = layout.get(kind, [])
        assert Counter(ids) == Counter(p["id"] for p in positions), f"{kind} YAML coverage"
        placed = re.findall(
            rf"{factory}<(.+?)>\(mm2px\(Vec\(L::(\w+)_X, L::\2_Y\)\),\s*module,\s*M::(\w+)",
            source,
        )
        assert Counter(ids) == Counter(pid for _, _, pid in placed), f"{kind} widget coverage"
        for widget, pos, pid in placed:
            assert pos == pid, f"{pid} uses {pos}'s coordinates"
        widgets = {pid: widget for widget, _, pid in placed}
        for p in positions:
            pid, x, y = p["id"], float(p["x"]), float(p["y"])
            assert f"{pid}_X = {x}f, {pid}_Y = {y}f;" in header, f"{pid} stale header"
            w, h = FOOTPRINTS[widgets[pid]]
            boxes[pid] = (x - w / 2, y - h / 2, x + w / 2, y + h / 2)
        count += len(ids)

    if "display" in layout:
        x, y, w, h = layout["display"]
        boxes["display"] = (x, y, x + w, y + h)
    # Rack mounting screws use a 15 px square at 75 dpi.
    screw = 5.08
    for col, x in enumerate((screw, width - 2 * screw)):
        for row, y in enumerate((0, PANEL_H - screw)):
            boxes[f"screw {row}/{col}"] = (x, y, x + screw, y + screw)
    for name, (x1, y1, x2, y2) in boxes.items():
        assert 0 <= x1 < x2 <= width and 0 <= y1 < y2 <= PANEL_H, f"{name} outside panel"
    for (a, box_a), (b, box_b) in combinations(boxes.items(), 2):
        ax, ay, ar, ab = box_a
        bx, by, br, bb = box_b
        assert max(ax, bx) >= min(ar, br) or max(ay, by) >= min(ab, bb), f"{a} overlaps {b}"
    return count


def main():
    manifest = json.loads((ROOT / "plugin.json").read_text())
    slugs = [module["slug"] for module in manifest["modules"]]
    for folder, pattern in (("layout", "*.yaml"), ("res", "*.svg"), ("src/layout", "*.hpp")):
        assert Counter(slugs) == Counter(p.stem for p in (ROOT / folder).glob(pattern)), folder
    assert re.findall(r"p->addModel\(model(\w+)\)", (ROOT / "src/plugin.cpp").read_text()) == slugs
    count, failed = 0, False
    for slug in slugs:
        try:
            count += check_panel(slug)
        except (AssertionError, KeyError, TypeError) as error:
            print(f"{slug}: {error or 'panel or wrapper mismatch'}", file=sys.stderr)
            failed = True
    if failed:
        return 1
    print(f"{len(slugs)} panels, {count} Rack params, inputs, outputs and lights placed without overlap.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
