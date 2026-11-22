#!/usr/bin/env python3
"""Summarize sampled GLES frames by stationary camera, without inventing GPU data."""
import argparse
from collections import defaultdict
import json
from pathlib import Path
import statistics


def distribution(values):
    values = sorted(v for v in values if v is not None and v >= 0)
    if not values:
        return None
    def percentile(f):
        index = (len(values)-1)*f
        lo = int(index)
        return values[lo] + (values[min(lo+1,len(values)-1)]-values[lo])*(index-lo)
    return dict(samples=len(values), mean=statistics.mean(values),
                p50=percentile(.5), p95=percentile(.95), p99=percentile(.99))


def summarize(path, tail_samples=0):
    records = []
    incomplete = 0
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        try:
            records.append(json.loads(line))
        except json.JSONDecodeError:
            incomplete += 1  # a final buffered/partial row is not a measurement
    metadata = next((r for r in records if r.get("record") == "metadata"), {})
    cpu = {r["frame"]: r for r in records if r.get("record") == "cpu"}
    gpu = {r["frame"]: r for r in records if r.get("record") == "gpu"}
    submissions = {r["frame"]: r for r in records if r.get("record") == "r1_submissions"}
    materials = defaultdict(list)
    for record in records:
        if record.get("record") == "r1_material":
            materials[record["frame"]].append(record)
    groups = defaultdict(list)
    for frame, row in cpu.items():
        # Position and direction remain in the raw file at full precision.
        camera = row.get("camera", [])
        key = tuple(round(v,1 if i<3 else 2) for i,v in enumerate(camera))
        groups[key].append(frame)
    result = []
    for camera, frames in groups.items():
        if len(frames)<4:
            continue
        if tail_samples:
            frames = frames[-tail_samples:]
        item = dict(camera=camera, samples=len(frames),
            interval_ms=distribution([cpu[f].get("interval_ms") for f in frames]),
            render_cpu_ms=distribution([cpu[f].get("render_thread_cpu_ms") for f in frames]),
            gpu_matched_samples=sum(f in gpu for f in frames), stages={})
        for index, name in enumerate(metadata.get("stages", [])):
            cpu_times = [cpu[f].get("inclusive_wall_ms", [])[index] for f in frames
                         if index < len(cpu[f].get("inclusive_wall_ms", []))]
            gpu_times = [gpu[f]["inclusive_ms"][index] for f in frames if f in gpu]
            thread_times = [cpu[f]["inclusive_thread_cpu_ms"][index] for f in frames
                            if index < len(cpu[f].get("inclusive_thread_cpu_ms", []))]
            item["stages"][name] = dict(cpu_wall_ms=distribution(cpu_times),
                                       cpu_thread_ms=distribution(thread_times),gpu_ms=distribution(gpu_times))
        for key in ("draw_calls", "detail_instances_submitted"):
            item[key] = distribution([cpu[f].get(key) for f in frames])
        for key in ("triangles","renderer_framebuffer_binds","attachment_changes","fallback_draws","native_base_draws",
                    "native_point_draws","native_spot_draws","deferred_draws","deferred_receivers","r2_lights_skipped"):
            item[key] = distribution([submissions.get(f,{}).get(key) for f in frames])
        totals = defaultdict(int)
        for frame in frames:
            for material in materials[frame]:
                totals[material["shader"]] += material["draws"]
        item["mean_material_draws"] = dict(sorted(((k,v/len(frames)) for k,v in totals.items()),
                                                  key=lambda pair: -pair[1]))
        result.append(item)
    return dict(file=str(path),metadata=metadata,cpu_samples=len(cpu),gpu_samples=len(gpu),
                gpu_drop_records=[r for r in records if r.get("record")=="gpu_drop"],
                malformed_rows_ignored=incomplete,cameras=sorted(result,key=lambda x:-x["samples"]))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files",nargs="+",type=Path)
    parser.add_argument("--output",required=True,type=Path)
    parser.add_argument("--tail-samples",type=int,default=0,
                        help="Use only the last N sampled frames of each stationary camera (exclude warm-up)")
    args=parser.parse_args()
    result=dict(note="Sampled diagnostic frames; nested pass times overlap. GPU gaps/drops are not zero time. "
                     "These short scenes do not establish sustained FPS or thermal behavior.",
                tail_samples_per_camera=args.tail_samples,
                profiles=[summarize(path,args.tail_samples) for path in args.files])
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"output":str(args.output),"profiles":[dict(file=p["file"],cpu=p["cpu_samples"],
        gpu=p["gpu_samples"],cameras=len(p["cameras"])) for p in result["profiles"]]},indent=2))


if __name__ == "__main__":
    main()
