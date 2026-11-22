#!/usr/bin/env python3
"""Inspect engine RGBA readbacks without game assets or image libraries."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib


def statistics(data):
    pixels=list(zip(*[iter(data)]*4))
    channels=list(zip(*pixels))
    return dict(pixels=len(pixels),distinct_rgba=len(set(pixels)),
                nonzero_rgb=sum(any(p[:3]) for p in pixels),
                nonzero_alpha=sum(p[3]!=0 for p in pixels),
                minimum=[min(c) for c in channels],maximum=[max(c) for c in channels],
                mean=[sum(c)/len(c) for c in channels])


def chunk(name,data):
    return struct.pack(">I",len(data))+name+data+struct.pack(">I",zlib.crc32(name+data)&0xffffffff)


def png_bytes(rgba,width,height):
    scanlines=b"".join(b"\0"+rgba[row*width*4:(row+1)*width*4] for row in reversed(range(height)))
    return (b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",width,height,8,6,0,0,0))+
            chunk(b"IDAT",zlib.compress(scanlines))+chunk(b"IEND",b""))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture",type=Path)
    parser.add_argument("--tiles",type=int,default=0)
    args=parser.parse_args()
    raw=args.capture.read_bytes()
    width,height=struct.unpack_from("<II",raw)
    if not width or not height or len(raw)!=8+width*height*4:
        raise ValueError("Capture dimensions/payload do not match")
    rgba=raw[8:]
    report=dict(file=str(args.capture),sha256=hashlib.sha256(raw).hexdigest(),
                width=width,height=height,origin="GLES lower left",statistics=statistics(rgba))
    if args.tiles:
        if width!=512 or height!=512 or not 0<args.tiles<=100:
            raise ValueError("Original projector atlas is 512x512, up to 100 tiles")
        report["tiles"]=[]
        for i in range(args.tiles):
            x,y=(i%10)*51,(i//10)*51
            tile=b"".join(rgba[((y+row)*width+x)*4:((y+row)*width+x+51)*4] for row in range(51))
            report["tiles"].append(dict(slot=i,x=x,y=y,**statistics(tile)))
    # Convert the actual framebuffer readback to a viewable PNG, preserving
    # RGBA values exactly and only accounting for the different row origins.
    args.capture.with_suffix(".png").write_bytes(png_bytes(rgba,width,height))
    if args.tiles:
        # Atlas A is a sun mask, not visual transparency. Separate derived
        # previews expose RGB and A without changing the authoritative raw data.
        rgb=bytearray(rgba)
        rgb[3::4]=b"\xff"*(width*height)
        alpha=b"".join(bytes((a,a,a,255)) for a in rgba[3::4])
        args.capture.with_name(args.capture.stem+"-rgb.png").write_bytes(png_bytes(rgb,width,height))
        args.capture.with_name(args.capture.stem+"-sun.png").write_bytes(png_bytes(alpha,width,height))
    args.capture.with_suffix(".json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(report,indent=2))


if __name__=="__main__":
    main()
