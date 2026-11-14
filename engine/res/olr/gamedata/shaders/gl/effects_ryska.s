local tex_base = "water\\water_water"

function normal(shader)
    shader:begin("water", "olr_ryska")
        :sorting(2, false)
        :blend(true, blend.srcalpha, blend.srcalpha)
        :zb(true, false)
        :distort(true)
        :fog(false)

    shader:sampler("s_base"):texture(tex_base)
    shader:sampler("s_nmap"):texture("water\\water_normal")
    shader:sampler("s_env0"):texture("$user$sky0"):clamp()
    shader:sampler("s_env1"):texture("$user$sky1"):clamp()
end

function l_special(shader)
    shader:begin("waterd", "waterd")
        :sorting(2, true)
        :blend(true, blend.srcalpha, blend.invsrcalpha)
        :zb(true, false)
        :distort(true)
        :fog(false)

    shader:sampler("s_base"):texture(tex_base)
    shader:sampler("s_distort"):texture("water\\water_dudv")
end
