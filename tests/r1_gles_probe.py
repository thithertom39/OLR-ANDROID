#!/usr/bin/env python3
"""Compile real R1 GLES sources and draw reference pixels with Mesa EGL.

Run in WSL/Linux, e.g.:
  python3 tests/r1_gles_probe.py --output logs/r1-gles-probe.json

Uses only Python's standard library plus system libEGL/libGLESv2. It expands
the repository's actual includes (including Windows separators), leaving GLSL
conditionals to the real driver compiler. No replacement shader functions or
mock GLES implementation are used. This is a software-driver correctness
probe, not an Adreno/device test or a scene/visual parity test.
"""

import argparse
import ctypes as C
import ctypes.util
import hashlib
import json
import math
import os
from pathlib import Path
import re
import sys
import traceback


ROOT = Path(__file__).resolve().parents[1]
SHADERS = ROOT / "engine/res/olr/gamedata/shaders/gl"
VARIANTS = ("", "_detail", "_vertex", "_vertex_detail", "_point", "_spot", "_implicit_detail")
SOURCES = {}


def expand(path, stack=()):
    """Engine-style include expansion, followed by driver preprocessing."""
    path = path.resolve()
    if path in stack:
        raise RuntimeError("Recursive GLSL include: " + str(path))
    raw = path.read_bytes()
    SOURCES[str(path.relative_to(ROOT))] = hashlib.sha256(raw).hexdigest()
    source = raw.decode("utf-8-sig")

    def include(match):
        name = match.group(1).replace("\\", "/")
        local = path.parent / name
        target = local if local.is_file() else SHADERS / name
        if not target.is_file():
            raise FileNotFoundError(f"GLSL include {name!r} from {path}")
        return "\n" + expand(target, stack + (path,)) + "\n"

    return re.sub(r'^\s*#\s*include\s+"([^"]+)"[^\n]*$', include,
                  source, flags=re.MULTILINE)


def shader_source(name, debug, scene=False, vtf=False):
    preamble = """#version 320 es
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp sampler3D;
precision highp samplerCube;
#define USE_SHOC_RESOURCES
#define OLR_R1_NATIVE
"""
    options = ("#define OLR_R1_SCENE\n" if scene else "") + ("#define USE_VTF\n" if vtf else "")
    return preamble + options + f"#define OLR_R1_DEBUG {debug}\n" + expand(SHADERS / name)


def function(library, name, result, *arguments):
    fn = getattr(library, name)
    fn.restype = result
    fn.argtypes = arguments
    return fn


