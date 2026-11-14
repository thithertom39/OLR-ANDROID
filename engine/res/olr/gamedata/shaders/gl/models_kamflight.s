function normal(shader, t_base, t_second, t_detail)
    models_selflightl.normal(shader, t_base, t_second, t_detail)
end

function l_special(shader, t_base, t_second, t_detail)
    shader:begin("shadow_direct_model", "olr_emissive_kamaz")
        :zb(true, false)
        :fog(false)
        :emissive(true)
end
