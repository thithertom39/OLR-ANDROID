#!/usr/bin/env python3
"""Real Mesa ES pixel equivalence: pointer vs ES3.1 format/binding API.

Exercises all 17 engine attribute encodings, buffer/VAO switches, nonzero
vertex offsets, stride changes and indexed draws. No performance inference.
"""
import ctypes as C
import ctypes.util
import json
import os
from pathlib import Path
import struct
import sys

os.environ.setdefault('EGL_PLATFORM', 'surfaceless')
os.environ.setdefault('LIBGL_ALWAYS_SOFTWARE', '1')
egl = C.CDLL(ctypes.util.find_library('EGL'))
gl = C.CDLL(ctypes.util.find_library('GLESv2'))
I, U, P, B = C.c_int, C.c_uint, C.c_void_p, C.c_ubyte
def fn(lib, name, result, *args):
    f = getattr(lib, name); f.restype = result; f.argtypes = args
    return f
display = fn(egl, 'eglGetDisplay', P, P)(None)
major, minor = I(), I()
assert fn(egl, 'eglInitialize', U, P, P, P)(display, C.byref(major), C.byref(minor))
assert fn(egl, 'eglBindAPI', U, U)(0x30a0)
attrs = (I * 13)(0x3033,1,0x3040,0x40,0x3024,8,0x3023,8,0x3022,8,0x3021,8,0x3038)
config, count = P(), I()
assert fn(egl, 'eglChooseConfig', U, P, P, P, I, P)(display, attrs, C.byref(config), 1, C.byref(count)) and count.value == 1
context = fn(egl, 'eglCreateContext', P, P, P, P, P)(display, config, None, (I*5)(0x3098,3,0x30fb,2,0x3038))
surface = fn(egl, 'eglCreatePbufferSurface', P, P, P, P)(display, config, (I*5)(0x3057,16,0x3056,16,0x3038))
assert fn(egl, 'eglMakeCurrent', U, P, P, P, P)(display, surface, surface, context)
get_error = fn(gl, 'glGetError', U)
create_shader = fn(gl, 'glCreateShader', U, U)
source = fn(gl, 'glShaderSource', None, U, I, P, P)
compile_shader = fn(gl, 'glCompileShader', None, U)
shader_iv = fn(gl, 'glGetShaderiv', None, U, U, P)
shaders = []
for kind, text in [(0x8b31, '#version 310 es\nlayout(location=3) in vec3 p; layout(location=0) in vec4 c; out highp vec4 v; void main(){gl_Position=vec4(p,1);v=.2+.6*fract(abs(c));}'),
                   (0x8b30, '#version 310 es\nprecision highp float; in vec4 v; out vec4 o; void main(){o=vec4(v.rgb,1);}')]:
    s = create_shader(kind); raw = C.c_char_p(text.encode()); source(s,1,C.byref(raw),None); compile_shader(s)
    ok=I(); shader_iv(s,0x8b81,C.byref(ok)); assert ok.value, 'shader compile'; shaders.append(s)
