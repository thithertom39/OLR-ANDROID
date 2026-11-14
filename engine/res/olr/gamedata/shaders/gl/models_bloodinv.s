function normal(shader, t_base, t_second, t_detail)
    shader:begin("dumb", "dumb")
        :sorting(2, false)
        :zb(false, false)
        :color_write_enable(false, false, false, false)
end
