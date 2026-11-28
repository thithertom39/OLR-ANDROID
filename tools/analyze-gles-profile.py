#!/usr/bin/env python3
"""Existing GLES JSONL summary, adapted for plain R2 camera comparisons."""
import argparse
from collections import defaultdict
import json
import math
from pathlib import Path
import statistics

def distribution(values):
    values=sorted(v for v in values if v is not None and v>=0)
    if not values:return None
    def percentile(f):
        at=(len(values)-1)*f;lo=int(at)
        return values[lo]+(values[min(lo+1,len(values)-1)]-values[lo])*(at-lo)
    return dict(samples=len(values),mean=statistics.mean(values),p50=percentile(.5),p95=percentile(.95),p99=percentile(.99))

def summarize(path,tail):
    rows=[];malformed=0
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        try:rows.append(json.loads(line))
        except json.JSONDecodeError:malformed+=1
    metadata=next((r for r in rows if r.get('record')=='metadata'),{})
    cpu={r['frame']:r for r in rows if r.get('record')=='cpu'}
    gpu={r['frame']:r for r in rows if r.get('record')=='gpu'}
    groups=defaultdict(list)
    for frame,row in cpu.items():
        camera=tuple(round(v,1 if i<3 else 2) for i,v in enumerate(row.get('camera',[])))
        groups[(row.get('width'),row.get('height'),camera)].append(frame)
    result=[]
    for (width,height,camera),frames in groups.items():
        if len(frames)<4:continue
        if tail:frames=frames[-tail:]
        intervals=[cpu[f].get('interval_ms',-1) for f in frames]
        intervals=sorted(v for v in intervals if v>0)
        item=dict(width=width,height=height,camera=camera,samples=len(frames),
                  device_frame_range=[min(cpu[f]['device_frame'] for f in frames),max(cpu[f]['device_frame'] for f in frames)],
                  interval_ms=distribution(intervals),render_wall_ms=distribution([cpu[f].get('render_wall_ms') for f in frames]),
                  render_thread_cpu_ms=distribution([cpu[f].get('render_thread_cpu_ms') for f in frames]),
                  draw_calls=distribution([cpu[f].get('draw_calls') for f in frames]),
                  matched_gpu_samples=sum(f in gpu for f in frames),
                  matched_complete_gpu_samples=sum(f in gpu and gpu[f].get('complete', True) for f in frames),
                  stages={})
        item['vertex_binding_updates']=distribution([cpu[f].get('vertex_binding_updates') for f in frames])
        item['legacy_vertex_pointer_batches']=distribution([cpu[f].get('legacy_vertex_pointer_batches') for f in frames])
        item['local_detail_cull']={key:distribution([cpu[f].get('local_detail_cull',{}).get(key) for f in frames])
            for key in sorted({k for f in frames for k in cpu[f].get('local_detail_cull',{})})}
        cache = [cpu[f]['static_index_cache'] for f in frames if 'static_index_cache' in cpu[f]]
        if cache:
            item['static_index_cache'] = {}
            for key in ('main_enabled', 'shadow_enabled'):
                item['static_index_cache'][key] = sorted({row[key] for row in cache})
            for key in ('resident_bytes', 'resident_entries', 'retained_source_bytes'):
                item['static_index_cache'][key] = distribution(row.get(key) for row in cache)
            for phase in ('main', 'shadow'):
                keys = sorted({key for row in cache for key in row.get(phase, {})})
                item['static_index_cache'][phase] = {
                    key: distribution(row.get(phase, {}).get(key) for row in cache) for key in keys
                }
        if intervals:
            item['fps_from_mean_interval']=1000/statistics.mean(intervals)
            item['sampled_1percent_low_fps']=1000/statistics.mean(intervals[-max(1,math.ceil(len(intervals)*.01)):])
        for i,name in enumerate(metadata.get('stages',[])):
            def metric(source,key):
                return distribution([source[f][key][i] for f in frames if f in source and i<len(source[f].get(key,[]))])
            item['stages'][name]=dict(wall_ms=metric(cpu,'inclusive_wall_ms'),thread_cpu_ms=metric(cpu,'inclusive_thread_cpu_ms'),gpu_timeline_ms=metric(gpu,'inclusive_ms'))
        result.append(item)
    return dict(file=str(path),metadata=metadata,cpu_samples=len(cpu),gpu_samples=len(gpu),
                complete_gpu_samples=sum(r.get('complete',True) for r in gpu.values()),
                gpu_drop_records=[r for r in rows if r.get('record')=='gpu_drop'],
                gpu_issues=[r for r in rows if r.get('record')=='gpu_issue'],malformed_rows_ignored=malformed,
                cameras=sorted(result,key=lambda r:-r['samples']))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('files',nargs='+',type=Path)
    p.add_argument('--output',required=True,type=Path);p.add_argument('--tail-samples',type=int,default=0)
    args=p.parse_args();report=dict(note='The GPU timeline includes gaps waiting for CPU submission. Nested stages overlap. In schema 4, stages with missing or invalid data are omitted from distributions, even when a frame has other valid GPU stages. 1% low is derived from sampled intervals and may have limited confidence.',profiles=[summarize(f,args.tail_samples) for f in args.files])
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps([dict(file=r['file'],cpu=r['cpu_samples'],gpu=r['gpu_samples'],cameras=len(r['cameras'])) for r in report['profiles']]))
