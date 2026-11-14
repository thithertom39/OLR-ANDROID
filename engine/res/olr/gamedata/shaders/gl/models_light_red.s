function normal(shader, t_base, t_second, t_detail)
    shader:begin("model_def_lq", "olr_emissive_red")
        :sorting(2, false)
        :zb(true, true)
        :fog(false)
    shader:sampler("s_base"):texture("fx\\fx_lavahell1")
end
