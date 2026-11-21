"""Read-only inspection of original OLR spawn packets. Does not export game assets."""
import argparse
import json
import math
import re
import struct
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("spawn", type=Path)
args = parser.parse_args()
data = args.spawn.read_bytes()
target = (-244.6303, -9.082104, -134.4171)  # compiled Cordon light #13
lamps = []
for match in re.finditer(rb"lights_hanging_lamp\x00", data):
    # CSE_Abstract::Spawn_Read: M_SPAWN, sectionZ, nameZ, gameid:u8,
    # RP:u8, position:vec3. Reject incidental configuration/string data.
    if match.start() < 2 or struct.unpack_from("<H", data, match.start() - 2)[0] != 1:
        continue
    end = data.find(b"\x00", match.end())
    if end < 0 or end - match.end() > 255 or end + 15 > len(data):
        continue
    name = data[match.end():end].decode("cp1251", errors="replace")
    position = struct.unpack_from("<3f", data, end + 3)
    if not all(math.isfinite(v) and abs(v) < 100000 for v in position):
        continue
    lamps.append({"name": name, "position": position,
        "distance_to_compiled_light_13": math.dist(position, target), "packet_offset": match.start() - 2})
lamps.sort(key=lambda item: item["distance_to_compiled_light_13"])
print(json.dumps({"spawn": str(args.spawn), "validated_packet_count": len(lamps),
    "nearest_lamps": lamps[:8]}, indent=2, ensure_ascii=False))