program = fn(gl,'glCreateProgram',U)()
for s in shaders: fn(gl,'glAttachShader',None,U,U)(program,s)
fn(gl,'glLinkProgram',None,U)(program)
ok=I(); fn(gl,'glGetProgramiv',None,U,U,P)(program,0x8b82,C.byref(ok)); assert ok.value,'program link'
fn(gl,'glUseProgram',None,U)(program)
gen_vao = fn(gl,'glGenVertexArrays',None,I,P); bind_vao = fn(gl,'glBindVertexArray',None,U)
gen_buffer = fn(gl,'glGenBuffers',None,I,P); bind_buffer=fn(gl,'glBindBuffer',None,U,U)
buffer_data=fn(gl,'glBufferData',None,U,C.c_ssize_t,P,U)
pointer=fn(gl,'glVertexAttribPointer',None,U,I,U,B,I,P)
enable=fn(gl,'glEnableVertexAttribArray',None,U)
format_attr=fn(gl,'glVertexAttribFormat',None,U,I,U,B,U)
binding=fn(gl,'glVertexAttribBinding',None,U,U)
bind_vertex=fn(gl,'glBindVertexBuffer',None,U,U,C.c_ssize_t,I)
draw=fn(gl,'glDrawElements',None,U,I,U,P)
read=fn(gl,'glReadPixels',None,I,I,I,I,U,U,P)
fn(gl,'glViewport',None,I,I,I,I)(0,0,16,16)
clear_color=fn(gl,'glClearColor',None,C.c_float,C.c_float,C.c_float,C.c_float)
clear=fn(gl,'glClear',None,U)
# Mirrors glBufferUtils.cpp size/type/normalized table, including D3DCOLOR.
formats=[(n,0x1406,False,struct.pack('<'+'f'*n,*([.25,.6,.85,.7][:n]))) for n in (1,2,3,4)]
formats += [(4,0x1401,n,bytes([37,113,207,255])) for n in (True,False)]
formats += [(n,0x1402,False,struct.pack('<'+'h'*n,*([123,1742,2941,30767][:n]))) for n in (2,4)]
formats += [(4,0x1401,True,bytes([41,133,201,255]))]
formats += [(n,0x1402,True,struct.pack('<'+'h'*n,*([4123,13742,22941,30767][:n]))) for n in (2,4)]
formats += [(n,0x1403,True,struct.pack('<'+'H'*n,*([9123,23742,42941,60767][:n]))) for n in (2,4)]
formats += [(4,t,n,struct.pack('<I',0x955aa155)) for t,n in [(0x8368,False),(0x8d9f,True)]]
formats += [(n,0x140b,False,struct.pack('<'+'e'*n,*([.25,.6,.85,.7][:n]))) for n in (2,4)]
results=[]
for number,(size,typ,norm,attribute) in enumerate(formats):
    # Two VAOs per format, alternate them during buffer and base-offset changes.
    vaos=(U*2)();gen_vao(2,vaos)
    buffers=(U*2)();gen_buffer(2,buffers)
    indices=U();gen_buffer(1,C.byref(indices))
    pixels={}
    for separated in (False,True):
        for vi in (0,1):
            bind_vao(vaos[vi]);bind_buffer(0x8893,indices)
            raw=C.create_string_buffer(struct.pack('<3H',0,1,2));buffer_data(0x8893,6,raw,0x88e4)
            for location,n,t,normalized,offset in [(3,3,0x1406,False,0),(0,size,typ,norm,12)]:
                enable(location)
                if separated:format_attr(location,n,t,normalized,offset);binding(location,0)
        for step,(vi,base) in enumerate([(0,0),(1,7),(0,7),(1,0),(0,0)]):
            stride=32+16*vi; data=bytearray(stride*(base+3))
            for v,p in enumerate([(-1.,-1.,0.),(3.,-1.,0.),(-1.,3.,0.)]):
                offset=stride*(base+v); data[offset:offset+12]=struct.pack('<3f',*p);data[offset+12:offset+12+len(attribute)]=attribute
            bind_vao(vaos[vi]);bind_buffer(0x8892,buffers[vi]);raw=C.create_string_buffer(bytes(data));buffer_data(0x8892,len(data),raw,0x88e4)
            if separated:bind_vertex(0,buffers[vi],base*stride,stride)
            else:
                pointer(3,3,0x1406,False,stride,P(base*stride))
                pointer(0,size,typ,norm,stride,P(base*stride+12))
            clear_color(0,0,0,0);clear(0x4000);draw(4,3,0x1403,None)
            rgba=(B*(16*16*4))();read(0,0,16,16,0x1908,0x1401,rgba)
            assert get_error()==0,(number,separated,step,'GL error')
            assert rgba[3]==255 and any(rgba[:3]),'empty draw'
            if separated:assert pixels[step]==bytes(rgba),(number,step,'pixel mismatch')
            else:pixels[step]=bytes(rgba)
    results.append(dict(encoding=number,comparisons=5,equal=True))
report=dict(renderer=fn(gl,'glGetString',C.c_char_p,U)(0x1f01).decode(),version=fn(gl,'glGetString',C.c_char_p,U)(0x1f02).decode(),comparisons=85,passed=True,formats=results)
if len(sys.argv)>1:Path(sys.argv[1]).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
