#!/usr/bin/env python3
"""Read-only material inventory; writes metadata, never copies game assets.

Level chunk 2, OGF_TEXTURE/OGF_CHILDREN, shader descriptors and particle sprite
references follow the actual engine loaders. Class coverage is conservative:
conditional native coverage and unverified Lua/forward paths are not 'done'.
"""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re
import struct


def chunks(stream, start=0, length=None):
    end = start + length if length is not None else stream.seek(0, 2)
    cursor = start
    while cursor < end:
        stream.seek(cursor)
        header = stream.read(8)
        if len(header) != 8:
            raise ValueError(f"truncated chunk header at {cursor}")
        chunk_id, size = struct.unpack("<II", header)
        if cursor+8+size > end:
            raise ValueError(f"chunk {chunk_id:#x} exceeds parent at {cursor}")
        yield chunk_id & 0x7fffffff, cursor+8, size, bool(chunk_id & 0x80000000)
        cursor += 8+size


def read_z(data, offset=0):
    end = data.index(b"\0", offset)
    return data[offset:end].decode("cp1251"), end+1


def payload(stream, item):
    _, offset, size, compressed = item
    if compressed:
        raise ValueError("compressed metadata chunk requires the engine decompressor")
    stream.seek(offset)
    return stream.read(size)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("assets", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    root = args.assets.resolve()
    definitions, uses, errors = {}, defaultdict(Counter), []
    textures = defaultdict(set)
    file_counts = Counter()

    def add(shader, texture, source):
        shader = shader.replace("/", "\\").lower()
        if not shader:
            return
        uses[shader][source] += 1
        if texture:
            textures[shader].add(texture)

    with (root/"shaders.xr").open("rb") as stream:
        container = next(c for c in chunks(stream) if c[0] == 2)
        if container[3]:
            raise ValueError("compressed shaders.xr blender container")
        for item in chunks(stream, container[1], container[2]):
            data = payload(stream, item)
            if len(data) < 174:
                raise ValueError("truncated CBlender_DESC")
            shader_class = data[:8][::-1].decode("ascii").strip()
            alias, _ = read_z(data, 8)
            version = struct.unpack_from("<H", data, 172)[0]
            definitions[alias.lower()] = dict(blender_class=shader_class, version=version)

    for path in sorted((root/"levels").glob("*/level")):
        try:
            with path.open("rb") as stream:
                item = next(c for c in chunks(stream) if c[0] == 2)
                data = payload(stream, item)
            count, = struct.unpack_from("<I", data)
            offset = 4
            for _ in range(count):
                material, offset = read_z(data, offset)
                if "/" in material:
                    shader, texture = material.split("/", 1)
                    add(shader, texture, "level:"+path.parent.name)
            if offset != len(data):
                raise ValueError("unexpected trailing level shader data")
            file_counts["levels"] += 1
            file_counts["level_shader_entries"] += count
        except (OSError, ValueError, StopIteration, struct.error) as exc:
            errors.append(dict(path=str(path.relative_to(root)), error=str(exc)))

    def model(stream, start, length, source):
        # Match the loader's find_chunk behavior: stop after the material or
        # embedded children. Some shipped skeletons have an undersized motion
        # reference chunk after this data; it is not a material read failure.
        for item in chunks(stream, start, length):
            if item[0] == 2:  # xrCore/FMesh.hpp: OGF_TEXTURE
                data = payload(stream, item)
                texture, offset = read_z(data)
                shader, _ = read_z(data, offset)
                add(shader, texture, source)
                file_counts["model_texture_chunks"] += 1
                return
            elif item[0] == 9:  # embedded OGF_CHILDREN, not filename links
                if item[3]:
                    raise ValueError("compressed OGF children")
                for child in list(chunks(stream, item[1], item[2])):
                    if child[3]:
                        raise ValueError("compressed embedded OGF")
                    model(stream, child[1], child[2], source)
                return

    for path in sorted((root/"meshes").rglob("*.ogf")):
        try:
            with path.open("rb") as stream:
                size = stream.seek(0, 2)
                model(stream, 0, size, "models")
            file_counts["model_files"] += 1
        except (OSError, ValueError, struct.error) as exc:
            errors.append(dict(path=str(path.relative_to(root)), error=str(exc)))

    for path in sorted((root/"particles").rglob("*.pe")):
        try:
            content = path.read_text(encoding="cp1251")
            for match in re.finditer(r"(?m)^\s*shader\s*=\s*([^;\r\n]+)", content):
                add(match.group(1).strip(), "", "particles")
            file_counts["particle_definitions"] += 1
        except (OSError, ValueError) as exc:
            errors.append(dict(path=str(path.relative_to(root)), error=str(exc)))

    # CPSLibrary::Load -> PS_CHUNK_SECONDGEN -> CPEDef::Load's sprite chunk.
    particle_library = root/"particles.xr"
    if particle_library.exists():
        try:
            with particle_library.open("rb") as stream:
                container = next(c for c in chunks(stream) if c[0] == 3)
                if container[3]:
                    raise ValueError("compressed particle container")
                for effect in list(chunks(stream, container[1], container[2])):
                    if effect[3]:
                        raise ValueError("compressed particle definition")
                    for item in chunks(stream, effect[1], effect[2]):
                        if item[0] == 7:
                            data = payload(stream, item)
                            shader, offset = read_z(data)
                            texture, _ = read_z(data, offset)
                            add(shader, texture, "particle_library")
                            file_counts["particle_sprites"] += 1
                    file_counts["particle_effects"] += 1
        except (OSError, ValueError, StopIteration, struct.error) as exc:
            errors.append(dict(path="particles.xr", error=str(exc)))

    r1_lua = {p.stem.lower(): p for p in (root/"shaders/r1").glob("*.s")}
    repository = Path(__file__).resolve().parents[1]
    gl_lua = {p.stem.lower(): p for p in (repository/"engine/res/olr/gamedata/shaders/gl").glob("*.s")}
    native = {"LM": "native_lmap_conditional", "V": "native_vertex_conditional",
              "LM_AREF": "native_opaque_aref_only", "V_AREF": "native_opaque_aref_only",
              "BmmDold": "native_implicit_requires_detail_metadata", "D_TREE": "native_tree",
              "D_STILL": "native_detail", "MODEL": "missing_model_ros_projector_shadows",
              "MODELEbB": "missing_model_environment", "LmEbB": "forward_environment_unverified",
              "PARTICLE": "native_particle_cpp_independent_only_scene_unverified"}
    rows = []
    for shader in sorted(set(uses)|set(definitions)):
        desc = definitions.get(shader, {})
        script = shader.replace("\\", "_")
        coverage = native.get(desc.get("blender_class"), "unclassified")
        if script in gl_lua:
            coverage = {"default": "native_lmap_conditional", "def_vertex": "native_vertex_conditional",
                        "details_lod": "native_lod"}.get(script, "lua_forward_or_deferred_unverified")
        rows.append(dict(alias=shader, **desc, references=dict(uses.get(shader, {})),
                         texture_lists=sorted(textures.get(shader, set())),
                         original_r1_lua=script in r1_lua, gl_lua=script in gl_lua, coverage=coverage))
    report = dict(assets=str(root), note="Static definitions/references, not runtime coverage or parity proof. "
        "Definitions may be unused; script-only/generated materials still require runtime inventory.",
        counts=dict(file_counts), errors=errors, used_aliases=sum(bool(row["references"]) for row in rows),
        definitions=len(definitions), materials=rows)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    print(json.dumps({key:report[key] for key in ("counts","used_aliases","definitions")},indent=2))
    print("metadata_read_errors="+str(len(errors)))
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
