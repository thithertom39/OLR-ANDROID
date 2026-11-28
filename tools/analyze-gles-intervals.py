#!/usr/bin/env python3
"""Summarize complete frame interval windows recorded by -gles_frame_intervals."""

import argparse
import json
import math
import re
import statistics
from pathlib import Path


WINDOW = re.compile(
    r"\[gles_perf\].*device_frame=(\d+) camera=(\S+).*direction=(\S+)"
    r" max_pos_delta=(\S+) max_dir_delta=(\S+) render=(\d+x\d+)"
)
INTERVALS = re.compile(r"\[gles_frame_intervals\] device_frame=(\d+) frames=(\d+) us=([0-9,]+)")


def load_windows(path, resolution, direction, position):
    scene = {}
    windows = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if window := WINDOW.search(line):
            frame, cam, facing, moved, turned, size = window.groups()
            scene[int(frame)] = (list(map(float, cam.split(","))), list(map(float, facing.split(","))),
                                 float(moved), float(turned), size)
        if capture := INTERVALS.search(line):
            frame, count, values = capture.groups()
            frame = int(frame)
            if frame not in scene:
                continue
            cam, facing, moved, turned, size = scene[frame]
            if size != resolution or moved > 0.05 or turned > 0.01:
                continue
            if position and math.dist(cam, position) > 0.1:
                continue
            if direction and math.dist(facing, direction) > 0.03:
                continue
            intervals = [int(value) for value in values.split(",")]
            if len(intervals) != int(count):
                continue
            windows.append((frame, cam, facing, intervals))
    return windows


def summarize(values):
    ordered = sorted(values)

    def percentile(fraction):
        return ordered[math.ceil(len(ordered) * fraction) - 1] / 1000

    return {
        "frames": len(ordered),
        "fps": 1_000_000 / statistics.mean(ordered),
        "p50_ms": percentile(0.50),
        "p95_ms": percentile(0.95),
        "p99_ms": percentile(0.99),
        "max_ms": ordered[-1] / 1000,
        "one_percent_low_fps": 1_000_000 / statistics.mean(ordered[-math.ceil(len(ordered) * 0.01):]),
        "frames_over_ms": {str(ms): sum(value > ms * 1000 for value in ordered)
                           for ms in (1000 / 60, 25, 33.33)},
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="+", type=Path)
    parser.add_argument("--resolution", default="1920x1080")
    parser.add_argument("--direction", type=float, nargs=3)
    parser.add_argument("--position", type=float, nargs=3)
    parser.add_argument("--skip-windows", type=int, default=2)
    parser.add_argument("--windows", type=int, default=8)
    args = parser.parse_args()
    if args.skip_windows < 0 or args.windows < 1:
        parser.error("require a non-negative warmup and at least one measurement window")
    results = []
    for path in args.files:
        windows = load_windows(path, args.resolution, args.direction, args.position)
        windows = windows[args.skip_windows:args.skip_windows + args.windows]
        if not windows:
            parser.error(f"{path}: no matching complete windows")
        results.append({
            "file": str(path),
            "resolution": args.resolution,
            "summary": summarize([us for _, _, _, intervals in windows for us in intervals]),
            "windows": [
                {"end_frame": frame, "camera": cam, "direction": facing, **summarize(intervals)}
                for frame, cam, facing, intervals in windows
            ],
        })
    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