class GLES:
    def __init__(self):
        os.environ.setdefault("EGL_PLATFORM", "surfaceless")
        os.environ.setdefault("LIBGL_ALWAYS_SOFTWARE", "1")
        os.environ.setdefault("GALLIUM_DRIVER", "llvmpipe")
        self.egl = C.CDLL(ctypes.util.find_library("EGL") or "libEGL.so.1")
        self.gl = C.CDLL(ctypes.util.find_library("GLESv2") or "libGLESv2.so.2")
        ptr, integer, uint, floating = C.c_void_p, C.c_int, C.c_uint, C.c_float
        self.get_error = function(self.egl, "eglGetError", uint)
        get_display = function(self.egl, "eglGetDisplay", ptr, ptr)
        initialize = function(self.egl, "eglInitialize", uint, ptr,
                              C.POINTER(integer), C.POINTER(integer))
        bind = function(self.egl, "eglBindAPI", uint, uint)
        choose = function(self.egl, "eglChooseConfig", uint, ptr,
                          C.POINTER(integer), C.POINTER(ptr), integer,
                          C.POINTER(integer))
        create_context = function(self.egl, "eglCreateContext", ptr, ptr, ptr,
                                  ptr, C.POINTER(integer))
        create_surface = function(self.egl, "eglCreatePbufferSurface", ptr,
                                  ptr, ptr, C.POINTER(integer))
        make_current = function(self.egl, "eglMakeCurrent", uint, ptr, ptr,
                                ptr, ptr)
        self.display = get_display(None)
        major, minor = integer(), integer()
        if not self.display or not initialize(self.display, C.byref(major), C.byref(minor)):
            raise RuntimeError(f"eglInitialize failed: {self.get_error():#x}")
        if not bind(0x30A0):  # EGL_OPENGL_ES_API
            raise RuntimeError(f"eglBindAPI failed: {self.get_error():#x}")
        attrs = (integer * 13)(0x3033, 1, 0x3040, 0x40,  # PBUFFER, ES3_BIT
                               0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3021, 8, 0x3038)
        config, count = ptr(), integer()
        if not choose(self.display, attrs, C.byref(config), 1, C.byref(count)) or count.value != 1:
            raise RuntimeError(f"eglChooseConfig failed: {self.get_error():#x}")
        ctx_attrs = (integer * 5)(0x3098, 3, 0x30FB, 2, 0x3038)
        self.context = create_context(self.display, config, None, ctx_attrs)
        surface_attrs = (integer * 5)(0x3057, 1, 0x3056, 1, 0x3038)
        self.surface = create_surface(self.display, config, surface_attrs)
        if not self.context or not self.surface or not make_current(
                self.display, self.surface, self.surface, self.context):
            raise RuntimeError(f"EGL ES 3.2 context failed: {self.get_error():#x}")

        signatures = {
            "glGetString": (C.c_char_p, uint),
            "glGetError": (uint,),
            "glCreateShader": (uint, uint),
            "glShaderSource": (None, uint, integer, C.POINTER(C.c_char_p), C.POINTER(integer)),
            "glCompileShader": (None, uint),
            "glGetShaderiv": (None, uint, uint, C.POINTER(integer)),
            "glGetShaderInfoLog": (None, uint, integer, C.POINTER(integer), ptr),
            "glDeleteShader": (None, uint),
            "glCreateProgram": (uint,),
            "glAttachShader": (None, uint, uint),
            "glLinkProgram": (None, uint),
            "glGetProgramiv": (None, uint, uint, C.POINTER(integer)),
            "glGetProgramInfoLog": (None, uint, integer, C.POINTER(integer), ptr),
            "glUseProgram": (None, uint),
            "glGetUniformLocation": (integer, uint, C.c_char_p),
            "glUniform1i": (None, integer, integer),
            "glUniform3f": (None, integer, floating, floating, floating),
            "glUniform4f": (None, integer, floating, floating, floating, floating),
            "glUniformMatrix4fv": (None, integer, integer, C.c_ubyte, C.POINTER(floating)),
            "glUniformMatrix4x3fv": (None, integer, integer, C.c_ubyte, C.POINTER(floating)),
            "glGenTextures": (None, integer, C.POINTER(uint)),
            "glBindTexture": (None, uint, uint),
            "glTexParameteri": (None, uint, uint, integer),
            "glTexImage2D": (None, uint, integer, integer, integer, integer, integer, uint, uint, ptr),
            "glActiveTexture": (None, uint),
            "glGenFramebuffers": (None, integer, C.POINTER(uint)),
            "glBindFramebuffer": (None, uint, uint),
            "glFramebufferTexture2D": (None, uint, uint, uint, uint, integer),
            "glCheckFramebufferStatus": (uint, uint),
            "glGenVertexArrays": (None, integer, C.POINTER(uint)),
            "glBindVertexArray": (None, uint),
            "glGenBuffers": (None, integer, C.POINTER(uint)),
            "glBindBuffer": (None, uint, uint),
            "glBufferData": (None, uint, C.c_ssize_t, ptr, uint),
            "glVertexAttribPointer": (None, uint, integer, uint, C.c_ubyte, integer, ptr),
            "glEnableVertexAttribArray": (None, uint),
            "glVertexAttrib4f": (None, uint, floating, floating, floating, floating),
            "glViewport": (None, integer, integer, integer, integer),
            "glDisable": (None, uint),
            "glEnable": (None, uint),
            "glBlendFunc": (None, uint, uint),
            "glClearColor": (None, floating, floating, floating, floating),
            "glClear": (None, uint),
            "glDrawArrays": (None, uint, integer, integer),
            "glReadPixels": (None, integer, integer, integer, integer, uint, uint, ptr),
            "glFinish": (None,),
        }
        for name, (result, *args) in signatures.items():
            setattr(self, name, function(self.gl, name, result, *args))
        self.info = {
            "egl": f"{major.value}.{minor.value}",
            "gl_version": self.glGetString(0x1F02).decode(),
            "gl_renderer": self.glGetString(0x1F01).decode(),
            "gl_vendor": self.glGetString(0x1F00).decode(),
            "glsl": self.glGetString(0x8B8C).decode(),
        }

    def check(self, label):
        error = self.glGetError()
        if error:
            raise RuntimeError(f"{label}: GL error {error:#x}")

    def compile(self, stage, source):
        shader = self.glCreateShader(stage)
        raw = C.c_char_p(source.encode())
        self.glShaderSource(shader, 1, C.byref(raw), None)
        self.glCompileShader(shader)
        status, length = C.c_int(), C.c_int()
        self.glGetShaderiv(shader, 0x8B81, C.byref(status))
        self.glGetShaderiv(shader, 0x8B84, C.byref(length))
        log = C.create_string_buffer(max(1, length.value))
        self.glGetShaderInfoLog(shader, len(log), None, log)
        return shader, bool(status.value), log.value.decode(errors="replace")

    def program(self, vertex, fragment):
        vs, vok, vlog = self.compile(0x8B31, vertex)
        ps, pok, plog = self.compile(0x8B30, fragment)
        program = self.glCreateProgram()
        self.glAttachShader(program, vs)
        self.glAttachShader(program, ps)
        self.glLinkProgram(program)
        status, length = C.c_int(), C.c_int()
        self.glGetProgramiv(program, 0x8B82, C.byref(status))
        self.glGetProgramiv(program, 0x8B84, C.byref(length))
        log = C.create_string_buffer(max(1, length.value))
        self.glGetProgramInfoLog(program, len(log), None, log)
        self.glDeleteShader(vs)
        self.glDeleteShader(ps)
        return program, {"vertex_compile": vok, "fragment_compile": pok,
                         "link": bool(status.value), "vertex_log": vlog,
                         "fragment_log": plog, "link_log": log.value.decode(errors="replace")}

    def texture(self, pixel, width=1, height=1):
        tex = C.c_uint()
        self.glGenTextures(1, C.byref(tex))
        self.glBindTexture(0x0DE1, tex.value)
        if pixel is not None and len(pixel) != width * height * 4:
            raise ValueError("RGBA8 texture fixture size does not match dimensions")
        data = (C.c_ubyte * (width * height * 4))(*pixel) if pixel is not None else None
        self.glTexImage2D(0x0DE1, 0, 0x8058, width, height, 0, 0x1908, 0x1401, data)
        self.glTexParameteri(0x0DE1, 0x2801, 0x2601)  # LINEAR
        self.glTexParameteri(0x0DE1, 0x2800, 0x2601)
        self.glTexParameteri(0x0DE1, 0x2802, 0x812F)  # CLAMP_TO_EDGE
        self.glTexParameteri(0x0DE1, 0x2803, 0x812F)
        return tex.value

    def draw_setup(self):
        fbo = C.c_uint()
        self.glGenFramebuffers(1, C.byref(fbo))
        self.glBindFramebuffer(0x8D40, fbo.value)
        self.target = self.texture(None)
        self.glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, self.target, 0)
        if self.glCheckFramebufferStatus(0x8D40) != 0x8CD5:
            raise RuntimeError("RGBA8 framebuffer incomplete")
        vao, vbo = C.c_uint(), C.c_uint()
        self.glGenVertexArrays(1, C.byref(vao))
        self.glBindVertexArray(vao.value)
        self.glGenBuffers(1, C.byref(vbo))
        self.glBindBuffer(0x8892, vbo.value)
        vertices = (C.c_float * 12)(-1, -1, 0, 1, 3, -1, 0, 1, -1, 3, 0, 1)
        self.glBufferData(0x8892, C.sizeof(vertices), vertices, 0x88E4)
        self.glVertexAttribPointer(3, 4, 0x1406, 0, 16, None)  # actual POSITION=3
        self.glEnableVertexAttribArray(3)
        self.glVertexAttrib4f(5, 1, .5, .5, .7)  # packed BGRA -> normal +Z
        self.glVertexAttrib4f(4, 0, 0, 0, 0)
        self.glVertexAttrib4f(6, 0, 0, 0, 0)
        self.glVertexAttrib4f(8, 512, 512, 0, 1)
        self.glVertexAttrib4f(9, 16384, 16384, 0, 1)
        self.glVertexAttrib4f(0, .12, .10, .08, .3)
        self.glViewport(0, 0, 1, 1)
        for cap in (0x0B71, 0x0B44, 0x0BE2, 0x0BD0):  # depth,cull,blend,dither
            self.glDisable(cap)
        self.check("draw setup")

    def cube(self, unit, pixel):
        tex = C.c_uint()
        self.glActiveTexture(0x84C0 + unit)
        self.glGenTextures(1, C.byref(tex))
        self.glBindTexture(0x8513, tex.value)
        data = (C.c_ubyte * 4)(*pixel)
        for face in range(6):
            self.glTexImage2D(0x8515 + face, 0, 0x8058, 1, 1, 0, 0x1908, 0x1401, data)
        for key, value in ((0x2801, 0x2601), (0x2800, 0x2601),
                           (0x2802, 0x812F), (0x2803, 0x812F), (0x8072, 0x812F)):
            self.glTexParameteri(0x8513, key, value)

    def use(self, program, textures):
        self.glUseProgram(program)
        loc = lambda name: self.glGetUniformLocation(program, name.encode())
        identity = (C.c_float * 16)(1, 0, 0, 0, 0, 1, 0, 0,
                                    0, 0, 1, 0, 0, 0, 0, 1)
        self.glUniformMatrix4fv(loc("m_WVP"), 1, 0, identity)
        self.glUniformMatrix4fv(loc("m_VP"), 1, 0, identity)
        self.glUniformMatrix4fv(loc("olr_r1_light_xform"), 1, 0, identity)
        for name, value in {
            "fog_plane": (0, 0, 0, 1), "fog_color": (0, 0, 0, 0),
            "L_ambient": (.03, .04, .05, 0), "L_hemi_color": (.10, .15, .20, 0),
            "dt_params": (1, 1, 0, 0), "olr_r1_dynamic_pos": (0, 0, 10, .05),
            "olr_r1_dynamic_color": (.45, .45, .45, 1),
        }.items():
            self.glUniform4f(loc(name), *value)
        self.glUniform3f(loc("L_sun_color"), .20, .15, .10)
        self.glUniform3f(loc("L_sun_dir_w"), 0, 0, -1)
        self.glUniform3f(loc("eye_position"), 0, 0, 0)
        for unit, (name, pixel) in enumerate(textures.items()):
            self.glActiveTexture(0x84C0 + unit)
            self.texture(pixel)
            self.glUniform1i(loc(name), unit)
        self.check("program inputs")

    def pixel(self, clear=True):
        if clear:
            self.glClearColor(0, 0, 0, 0)
            self.glClear(0x4000)
        self.glDrawArrays(0x0004, 0, 3)
        self.glFinish()
        result = (C.c_ubyte * 4)()
        self.glReadPixels(0, 0, 1, 1, 0x1908, 0x1401, result)
        self.check("draw/read RGBA8")
        return list(result)


