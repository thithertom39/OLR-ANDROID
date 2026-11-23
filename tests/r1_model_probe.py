"""Original OLR model/projector shader fixtures, called by r1_gles_probe.

These are actual GLES draws, not a mock rendering implementation. The software
driver does not establish device/scene parity or completion of model shadows.
"""
import ctypes as C

def model_projector_checks(gl, report, shader_source):
    programs = {}
    for family in ("hq", "lq", "point", "spot"):
        for aref in ("", "_aref"):
            stem = "olr_r1_model_" + family + aref
            for skin in ("NONE", "0", "1", "2", "3", "4"):
                vertex = shader_source(stem+".vs", 0, True).replace(
                    "#define OLR_R1_NATIVE", "#define OLR_R1_NATIVE\n#define SKIN_"+skin)
                program, result = gl.program(vertex, shader_source(stem+".ps", 0, True))
                report["compile_results"].append(dict(variant=stem, skin=skin, **result))
                if not all(result[key] for key in ("vertex_compile", "fragment_compile", "link")):
                    raise RuntimeError(f"{stem} SKIN_{skin}: {result}")
                programs[(family+aref, skin)] = program
    surfaces = {}
    for family in ("lmap", "vertex", "implicit"):
        stem = "olr_r1_projector_"+family
        program, result = gl.program(shader_source(stem+".vs",0,True), shader_source(stem+".ps",0,True))
        report["compile_results"].append(dict(variant=stem, **result))
        if not all(result[key] for key in ("vertex_compile", "fragment_compile", "link")):
            raise RuntimeError(f"{stem}: {result}")
        surfaces[family] = program

    def record(name, actual, expected):
        expected = [round(max(0,min(1,x))*255) for x in expected]
        report["pixel_results"].append(dict(name=name, actual_rgba8=actual, expected_rgba8=expected,
            passed=all(abs(a-b)<=1 for a,b in zip(actual,expected))))

    identity = (C.c_float*16)(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1)
    world = (C.c_float*12)(1,0,0, 0,1,0, 0,0,1, 0,0,0)
    base = [96/255,128/255,160/255,1]
    atlas = [51/255,77/255,102/255,128/255]
    sun = [.2,.15,.1]
    lq = [.07,.10,.12]
    gl.glVertexAttrib4f(5,0,0,1,0)  # SKIN_NONE is already unpacked FLOAT3.
    gl.glVertexAttrib4f(8,.5,.5,0,3)
    for family in ("lq", "hq"):
        for factor in (0,.25,1):
            p = programs[(family,"NONE")]
            gl.use(p, {"s_base":(96,128,160,255),"s_lmap":(51,77,102,128)})
            loc = lambda name: gl.glGetUniformLocation(p,name.encode())
            gl.glUniformMatrix4x3fv(loc("m_W"),1,0,world)
            gl.glUniformMatrix4fv(loc("olr_r1_model_projector"),1,0,identity)
            gl.glUniform4f(loc("olr_r1_model_props"),.1,.2,.3,.4)
            gl.glUniform4f(loc("olr_r1_model_clamp[0]"),-.5,-.5,-.5,factor)
            gl.glUniform4f(loc("olr_r1_model_clamp[1]"),.5,.5,.5,0)
            light = lq if family=="lq" else [(atlas[i]+sun[i]*atlas[3])*(1-factor)+lq[i]*factor for i in range(3)]
            record(f"model_{family}_original_projected_mix_{factor}",gl.pixel(),
                   [base[i]*light[i]*2 for i in range(3)]+[1])
    # Authored GREATER alpha and fog.
    for family in ("lq_aref","hq_aref"):
        p=programs[(family,"NONE")]
        for alpha in (32,33):
            gl.use(p,{"s_base":(96,128,160,alpha),"s_lmap":(51,77,102,128)})
            loc=lambda name: gl.glGetUniformLocation(p,name.encode())
            gl.glUniformMatrix4x3fv(loc("m_W"),1,0,world)
            gl.glUniformMatrix4fv(loc("olr_r1_model_projector"),1,0,identity)
            gl.glUniform4f(loc("olr_r1_model_props"),.1,.2,.3,.4)
            gl.glUniform4f(loc("olr_r1_model_clamp[0]"),-1,-1,-1,1)
            gl.glUniform4f(loc("olr_r1_model_clamp[1]"),1,1,1,0)
            gl.glUniform4f(loc("olr_r1_alpha_ref"),32,0,0,0)
            gl.glUniform4f(loc("fog_plane"),0,0,0,.5)
            gl.glUniform4f(loc("fog_color"),.2,.4,.6,0)
            expected = [0,0,0,0] if alpha==32 else [(.2,.4,.6)[i]*.5+base[i]*lq[i] for i in range(3)]+[alpha/255]
            record(f"model_{family}_fog_alpha_{alpha}",gl.pixel(),expected)

    # Lighting-only surfaces: diffuse RGB/detail/sun/fog do not enter atlas.
    textures={"s_base":(64,128,192,102),"s_lmap":(26,51,77,153),"s_hemi":(51,102,153,255)}
    hemi=[.098,.147,.196]
    for family,p in surfaces.items():
        gl.use(p,textures)
        gl.glVertexAttrib4f(5,1,.5,.5,.7)
        gl.glVertexAttrib4f(8,512,512,0,1)
        gl.glVertexAttrib4f(9,16384,16384,0,1)
        gl.glVertexAttrib4f(0,.12,.10,.08,.3)
        loc=lambda name: gl.glGetUniformLocation(p,name.encode())
        gl.glUniform4f(loc("olr_r1_projector_raster"),0,0,0,0)
        if family=="vertex":
            expected=[(.08,.10,.12)[i]+hemi[i]*.7+(.03,.04,.05)[i] for i in range(3)]+[.3]
        else:
            mask=102/255 if family=="implicit" else .4
            divisor=1 if family=="implicit" else 1.5
            expected=[textures["s_lmap"][i]/255+hemi[i]*mask+(.03,.04,.05)[i]/divisor for i in range(3)]+[.6]
        record("projector_"+family+"_lighting_only_rgba8",gl.pixel(),expected)

    # Raster/UV round trip in a nonzero atlas tile. Two colors at opposite Y
    # positions catch accidental D3D/GL row inversion and half-texel mistakes.
    gl.glActiveTexture(0x84C0)
    atlas_tex=gl.texture(None,512,512)
    gl.glFramebufferTexture2D(0x8D40,0x8CE0,0x0DE1,atlas_tex,0)
    gl.glViewport(0,0,512,512)
    gl.glClearColor(0,0,0,0); gl.glClear(0x4000)
    tile_x,tile_y=102,153
    gl.glViewport(tile_x,tile_y,51,51)
    p=surfaces["vertex"]
    gl.use(p,{})
    loc=lambda name: gl.glGetUniformLocation(p,name.encode())
    gl.glUniform4f(loc("olr_r1_projector_raster"),1/51,1/51,0,0)
    # COLOR varying linearly encodes the original NDC Y. Actual surface VS
    # remains intact; only per-vertex attribute storage supplies the fixture.
    vao,vbo=C.c_uint(),C.c_uint()
    gl.glGenVertexArrays(1,C.byref(vao)); gl.glBindVertexArray(vao.value)
    gl.glGenBuffers(1,C.byref(vbo)); gl.glBindBuffer(0x8892,vbo.value)
    data=(C.c_float*24)(-1,-1,0,1, 0,0,.2,1,
                         3,-1,0,1, 0,0,.2,1,
                        -1, 3,0,1, 0,0,1.0,1)
    gl.glBufferData(0x8892,C.sizeof(data),data,0x88E4)
    gl.glVertexAttribPointer(3,4,0x1406,0,32,None);gl.glEnableVertexAttribArray(3)
    gl.glVertexAttribPointer(0,4,0x1406,0,32,C.c_void_p(16));gl.glEnableVertexAttribArray(0)
    gl.glVertexAttrib4f(5,1,.5,.5,0)
    gl.glUniform4f(loc("L_ambient"),0,0,0,0)
    gl.glUniform4f(loc("L_hemi_color"),0,0,0,0)
    gl.glDrawArrays(4,0,3);gl.glFinish()
    for y,expected_red in ((tile_y+10,.2+.2*(2*(1-10/51))), (tile_y+40,.2+.2*(2*(1-40/51)))):
        pixel=(C.c_ubyte*4)()
        gl.glReadPixels(tile_x+25,y,1,1,0x1908,0x1401,pixel)
        record(f"projector_atlas_d3d_row_{y-tile_y}",list(pixel),[expected_red,0,0,1])
    gl.check("projector atlas draw/read")
    gl.draw_setup()  # Restore standard 1x1 fixture, VAO, and framebuffer.
    # Sample that rendered atlas with the real HQ shader and the original
    # D3D9 UVgen, rather than relying solely on direct framebuffer reads.
    for ndc_y in (.6,-.6):
        p=programs[("hq","NONE")]
        gl.use(p,{"s_base":(128,128,128,255)})
        loc=lambda name: gl.glGetUniformLocation(p,name.encode())
        gl.glActiveTexture(0x84C1);gl.glBindTexture(0x0DE1,atlas_tex)
        gl.glUniform1i(loc("s_lmap"),1)
        gl.glUniformMatrix4x3fv(loc("m_W"),1,0,world)
        slot=51/512; half=.5/512
        uvgen=(C.c_float*16)(.5*slot,0,0,0, 0,-.5*slot,0,0, 0,0,1,0,
                            tile_x/512+half+.5*slot,tile_y/512+half+(.5+half)*slot,0,1)
        gl.glUniformMatrix4fv(loc("olr_r1_model_projector"),1,0,uvgen)
        gl.glUniform4f(loc("olr_r1_model_clamp[0]"),0,ndc_y,0,0)
        gl.glUniform4f(loc("olr_r1_model_clamp[1]"),0,ndc_y,0,0)
        gl.glUniform3f(loc("L_sun_color"),0,0,0)
        gl.glVertexAttrib4f(5,0,0,1,0)
        gl.glVertexAttrib4f(8,.5,.5,0,0)
        record(f"model_hq_samples_rendered_atlas_y_{ndc_y}",gl.pixel(),
               [(.4+.2*ndc_y)*2*128/255,0,0,1])

    # OLR LQ skinning_2lq ignores the second bone even when its weight is 1.
    # Give the second bone an inverted normal; HQ must use it, LQ must not.
    for family in ("lq","hq"):
        p=programs[(family,"2")]
        gl.use(p,{"s_base":(255,255,255,255),"s_lmap":(0,0,0,0)})
        loc=lambda name: gl.glGetUniformLocation(p,name.encode())
        gl.glUniformMatrix4x3fv(loc("m_W"),1,0,world)
        gl.glUniform4f(loc("olr_r1_model_props"),1,1,1,0)
        gl.glUniform4f(loc("L_ambient"),0,0,0,0)
        gl.glUniform4f(loc("olr_r1_model_clamp[0]"),0,0,0,1)
        gl.glUniform4f(loc("olr_r1_model_clamp[1]"),0,0,0,0)
        gl.glUniformMatrix4fv(loc("olr_r1_model_projector"),1,0,identity)
        bones=((1,0,0,0),(0,1,0,0),(0,0,1,0), (1,0,0,0),(0,-1,0,0),(0,0,-1,0))
        for i,bone in enumerate(bones):
            gl.glUniform4f(loc(f"sbones_array[{i}]"),*bone)
        gl.glVertexAttrib4f(5,1,.5,.5,1)  # Packed BGRA +Z, full second-bone weight.
        gl.glVertexAttrib4f(8,.5,.5,0,3)
        record(f"model_{family}_skin2_original_bone_choice",gl.pixel(),
               [.4,.3,.2,1] if family=="lq" else [0,0,0,1])
    gl.draw_setup()
    # Actual model POINT/SPOT programs, including the original alpha product.
    # A distant source makes Gouraud N.L exactly one over this small triangle;
    # constant lookup pixels isolate the authored material/attenuation/fog math.
    for family in ("point","spot","point_aref","spot_aref"):
        for alpha in ((120,128) if "aref" in family else (128,)):
            p=programs[(family,"NONE")]
            gl.use(p,{"s_base":(96,128,160,alpha),"s_lmap":(128,160,192,128),
                      "s_r1_att":(192,128,64,64)})
            loc=lambda name: gl.glGetUniformLocation(p,name.encode())
            gl.glUniformMatrix4x3fv(loc("m_W"),1,0,world)
            gl.glUniform4f(loc("olr_r1_dynamic_pos"),0,0,1e8,5e-9)
            gl.glUniform4f(loc("olr_r1_dynamic_color"),.3,.4,.5,1)
            gl.glUniform4f(loc("fog_plane"),0,0,0,.75)
            gl.glUniform4f(loc("fog_color"),.8,.7,.6,0)
            gl.glUniform4f(loc("olr_r1_alpha_ref"),23,0,0,0)
            gl.glVertexAttrib4f(5,0,0,1,0)
            gl.glVertexAttrib4f(8,.5,.5,0,0)
            expected=[(96,128,160)[i]/255*(128,160,192)[i]/255*(192,128,64)[i]/255*
                      (.3,.4,.5)[i]*.75*(alpha/255)*2 for i in range(3)]
            expected.append(alpha/255*(128/255)*(64/255)*.75*2)
            # Both diffuse alphas exceed reference 23. Only the completed
            # lighting result distinguishes them (23 fails, 24 passes).
            if "aref" in family and alpha==120: expected=[0,0,0,0]
            record(f"model_{family}_original_rgba_alpha_{alpha}",gl.pixel(),expected)
            if gl.glGetUniformLocation(p,b"s_position")!=-1:
                raise RuntimeError("Model local shader still requires a G-buffer sampler")
        # Opaque original passes also test final alpha > 0. RGB-positive lookup
        # textures with A=0 must not contribute just because diffuse A is 255.
        if "aref" not in family:
            gl.glActiveTexture(0x84C1);gl.texture((128,160,192,0))
            record(f"model_{family}_opaque_final_alpha_zero_discards",gl.pixel(),[0,0,0,0])
    # Original LightShadows silhouette, including full SKIN_2 (unlike LQ),
    # and four-tap avg4. No diffuse alpha test or R2 texture is allowed here.
    caster_programs={}
    for skin in ("NONE","0","1","2","3","4"):
        vertex=shader_source("olr_r1_model_shadow.vs",0,True).replace(
            "#define OLR_R1_NATIVE","#define OLR_R1_NATIVE\n#define SKIN_"+skin)
        program,result=gl.program(vertex,shader_source("olr_r1_model_shadow.ps",0,True))
        report["compile_results"].append(dict(variant="olr_r1_model_shadow",skin=skin,**result))
        if not all(result[key] for key in ("vertex_compile","fragment_compile","link")):
            raise RuntimeError(f"R1 shadow caster SKIN_{skin}: {result}")
        caster_programs[skin]=program
    blur,result=gl.program(shader_source("olr_r1_shadow_blur.vs",0,True),shader_source("olr_r1_shadow_blur.ps",0,True))
    report["compile_results"].append(dict(variant="olr_r1_shadow_blur",**result))
    if not all(result[key] for key in ("vertex_compile","fragment_compile","link")):
        raise RuntimeError(f"R1 shadow blur: {result}")
    gl.draw_setup()
    gl.use(caster_programs["NONE"],{})
    if gl.glGetUniformLocation(caster_programs["NONE"],b"s_base")!=-1:
        raise RuntimeError("Original shadow silhouette must not alpha-test diffuse texture")
    gl.glClearColor(.8,.6,.4,1);gl.glClear(0x4000)
    gl.glEnable(0x0BE2);gl.glBlendFunc(0,0x0300) # ZERO, SRC_COLOR
    record("shadow_silhouette_zero_source_color_multiply",gl.pixel(clear=False),[0,0,0,0])
    gl.glDisable(0x0BE2)
    gl.use(blur,{"s_base":(64,128,192,255)})
    gl.glVertexAttrib4f(8,.5,.5,0,1)
    record("shadow_avg4_preserves_constant",gl.pixel(),[64/255,128/255,192/255,1])
    texels=bytearray([255])*(512*512*4)
    texels[(256*512+256)*4:(256*512+256)*4+4]=bytes(4)
    gl.glActiveTexture(0x84C0);gl.texture(texels,512,512)
    gl.glVertexAttrib4f(8,256.5/512,256.5/512,0,1)
    record("shadow_avg4_original_075_texel_kernel",gl.pixel(),[.9375]*4)
    world,result=gl.program(shader_source("olr_r1_shadow_world.vs",0,True),shader_source("olr_r1_shadow_world.ps",0,True))
    report["compile_results"].append(dict(variant="olr_r1_shadow_world",**result))
    if not all(result[key] for key in ("vertex_compile","fragment_compile","link")):
        raise RuntimeError(f"R1 projected shadow: {result}")
    gl.draw_setup()
    gl.glVertexAttrib4f(0,.2,.2,.2,.2);gl.glVertexAttrib4f(8,.5,.5,0,1)
    for texel in ((64,128,192,255),(0,0,0,0),(255,255,255,255)):
        gl.use(world,{"s_base":texel})
        gl.glClearColor(.8,.6,.4,1);gl.glClear(0x4000)
        gl.glEnable(0x0BE2);gl.glBlendFunc(0x0306,0) # original DEST_COLOR,ZERO
        expected=[(.8,.6,.4,1)[i]*min(1,texel[i]/255+.2) for i in range(4)]
        record(f"shadow_world_fixed_function_ADD_multiply_{texel[0]}",gl.pixel(clear=False),expected)
        gl.glDisable(0x0BE2)
    gl.draw_setup()


if __name__=="__main__":
    import json
    import sys
    import traceback
    from pathlib import Path
    from r1_gles_probe import GLES, SOURCES, shader_source
    report={"compile_results":[],"pixel_results":[]}
    try:
        gl=GLES();gl.draw_setup()
        report["environment"]=gl.info
        model_projector_checks(gl,report,shader_source)
        report["passed"]=all(row["passed"] for row in report["pixel_results"])
    except Exception:
        report["passed"]=False;report["error"]=traceback.format_exc()
    report["source_sha256"]=dict(sorted(SOURCES.items()))
    output=Path(sys.argv[1])
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(dict(passed=report["passed"],pairs=len(report["compile_results"]),
                         pixels=len(report["pixel_results"]),error=report.get("error"))))
    sys.exit(0 if report["passed"] else 1)
