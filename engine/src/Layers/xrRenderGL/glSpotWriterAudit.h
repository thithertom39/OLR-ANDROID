#pragma once

#include <cstring>

// Queries only: no binds, setters, buffer reads or cache invalidation.
namespace xray::render::RENDER_NAMESPACE::GLSpotWriterAudit
{
inline bool Enabled()
{
    static const bool enabled = []
    {
        if (!Core.Params) return false;
        constexpr char flag[] = "-gles_spot_writer_audit";
        constexpr size_t length = sizeof(flag) - 1;
        for (pcstr arg = Core.Params; (arg = std::strstr(arg, flag)) != nullptr; ++arg)
            if ((arg == Core.Params || std::strchr(" \t\r\n", arg[-1])) &&
                (arg[length] == '\0' || std::strchr(" \t\r\n", arg[length])))
                return true;
        return false;
    }();
    return enabled;
}

struct Entry
{
    bool used = false;
    GLuint program = 0;
    string256 name{};
    u32 draws = 0;
};

struct State
{
    bool active = false;
    u32 passes = 0, frame = 0, context = 0, draws = 0;
    const void* light = nullptr;
    string256 name{};
    // Reserve two program slots for each world-matrix category, so an opaque
    // static program cannot consume every sample before transformed models.
    // Identity is a sampling category, not a claim about object ownership.
    Entry entries[2][2]{};
};

inline State& Get()
{
    static thread_local State state;
    return state;
}

inline void Errors(pcstr stage)
{
    const auto& state = Get();
    for (u32 i = 0; i < 8; ++i)
    {
        const GLenum error = glGetError();
        if (error == GL_NO_ERROR) return;
        Msg("! [gles_spot_writer_audit] pass=%u frame=%u stage=%s error=0x%x",
            state.passes, state.frame, stage, u32(error));
    }
    Msg("! [gles_spot_writer_audit] pass=%u stage=%s error_drain_limit=8", state.passes, stage);
}

class Pass
{
    bool m_owned = false;
public:
    Pass(const void* source, u32 context, u32 frame, bool eligible)
    {
        if (!Enabled() || !eligible) return;
        auto& state = Get();
        if (state.active || state.passes >= 8) return;
        ++state.passes;
        state.active = true;
        state.frame = frame;
        state.context = context;
        state.light = source;
        state.draws = 0;
        state.name[0] = '\0';
        for (auto& group : state.entries)
            for (auto& entry : group) entry = Entry{};
        m_owned = true;
        Msg("* [gles_spot_writer_audit] pass=%u stage=begin frame=%u context=%u light=%p",
            state.passes, frame, context, source);
        Errors("preexisting");
    }
    Pass(const Pass&) = delete;
    Pass& operator=(const Pass&) = delete;
    ~Pass()
    {
        if (!m_owned) return;
        Errors("after_graph");
        auto& state = Get();
        Msg("* [gles_spot_writer_audit] pass=%u stage=end sampled_draws=%u", state.passes, state.draws);
        state.active = false;
    }
};

inline void NamePP(u32 context, pcstr name)
{
    if (!Enabled() || !name) return;
    auto& state = Get();
    if (!state.active || state.context != context) return;
    std::strncpy(state.name, name, sizeof(state.name) - 1);
    state.name[sizeof(state.name) - 1] = '\0';
}

inline void Matrix(pcstr name, const Fmatrix& matrix)
{
    const auto& state = Get();
    Msg("* [gles_spot_writer_audit] pass=%u draw=%u matrix=%s finite=%u "
        "values=%g,%g,%g,%g;%g,%g,%g,%g;%g,%g,%g,%g;%g,%g,%g,%g",
        state.passes, state.draws, name, u32(_valid(matrix)),
        matrix._11, matrix._12, matrix._13, matrix._14, matrix._21, matrix._22, matrix._23, matrix._24,
        matrix._31, matrix._32, matrix._33, matrix._34, matrix._41, matrix._42, matrix._43, matrix._44);
}

inline void MatrixGPU(const R_constant& constant, const R_constant_load& load, pcstr stage,
    GLint current_program, const Fmatrix& expected)
{
    const auto& state = Get();
    GLint linked = 0, location = -1, type = 0, size = 0;
    GLuint index = GL_INVALID_INDEX;
    bool valid = constant.type == RC_float && load.cls == RC_4x4 && load.program &&
        load.location != GLuint(-1) && glIsProgram(load.program);
    if (valid) { glGetProgramiv(load.program, GL_LINK_STATUS, &linked); valid = linked == GL_TRUE; }
    if (valid) { location = glGetUniformLocation(load.program, "m_WVP"); valid = location >= 0 && GLuint(location) == load.location; }
    if (valid)
    {
        const GLchar* name = "m_WVP";
        glGetUniformIndices(load.program, 1, &name, &index);
        valid = index != GL_INVALID_INDEX;
        if (valid)
        {
            glGetActiveUniformsiv(load.program, 1, &index, GL_UNIFORM_TYPE, &type);
            glGetActiveUniformsiv(load.program, 1, &index, GL_UNIFORM_SIZE, &size);
            valid = type == GL_FLOAT_MAT4 && size == 1;
        }
    }
    Msg("* [gles_spot_writer_audit] pass=%u draw=%u stage=wvp_mapping load=%s destination=0x%x "
        "type=%u class=%u program=%u linked=%d program_active=%u location=%u actual_location=%d "
        "uniform_type=0x%x uniform_size=%d valid=%u",
        state.passes, state.draws, stage, constant.destination, u32(constant.type), u32(load.cls), load.program,
        linked, u32(load.program == GLuint(current_program)), load.location, location, u32(type), size, u32(valid));
    if (!valid) return;
    GLfloat gpu[16]{};
    glGetUniformfv(load.program, location, gpu);
    bool finite = true;
    float error = 0.f;
    for (u32 i = 0; i < 16; ++i)
    {
        const float cpu = expected.m[i / 4][i % 4];
        if (!_valid(cpu) || !_valid(gpu[i])) finite = false;
        else if (_abs(cpu - gpu[i]) > error) error = _abs(cpu - gpu[i]);
    }
    Msg("* [gles_spot_writer_audit] pass=%u draw=%u stage=wvp_gpu load=%s finite=%u max_error=%g "
        "values=%g,%g,%g,%g;%g,%g,%g,%g;%g,%g,%g,%g;%g,%g,%g,%g",
        state.passes, state.draws, stage, u32(finite), error,
        gpu[0], gpu[1], gpu[2], gpu[3], gpu[4], gpu[5], gpu[6], gpu[7],
        gpu[8], gpu[9], gpu[10], gpu[11], gpu[12], gpu[13], gpu[14], gpu[15]);
}

inline bool BeforeDraw(CBackend& cmd_list, GLuint logical_pp, u32 count, u32 base_vertex, u32 first_index)
{
    if (!Enabled()) return false;
    auto& state = Get();
    if (!state.active || state.context != cmd_list.context_id || state.draws >= 8) return false;
    const auto& world = cmd_list.xforms.m_w;
    bool identity = true;
    for (u32 row = 0; row < 4; ++row)
        for (u32 column = 0; column < 4; ++column)
            identity = identity && world.m[row][column] == (row == column ? 1.f : 0.f);
    // Logical program already bound by Render; selecting an entry requires no
    // GL query on the unsampled draws. Name also distinguishes failed PP=0s.
    Entry* selected = nullptr;
    for (auto& entry : state.entries[identity ? 0 : 1])
    {
        if (entry.used && (entry.program != logical_pp || std::strcmp(entry.name, state.name) != 0)) continue;
        if (!entry.used)
        {
            entry.used = true;
            entry.program = logical_pp;
            std::memcpy(entry.name, state.name, sizeof(entry.name));
        }
        if (entry.draws >= 2) return false;
        selected = &entry;
        break;
    }
    if (!selected) return false;
    ++selected->draws;
    ++state.draws;
    Errors("before_queries");
    GLint program = 0, linked = 0, fbo = 0, vao = 0, element = 0, viewport[4]{};
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &element);
    glGetIntegerv(GL_VIEWPORT, viewport);
    const bool program_valid = program > 0 && glIsProgram(GLuint(program));
    if (program_valid) glGetProgramiv(GLuint(program), GL_LINK_STATUS, &linked);
    Msg("* [gles_spot_writer_audit] pass=%u draw=%u frame=%u context=%u light=%p world_identity=%u "
        "pp_name=%s logical_pp=%u actual_program=%d program_valid=%u linked=%d fbo=%d fbo_status=0x%x "
        "vao=%d ebo=%d viewport=%d,%d,%d,%d count=%u base_vertex=%u first_index=%u",
        state.passes, state.draws, state.frame, state.context, state.light, u32(identity), state.name,
        logical_pp, program, u32(program_valid), linked, fbo, u32(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)),
        vao, element, viewport[0], viewport[1], viewport[2], viewport[3], count, base_vertex, first_index);
    GLint depth_func = 0, cull = 0, front = 0, stencil_func = 0, stencil_ref = 0, stencil_mask = 0;
    GLboolean depth_write = GL_FALSE;
    GLfloat depth_range[2]{}, factor = 0.f, units = 0.f;
    glGetIntegerv(GL_DEPTH_FUNC, &depth_func); glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
    glGetFloatv(GL_DEPTH_RANGE, depth_range); glGetIntegerv(GL_CULL_FACE_MODE, &cull);
    glGetIntegerv(GL_FRONT_FACE, &front); glGetIntegerv(GL_STENCIL_FUNC, &stencil_func);
    glGetIntegerv(GL_STENCIL_REF, &stencil_ref); glGetIntegerv(GL_STENCIL_VALUE_MASK, &stencil_mask);
    glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &factor); glGetFloatv(GL_POLYGON_OFFSET_UNITS, &units);
    const GLint depth_clamp = GLAD_GL_EXT_depth_clamp ? GLint(glIsEnabled(GL_DEPTH_CLAMP_EXT)) : -1;
    const GLint sample_mask = GLAD_GL_ES_VERSION_3_1 ? GLint(glIsEnabled(GL_SAMPLE_MASK)) : -1;
    Msg("* [gles_spot_writer_audit] pass=%u draw=%u stage=state depth_test=%u depth_write=%u depth_func=0x%x "
        "depth_range=%g,%g stencil_test=%u stencil_func=0x%x stencil_ref=%d stencil_mask=0x%x "
        "cull_enable=%u cull_mode=0x%x front_face=0x%x scissor=%u discard=%u polygon_offset=%u factor=%g units=%g "
        "depth_clamp=%d sample_mask=%d sample_coverage=%u alpha_to_coverage=%u",
        state.passes, state.draws, u32(glIsEnabled(GL_DEPTH_TEST)), u32(depth_write), u32(depth_func),
        depth_range[0], depth_range[1], u32(glIsEnabled(GL_STENCIL_TEST)), u32(stencil_func), stencil_ref,
        u32(stencil_mask), u32(glIsEnabled(GL_CULL_FACE)), u32(cull), u32(front), u32(glIsEnabled(GL_SCISSOR_TEST)),
        u32(glIsEnabled(GL_RASTERIZER_DISCARD)), u32(glIsEnabled(GL_POLYGON_OFFSET_FILL)), factor, units,
        depth_clamp, sample_mask, u32(glIsEnabled(GL_SAMPLE_COVERAGE)), u32(glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE)));
    // POSITION maps to location 3 in glBufferUtils::VertexUsageList and GLSL.
    GLint enabled = 0, buffer = 0, size = 0, type = 0, normalized = 0, stride = 0;
    void* pointer = nullptr;
    glGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
    glGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer);
    glGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
    glGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
    glGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized);
    glGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
    glGetVertexAttribPointerv(3, GL_VERTEX_ATTRIB_ARRAY_POINTER, &pointer);
    Msg("* [gles_spot_writer_audit] pass=%u draw=%u stage=position index=3 enabled=%d buffer=%d size=%d "
        "type=0x%x normalized=%d stride=%d offset=%p", state.passes, state.draws, enabled, buffer,
        size, u32(type), normalized, stride, pointer);
    Matrix("wvp_cpu", cmd_list.xforms.m_wvp);
    if (selected->draws == 1)
    {
        Matrix("world_cpu", cmd_list.xforms.m_w);
        Matrix("view_cpu", cmd_list.xforms.m_v);
        Matrix("project_cpu", cmd_list.xforms.m_p);
    }
    const auto constant = cmd_list.get_c("m_WVP");
    if (constant)
    {
        if (constant->destination & RC_dest_all) MatrixGPU(*constant, constant->pp, "pp", program, cmd_list.xforms.m_wvp);
        if (constant->destination & RC_dest_pixel) MatrixGPU(*constant, constant->ps, "ps", program, cmd_list.xforms.m_wvp);
        if (constant->destination & RC_dest_vertex) MatrixGPU(*constant, constant->vs, "vs", program, cmd_list.xforms.m_wvp);
    }
    else Msg("* [gles_spot_writer_audit] pass=%u draw=%u stage=wvp_mapping found=0", state.passes, state.draws);
    Errors("queries");
    return true;
}

inline void AfterDraw(bool sampled)
{
    if (sampled) Errors("after_draw");
}
} // namespace xray::render::RENDER_NAMESPACE::GLSpotWriterAudit