def pixel_checks(gl, programs):
    gl.draw_setup()
    textures = {"s_base": (96, 128, 160, 51), "s_lmap": (20, 30, 40, 64),
                "s_hemi": (32, 64, 96, 230), "s_detail": (90, 110, 130, 255)}
    base = [x / 255 for x in textures["s_base"]]
    lmap = [x / 255 for x in textures["s_lmap"]]
    hemi = 64 / 255
    ambient, sky, sun = (.03, .04, .05), (.1, .15, .2), (.2, .15, .1)
    light = [ambient[i] / 1.5 + lmap[i] + sky[i] * .98 * hemi + sun[i] * lmap[3]
             for i in range(3)]
    expected = [2 * base[i] * light[i] for i in range(3)]
    results = []

    def record(label, actual, reference, tolerance=1):
        rgba = [round(min(1, max(0, x)) * 255) for x in reference]
        ok = all(abs(a - b) <= tolerance for a, b in zip(actual, rgba))
        results.append({"name": label, "actual_rgba8": actual,
                        "expected_rgba8": rgba, "tolerance_lsb": tolerance, "passed": ok})

    gl.use(programs[("", 0)], textures)
    record("lmap_original_formula_second_map_rgb_not_base_alpha", gl.pixel(), expected + [base[3]])
    gl.use(programs[("_detail", 0)], textures)
    detail = [x / 255 for x in textures["s_detail"]]
    record("lmap_detail_modulation", gl.pixel(),
           [expected[i] * 2 * detail[i] for i in range(3)] + [base[3]])

    vertex_rgb = (.08, .10, .12)  # actual unpack_D3DCOLOR BGRA input
    vertex_light = [vertex_rgb[i] + ambient[i] + sky[i] * .98 * .7 + sun[i] * .3
                    for i in range(3)]
    gl.use(programs[("_vertex", 0)], textures)
    record("vertex_original_formula", gl.pixel(),
           [base[i] * min(vertex_light[i], 1) * 2 for i in range(3)] + [base[3]])

    # Draw the actual POINT program twice into an RGBA8 target. Constant lookup
    # samples isolate real VS Gouraud lighting, base alpha and ONE+ONE saturation.
    white = {"s_base": (255, 255, 255, 255), "s_lmap": (255, 255, 255, 255),
             "s_r1_att": (255, 255, 255, 255)}
    gl.use(programs[("_point", 0)], white)
    gl.glEnable(0x0BE2)
    gl.glBlendFunc(1, 1)
    lambert = (.5 * 10 / math.sqrt(102) + .25 * 10 / math.sqrt(110)
               + .25 * 10 / math.sqrt(110))
    point = .45 * lambert * 2
    first = gl.pixel()
    record("point_gouraud_lookup_product", first, [point, point, point, 0])
    record("additive_one_one_unorm_saturation", gl.pixel(clear=False), [1, 1, 1, 0])
    gl.glDisable(0x0BE2)

    implicit = programs[("_implicit_detail", 0)]
    gl.use(implicit, textures)
    # Different base/lmap coordinates and a nonconstant map distinguish the
    # source's tc1=tc0 from ordinary unpack_tc_lmap(v.uv1).
    gl.glVertexAttrib4f(8, 256, 256, 0, 1)  # base UV=.25, lightmap UV remains .5
    gl.glActiveTexture(0x84C1)
    gl.texture((20,30,40,64, 180,160,140,255,
                160,140,120,255, 140,120,100,255),2,2)
    implicit_light = [ambient[i]+lmap[i]+sky[i]*.98*base[3]+sun[i]*lmap[3] for i in range(3)]
    implicit_rgb = [2*base[i]*implicit_light[i]*2*detail[i] for i in range(3)]
    record("implicit_base_uv_alpha_hemi_and_full_ambient",gl.pixel(),implicit_rgb+[1])
    gl.glUniform4f(gl.glGetUniformLocation(implicit,b"dt_params"),1,1,0,100)
    record("olr_implicit_detail_remains_at_distance",gl.pixel(),implicit_rgb+[1])
    gl.glVertexAttrib4f(8,512,512,0,1)

    # Nonconstant samplers make xy/w and the UNDIVIDED z coordinate observable
    # independently. This intentionally specified homogeneous transform maps
    # every vertex to s=(.5,1.5,.25,2): projector UV=(.25,.75), clip UV=(.25,.25).
    # At the texel centers below, the 2x2 projector returns upper-left exactly;
    # the 4x1 clip texture interpolates texels 0/1 equally. Dividing z by w would
    # instead read texel 0. Omitting xy/w would read different projector RGB.
    spot = programs[("_spot", 0)]
    gl.use(spot, white)
    gl.glActiveTexture(0x84C1)
    gl.texture((12, 24, 36, 255, 48, 60, 72, 255,
                64, 128, 192, 255, 192, 64, 32, 255), 2, 2)
    gl.glActiveTexture(0x84C2)
    gl.texture((64, 96, 128, 255, 192, 224, 255, 255,
                10, 20, 30, 255, 210, 220, 230, 255), 4, 1)
    xform_location = gl.glGetUniformLocation(spot, b"olr_r1_light_xform")
    if xform_location < 0:
        raise RuntimeError("Real SPOT transform uniform unexpectedly inactive")
    xform = (C.c_float * 16)(0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, .5, 1.5, .25, 2)
    gl.glUniformMatrix4fv(xform_location, 1, 0, xform)
    projector_rgb = (64 / 255, 128 / 255, 192 / 255)
    clip_rgb = (128 / 255, 160 / 255, 191.5 / 255)
    record("spot_projective_xy_over_w_and_unprojected_clip_z", gl.pixel(),
           [point * projector_rgb[i] * clip_rgb[i] for i in range(3)] + [0])

    # Preserve an explicit nonblack target to distinguish discard from a black
    # output, which would still overwrite alpha and/or color without blending.
    xform[15] = -2
    gl.glUniformMatrix4fv(xform_location, 1, 0, xform)
    sentinel = (17, 29, 43, 61)
    gl.glClearColor(*(x / 255 for x in sentinel))
    gl.glClear(0x4000)
    record("spot_negative_projection_w_discards", gl.pixel(clear=False),
           [x / 255 for x in sentinel], tolerance=0)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "logs/r1-gles-probe.json")
    args = parser.parse_args()
    report = {"description": "Real repository shader compile/link and RGBA8 pixel draws; software GLES, not device proof",
              "defines": ["USE_SHOC_RESOURCES", "OLR_R1_NATIVE"],
              "debug_values": list(range(7)), "compile_results": [], "pixel_results": []}
    try:
        gl = GLES()
        report["environment"] = gl.info
        programs = {}
        for debug in range(7):
            for variant in VARIANTS:
                stem = "olr_r1_native" + variant
                vertex, fragment = shader_source(stem + ".vs", debug), shader_source(stem + ".ps", debug)
                program, result = gl.program(vertex, fragment)
                result.update(variant=stem, debug=debug,
                              vertex_expanded_sha256=hashlib.sha256(vertex.encode()).hexdigest(),
                              fragment_expanded_sha256=hashlib.sha256(fragment.encode()).hexdigest())
                report["compile_results"].append(result)
                if all(result[key] for key in ("vertex_compile", "fragment_compile", "link")):
                    programs[(variant, debug)] = program
                else:
                    print(json.dumps(result), file=sys.stderr)
        legacy_ok = len(programs) == len(VARIANTS)*7
        if legacy_ok:
            report["pixel_results"] = pixel_checks(gl, programs)
        scene_programs = {}
        for debug in range(7):
            for variant in VARIANTS:
                stem = "olr_r1_native" + variant
                program, result = gl.program(shader_source(stem + ".vs", debug, scene=True),
                                             shader_source(stem + ".ps", debug, scene=True))
                result.update(variant=stem, debug=debug, scene=True)
                report["compile_results"].append(result)
                if all(result[key] for key in ("vertex_compile", "fragment_compile", "link")):
                    scene_programs[(variant, debug)] = program
                else:
                    print(json.dumps(result), file=sys.stderr)
        scene_ok = len(scene_programs) == len(VARIANTS)*7
        if scene_ok:
            # Existing light/material equations must survive the scene variant.
            rows = pixel_checks(gl, scene_programs)
            for row in rows:
                row["name"] = "scene_" + row["name"]
            report["pixel_results"].extend(rows)
            gl.glDisable(0x0BE2)
            gl.use(scene_programs[("", 0)], {"s_base": (0, 0, 0, 255),
                   "s_lmap": (0, 0, 0, 0), "s_hemi": (0, 0, 0, 0)})
            program = scene_programs[("", 0)]
            gl.glUniform4f(gl.glGetUniformLocation(program, b"fog_plane"), 1, 0, 0, .6)
            gl.glUniform4f(gl.glGetUniformLocation(program, b"fog_color"), .2, .4, .6, 0)
            actual = gl.pixel()
            expected = [round(v * .4 * 255) for v in (.2, .4, .6)] + [255]
            report["pixel_results"].append(dict(name="scene_fog_interpolate_before_clamp",
                actual_rgba8=actual, expected_rgba8=expected,
                passed=all(abs(a-b) <= 1 for a, b in zip(actual, expected))))
        environment_ok = True
        for wave in (False,True):
            for alpha_test in (False,True):
                vs = "olr_r1_grass" if wave else "olr_r1_grass_still"
                ps = "olr_r1_grass_aref" if alpha_test else "olr_r1_grass"
                program,result=gl.program(shader_source(vs+".vs",0,True),shader_source(ps+".ps",0,True))
                result.update(variant=vs+"/"+ps,scene=True)
                report["compile_results"].append(result)
                ok=all(result[key] for key in ("vertex_compile","fragment_compile","link"))
                environment_ok &= ok
                if not ok:
                    print(json.dumps(result),file=sys.stderr)
                    continue
                gl.draw_setup()
                gl.glVertexAttrib4f(8,.5,.5,.5,0)
                gl.use(program,{"s_base":(96,128,160,255)})
                loc=lambda name: gl.glGetUniformLocation(program,name.encode())
                for name,value in {"consts":(1,1,.4,.6),"wave":(0,0,0,0),"dir2D":(0,0,0,0),
                        "array[0]":(1,0,0,0),"array[1]":(0,1,0,0),
                        "array[2]":(0,0,1,0),"array[3]":(.12,.23,.34,1)}.items():
                    gl.glUniform4f(loc(name),*value)
                actual=gl.pixel()
                expected=[round(2*b*light*(.8 if wave else 1))
                          for b,light in zip((96,128,160),(.12,.23,.34))]+[255]
                report["pixel_results"].append(dict(name=f"scene_grass_wave_{wave}_alpha_test_{alpha_test}",
                    actual_rgba8=actual,expected_rgba8=expected,
                    passed=all(abs(a-b)<=1 for a,b in zip(actual,expected))))
        material_programs = {}
        for stem in ["olr_r1_tree"+v for v in ("", "_detail", "_rigid", "_rigid_detail",
                     "_point", "_spot", "_rigid_point", "_rigid_spot")] + ["olr_r1_lod", "olr_r1_lod_depth"]:
            for debug in range(7):
                program, result = gl.program(shader_source(stem+".vs", debug, True),
                                             shader_source(stem+".ps", debug, True))
                result.update(variant=stem, scene=True, debug=debug)
                report["compile_results"].append(result)
                ok = all(result[key] for key in ("vertex_compile", "fragment_compile", "link"))
                environment_ok &= ok
                if not ok:
                    print(json.dumps(result), file=sys.stderr)
                elif debug == 0:
                    material_programs[stem] = program
        if len(material_programs) == 10:
            gl.draw_setup()
            program = material_programs["olr_r1_tree"]
            gl.use(program, {"s_base": (96,128,160,255)})
            loc = lambda name: gl.glGetUniformLocation(program, name.encode())
            transform = (C.c_float*12)(1,0,0,0,1,0,0,0,1,0,0,0)
            gl.glUniformMatrix4x3fv(loc("m_xform"),1,0,transform)
            for name,value in {"consts": (1/1024,1/1024,0,0), "wave": (0,0,0,0),
                    "wind": (0,0,0,0), "c_scale": (.1,.2,.3,.4), "c_bias": (.01,.02,.03,.04)}.items():
                gl.glUniform4f(loc(name),*value)
            glUniform2f = function(gl.gl,"glUniform2f",None,C.c_int,C.c_float,C.c_float)
            glUniform2f(loc("c_sun"),.5,.1)
            tree_light = [(i*.7+j) + sky*.75*(.4*.7+.04) + sun*(.7*.5+.1)
                          for i,j,sky,sun in zip((.1,.2,.3),(.01,.02,.03),(.1,.15,.2),(.2,.15,.1))]
            expected = [round(2*b*light) for b,light in zip((96,128,160),tree_light)] + [255]
            actual = gl.pixel()
            report["pixel_results"].append(dict(name="scene_tree_original_decompression_wrapped_hemi_flyer_sun",
                actual_rgba8=actual,expected_rgba8=expected,
                passed=all(abs(a-b)<=1 for a,b in zip(actual,expected))))
            # Preserve the target when the source's 200/255 alpha test discards.
            gl.use(program,{"s_base":(96,128,160,200)})
            gl.glClearColor(17/255,29/255,43/255,61/255)
            gl.glClear(0x4000)
            actual=gl.pixel(clear=False)
            report["pixel_results"].append(dict(name="scene_tree_alpha_200_discards",actual_rgba8=actual,
                expected_rgba8=[17,29,43,61],passed=actual==[17,29,43,61]))
            gl.glVertexAttrib4f(0,.9,.4,.2,0)
            gl.glVertexAttrib4f(5,0,0,1,0)
            gl.glVertexAttrib4f(6,0,0,1,0)
            gl.glVertexAttrib4f(10,.12,.10,.08,.3)
            gl.glVertexAttrib4f(11,.12,.10,.08,.3)
            lod_light=[v*1.55 + sky*.3*1.55 + amb + sun*.2*1.55*(1-.7/math.sqrt(2))
                       for v,sky,amb,sun in zip((.08,.10,.12),(.1,.15,.2),(.03,.04,.05),(.2,.15,.1))]
            for stem,alpha in (("olr_r1_lod",64),("olr_r1_lod_depth",255),("olr_r1_lod_depth",64)):
                gl.use(material_programs[stem],{"s_base":(96,128,160,alpha),"s_hemi":(0,0,0,230)})
                gl.glClearColor(17/255,29/255,43/255,61/255)
                gl.glClear(0x4000)
                actual=gl.pixel(clear=False)
                expected=([17,29,43,61] if stem.endswith("depth") and alpha==64 else
                    [round(2*b*light*(.5+.5*230/255)) for b,light in zip((96,128,160),lod_light)]+[round(alpha*.9)])
                report["pixel_results"].append(dict(name=f"scene_{stem}_alpha_{alpha}",actual_rgba8=actual,
                    expected_rgba8=expected,passed=all(abs(a-b)<=1 for a,b in zip(actual,expected))))
        for stem in ("sky2", "clouds"):
            for vtf in (False, True):
                _, result = gl.program(shader_source(stem + ".vs", 0, True, vtf),
                                       shader_source(stem + ".ps", 0, True, vtf))
                result.update(variant=stem, scene=True, vtf=vtf)
                report["compile_results"].append(result)
                environment_ok &= all(result[key] for key in ("vertex_compile", "fragment_compile", "link"))
                if not environment_ok:
                    print(json.dumps(result), file=sys.stderr)
        for stem, varyings, expected, textures in (
            ("sky2", "layout(location=0) out vec4 c; layout(location=8) out vec3 t0; "
             "layout(location=9) out vec3 t1; void main(){gl_Position=p; "
             "c=vec4(.2,.3,.4,.25); t0=t1=vec3(0,0,1);}",
             [2 * f * ((1-.25)*a + .25*b) / 255 for f,a,b in
              zip((.2,.3,.4),(64,96,128),(128,160,192))] + [1],
             {"s_sky0": (64,96,128,255), "s_sky1": (128,160,192,255)}),
            ("clouds", "layout(location=0) out vec4 c; layout(location=8) out vec2 t0; "
             "layout(location=9) out vec2 t1; void main(){gl_Position=p; "
             "c=vec4(.2,.3,.4,.5); t0=t1=vec2(.5);}",
             [f*(a+b)/255 for f,a,b in zip((.2,.3,.4,.5),
                (64,96,128,32),(128,160,192,64))],
             {"s_clouds0": (64,96,128,32), "s_clouds1": (128,160,192,64)})):
            vertex = "#version 320 es\nprecision highp float; layout(location=3) in vec4 p; " + varyings
            program, result = gl.program(vertex, shader_source(stem+".ps", 0, True))
            if not all(result[key] for key in ("vertex_compile", "fragment_compile", "link")):
                raise RuntimeError(f"{stem} pixel fixture: {result}")
            gl.use(program, {} if stem == "sky2" else textures)
            if stem == "sky2":
                for unit, (name, pixel) in enumerate(textures.items()):
                    gl.cube(unit, pixel)
                    gl.glUniform1i(gl.glGetUniformLocation(program, name.encode()), unit)
            actual = gl.pixel()
            expected = [round(min(1,x)*255) for x in expected]
            report["pixel_results"].append(dict(name="scene_"+stem+"_original_color_alpha",
                actual_rgba8=actual, expected_rgba8=expected,
                passed=all(abs(a-b) <= 1 for a,b in zip(actual,expected))))
        report["passed"] = legacy_ok and scene_ok and environment_ok and bool(report["pixel_results"]) and all(
            row["passed"] for row in report["pixel_results"])
    except Exception:
        report["error"] = traceback.format_exc()
        report["passed"] = False
        print(report["error"], file=sys.stderr)
    report["source_sha256"] = dict(sorted(SOURCES.items()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": report["passed"], "compiled_pairs": len(report["compile_results"]),
                      "pixels": report["pixel_results"], "output": str(args.output)}, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
