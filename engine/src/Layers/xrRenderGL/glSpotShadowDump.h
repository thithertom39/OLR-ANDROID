#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include "glBackendStateCache.h"

// Diagnostic only. Raw bindings are restored exactly; the draw-selection
// cache is invalidated around temporary FBO operations.
namespace xray::render::RENDER_NAMESPACE::GLSpotShadowDump
{
inline bool Enabled()
{
    static const bool enabled = []
    {
        if (!Core.Params)
            return false;
        constexpr char flag[] = "-gles_spot_shadow_dump";
        constexpr size_t length = sizeof(flag) - 1;
        for (pcstr arg = Core.Params; (arg = std::strstr(arg, flag)) != nullptr; ++arg)
        {
            if ((arg == Core.Params || std::strchr(" \t\r\n", arg[-1])) &&
                (arg[length] == '\0' || std::strchr(" \t\r\n", arg[length])))
                return true;
        }
        return false;
    }();
    return enabled;
}

inline bool CheckErrors(u32 ticket, pcstr stage)
{
    bool clean = true;
    for (u32 i = 0; i < 8; ++i)
    {
        const GLenum error = glGetError();
        if (error == GL_NO_ERROR)
            return clean;
        clean = false;
        Msg("! [gles_spot_shadow_dump] ticket=%u stage=%s error=0x%x", ticket, stage, u32(error));
    }
    Msg("! [gles_spot_shadow_dump] ticket=%u stage=%s error_drain_limit=8", ticket, stage);
    return false;
}

struct Result
{
    bool captured = false;
    bool selftest_ok = false;
    bool restore_ok = false;
    bool cleanup_ok = false;
};

class Scope
{
    static constexpr std::array<GLenum, 7> Enables = {
        GL_DEPTH_TEST, GL_STENCIL_TEST, GL_CULL_FACE,
        GL_DITHER, GL_RASTERIZER_DISCARD, GL_SAMPLE_ALPHA_TO_COVERAGE, GL_SAMPLE_COVERAGE
    };
    static constexpr std::array<GLenum, 4> PackNames = {
        GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_ROWS, GL_PACK_SKIP_PIXELS
    };
    u32 m_ticket;
    Result& m_result;
    bool m_saved = false;
    GLint m_draw_fbo = 0, m_read_fbo = 0, m_program = 0, m_vao = 0;
    GLint m_active = 0, m_texture = 0, m_sampler = 0, m_pack_buffer = 0;
    GLint m_unpack_buffer = 0, m_element_buffer = 0, m_read_buffer = 0;
    xr_vector<GLint> m_draw_buffers;
    std::array<GLfloat, 4> m_viewport{};
    std::array<GLint, 4> m_pack{};
    std::array<GLboolean, 4> m_color{};
    std::array<GLboolean, Enables.size()> m_enables{};
    GLboolean m_blend = GL_FALSE, m_sample_mask = GL_FALSE, m_scissor = GL_FALSE;
    bool m_has_sample_mask = false;
    PFNGLCOLORMASKIPROC m_color_mask_i = nullptr;
    PFNGLENABLEIPROC m_enable_i = nullptr;
    PFNGLDISABLEIPROC m_disable_i = nullptr;
    PFNGLISENABLEDIPROC m_is_enabled_i = nullptr;
    PFNGLVIEWPORTINDEXEDFPROC m_viewport_i = nullptr;
    PFNGLGETFLOATI_VPROC m_get_float_i = nullptr;
    PFNGLENABLEIPROC m_scissor_enable_i = nullptr;
    PFNGLDISABLEIPROC m_scissor_disable_i = nullptr;
    PFNGLISENABLEDIPROC m_scissor_is_enabled_i = nullptr;

    void GetViewport(GLfloat* viewport) const
    {
        if (m_get_float_i)
            m_get_float_i(GL_VIEWPORT, 0, viewport);
        else
        {
            GLint integer_viewport[4]{};
            glGetIntegerv(GL_VIEWPORT, integer_viewport);
            for (u32 i = 0; i < 4; ++i)
                viewport[i] = GLfloat(integer_viewport[i]);
        }
    }

    void SetViewport(const GLfloat* viewport) const
    {
        if (m_viewport_i)
            m_viewport_i(0, viewport[0], viewport[1], viewport[2], viewport[3]);
        else
            glViewport(GLint(viewport[0]), GLint(viewport[1]), GLsizei(viewport[2]), GLsizei(viewport[3]));
    }

    void SetScissor(bool enabled) const
    {
        if (m_scissor_enable_i)
        {
            if (enabled)
                m_scissor_enable_i(GL_SCISSOR_TEST, 0);
            else
                m_scissor_disable_i(GL_SCISSOR_TEST, 0);
        }
        else if (enabled)
            glEnable(GL_SCISSOR_TEST);
        else
            glDisable(GL_SCISSOR_TEST);
    }

    void SetBlend(bool enabled) const
    {
        if (m_enable_i)
        {
            if (enabled)
                m_enable_i(GL_BLEND, 0);
            else
                m_disable_i(GL_BLEND, 0);
        }
        else if (enabled)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);
    }

    void SetColor(const GLboolean* mask) const
    {
        if (m_color_mask_i)
            m_color_mask_i(0, mask[0], mask[1], mask[2], mask[3]);
        else
            glColorMask(mask[0], mask[1], mask[2], mask[3]);
    }

    bool Verify() const
    {
        bool same = true;
        const auto check_int = [&](GLenum name, GLint expected)
        {
            GLint actual = 0;
            glGetIntegerv(name, &actual);
            if (actual != expected)
            {
                same = false;
                Msg("! [gles_spot_shadow_dump] ticket=%u stage=restore_mismatch enum=0x%x expected=%d actual=%d",
                    m_ticket, u32(name), expected, actual);
            }
        };
        check_int(GL_DRAW_FRAMEBUFFER_BINDING, m_draw_fbo);
        check_int(GL_READ_FRAMEBUFFER_BINDING, m_read_fbo);
        check_int(GL_CURRENT_PROGRAM, m_program);
        check_int(GL_VERTEX_ARRAY_BINDING, m_vao);
        check_int(GL_ACTIVE_TEXTURE, m_active);
        check_int(GL_TEXTURE_BINDING_2D, m_texture);
        check_int(GL_SAMPLER_BINDING, m_sampler);
        check_int(GL_PIXEL_PACK_BUFFER_BINDING, m_pack_buffer);
        check_int(GL_PIXEL_UNPACK_BUFFER_BINDING, m_unpack_buffer);
        check_int(GL_ELEMENT_ARRAY_BUFFER_BINDING, m_element_buffer);
        check_int(GL_READ_BUFFER, m_read_buffer);
        for (u32 i = 0; i < m_draw_buffers.size(); ++i)
            check_int(GL_DRAW_BUFFER0 + i, m_draw_buffers[i]);
        std::array<GLfloat, 4> viewport{};
        std::array<GLboolean, 4> color{};
        GetViewport(viewport.data());
        // Ordinary queries return draw-buffer index 0, including EXT/OES
        // indexed-draw-buffer implementations on ES 3.0.
        glGetBooleanv(GL_COLOR_WRITEMASK, color.data());
        if (viewport != m_viewport || color != m_color)
            Msg("! [gles_spot_shadow_dump] ticket=%u stage=restore_mismatch viewport_same=%u color_mask_same=%u",
                m_ticket, u32(viewport == m_viewport), u32(color == m_color));
        same = same && viewport == m_viewport && color == m_color;
        for (u32 i = 0; i < PackNames.size(); ++i)
            check_int(PackNames[i], m_pack[i]);
        for (u32 i = 0; i < Enables.size(); ++i)
        {
            const bool matches = glIsEnabled(Enables[i]) == m_enables[i];
            if (!matches)
                Msg("! [gles_spot_shadow_dump] ticket=%u stage=restore_mismatch enable=0x%x", m_ticket, u32(Enables[i]));
            same = matches && same;
        }
        const GLboolean blend = m_is_enabled_i ? m_is_enabled_i(GL_BLEND, 0) : glIsEnabled(GL_BLEND);
        same = (blend == m_blend) && same;
        const GLboolean scissor = m_scissor_is_enabled_i ?
            m_scissor_is_enabled_i(GL_SCISSOR_TEST, 0) : glIsEnabled(GL_SCISSOR_TEST);
        same = (scissor == m_scissor) && same;
        if (m_has_sample_mask)
            same = (glIsEnabled(GL_SAMPLE_MASK) == m_sample_mask) && same;
        return CheckErrors(m_ticket, "restore_queries") && same;
    }

public:
    GLuint program = 0, vertex_shader = 0, fragment_shader = 0;
    GLuint vao = 0, fbo = 0, color_texture = 0, sampler = 0;

    Scope(u32 ticket, Result& result) : m_ticket(ticket), m_result(result)
    {
        // Only index 0 is touched when indexed masks/blending exist. Other MRT
        // masks and enables must survive even if they differ from index 0.
        if (GLAD_GL_ES_VERSION_3_2)
        {
            m_color_mask_i = glColorMaski;
            m_enable_i = glEnablei;
            m_disable_i = glDisablei;
            m_is_enabled_i = glIsEnabledi;
        }
        else if (GLAD_GL_OES_draw_buffers_indexed)
        {
            m_color_mask_i = glColorMaskiOES;
            m_enable_i = glEnableiOES;
            m_disable_i = glDisableiOES;
            m_is_enabled_i = glIsEnablediOES;
        }
        else if (GLAD_GL_EXT_draw_buffers_indexed)
        {
            m_color_mask_i = glColorMaskiEXT;
            m_enable_i = glEnableiEXT;
            m_disable_i = glDisableiEXT;
            m_is_enabled_i = glIsEnablediEXT;
        }
        if ((GLAD_GL_ES_VERSION_3_2 || GLAD_GL_OES_draw_buffers_indexed || GLAD_GL_EXT_draw_buffers_indexed) &&
            (!m_color_mask_i || !m_enable_i || !m_disable_i || !m_is_enabled_i))
        {
            Msg("! [gles_spot_shadow_dump] ticket=%u stage=state_capture indexed_functions_missing=1", ticket);
            return;
        }
        // Viewport-array extensions make the non-indexed viewport/scissor
        // setters overwrite every slot. Touch only slot 0 and preserve its
        // floating-point viewport; all other slots remain unchanged.
        if (GLAD_GL_OES_viewport_array)
        {
            m_viewport_i = glViewportIndexedfOES;
            m_get_float_i = glGetFloati_vOES;
            m_scissor_enable_i = glEnableiOES;
            m_scissor_disable_i = glDisableiOES;
            m_scissor_is_enabled_i = glIsEnablediOES;
        }
        else if (GLAD_GL_NV_viewport_array)
        {
            m_viewport_i = glViewportIndexedfNV;
            m_get_float_i = glGetFloati_vNV;
            m_scissor_enable_i = glEnableiNV;
            m_scissor_disable_i = glDisableiNV;
            m_scissor_is_enabled_i = glIsEnablediNV;
        }
        if ((GLAD_GL_OES_viewport_array || GLAD_GL_NV_viewport_array) &&
            (!m_viewport_i || !m_get_float_i || !m_scissor_enable_i || !m_scissor_disable_i || !m_scissor_is_enabled_i))
        {
            Msg("! [gles_spot_shadow_dump] ticket=%u stage=state_capture viewport_array_functions_missing=1", ticket);
            return;
        }
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_draw_fbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_read_fbo);
        glGetIntegerv(GL_CURRENT_PROGRAM, &m_program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &m_vao);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &m_active);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &m_texture);
        glGetIntegerv(GL_SAMPLER_BINDING, &m_sampler);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &m_pack_buffer);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &m_unpack_buffer);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &m_element_buffer);
        glGetIntegerv(GL_READ_BUFFER, &m_read_buffer);
        GLint max_draw_buffers = 0;
        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &max_draw_buffers);
        if (max_draw_buffers < 1 || max_draw_buffers > 64)
        {
            Msg("! [gles_spot_shadow_dump] ticket=%u stage=state_capture max_draw_buffers=%d", ticket, max_draw_buffers);
            CheckErrors(ticket, "state_capture");
            return;
        }
        m_draw_buffers.resize(size_t(max_draw_buffers));
        for (u32 i = 0; i < m_draw_buffers.size(); ++i)
            glGetIntegerv(GL_DRAW_BUFFER0 + i, &m_draw_buffers[i]);
        GetViewport(m_viewport.data());
        glGetBooleanv(GL_COLOR_WRITEMASK, m_color.data());
        for (u32 i = 0; i < PackNames.size(); ++i)
            glGetIntegerv(PackNames[i], &m_pack[i]);
        for (u32 i = 0; i < Enables.size(); ++i)
            m_enables[i] = glIsEnabled(Enables[i]);
        m_blend = m_is_enabled_i ? m_is_enabled_i(GL_BLEND, 0) : glIsEnabled(GL_BLEND);
        m_scissor = m_scissor_is_enabled_i ? m_scissor_is_enabled_i(GL_SCISSOR_TEST, 0) : glIsEnabled(GL_SCISSOR_TEST);
        m_has_sample_mask = GLAD_GL_ES_VERSION_3_1 != 0;
        if (m_has_sample_mask)
            m_sample_mask = glIsEnabled(GL_SAMPLE_MASK);
        m_saved = CheckErrors(ticket, "state_capture");
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    bool valid() const { return m_saved; }
    GLuint unit() const { return GLuint(m_active - GL_TEXTURE0); }

    void Viewport(u32 width, u32 height) const
    {
        const GLfloat viewport[4] = { 0.f, 0.f, GLfloat(width), GLfloat(height) };
        SetViewport(viewport);
    }

    void Prepare(u32 size)
    {
        for (const auto enable : Enables)
            glDisable(enable);
        SetBlend(false);
        SetScissor(false);
        if (m_has_sample_mask)
            glDisable(GL_SAMPLE_MASK);
        constexpr GLboolean all[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
        SetColor(all);
        Viewport(size, size);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei(GL_PACK_SKIP_ROWS, 0);
        glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    }

    ~Scope()
    {
        if (m_saved)
        {
            GLHardwareState::InvalidateDrawBuffers();
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLuint(m_draw_fbo));
            glBindFramebuffer(GL_READ_FRAMEBUFFER, GLuint(m_read_fbo));
            glUseProgram(GLuint(m_program));
            glBindVertexArray(GLuint(m_vao));
            glActiveTexture(GLenum(m_active));
            glBindTexture(GL_TEXTURE_2D, GLuint(m_texture));
            glBindSampler(unit(), GLuint(m_sampler));
            SetViewport(m_viewport.data());
            SetColor(m_color.data());
            SetBlend(m_blend == GL_TRUE);
            SetScissor(m_scissor == GL_TRUE);
            for (u32 i = 0; i < Enables.size(); ++i)
            {
                if (m_enables[i])
                    glEnable(Enables[i]);
                else
                    glDisable(Enables[i]);
            }
            if (m_has_sample_mask)
            {
                if (m_sample_mask)
                    glEnable(GL_SAMPLE_MASK);
                else
                    glDisable(GL_SAMPLE_MASK);
            }
            glBindBuffer(GL_PIXEL_PACK_BUFFER, GLuint(m_pack_buffer));
            for (u32 i = 0; i < PackNames.size(); ++i)
                glPixelStorei(PackNames[i], m_pack[i]);
            const bool commands_ok = CheckErrors(m_ticket, "restore_commands");
            m_result.restore_ok = Verify() && commands_ok;
        }
        // Restore before deletion; none of these private names can affect the
        // restored engine FBO/VAO/texture/sampler/program bindings.
        if (fbo)
        {
            GLHardwareState::InvalidateDrawBuffers();
            glDeleteFramebuffers(1, &fbo);
        }
        if (vao) glDeleteVertexArrays(1, &vao);
        if (color_texture) glDeleteTextures(1, &color_texture);
        if (sampler) glDeleteSamplers(1, &sampler);
        if (program) glDeleteProgram(program);
        if (vertex_shader) glDeleteShader(vertex_shader);
        if (fragment_shader) glDeleteShader(fragment_shader);
        m_result.cleanup_ok = CheckErrors(m_ticket, "cleanup");
        Msg("* [gles_spot_shadow_dump] ticket=%u stage=restore captured=%u selftest_ok=%u restore_ok=%u cleanup_ok=%u",
            m_ticket, u32(m_result.captured), u32(m_result.selftest_ok), u32(m_result.restore_ok),
            u32(m_result.cleanup_ok));
    }
};

inline bool Compile(u32 ticket, GLuint shader, pcstr source, pcstr name)
{
    if (!shader)
        return false;
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE)
    {
        GLchar info[2048]{};
        glGetShaderInfoLog(shader, GLsizei(sizeof(info) - 1), nullptr, info);
        Msg("! [gles_spot_shadow_dump] ticket=%u stage=compile shader=%s info=%s", ticket, name, info);
    }
    const bool errors_ok = CheckErrors(ticket, name);
    return compiled == GL_TRUE && errors_ok;
}

inline u32 Bits(const u8* pixel)
{
    return u32(pixel[0]) | (u32(pixel[1]) << 8) | (u32(pixel[2]) << 16) | (u32(pixel[3]) << 24);
}

inline bool ReadDepth(u32 ticket, GLuint depth_texture, u32 pos_x, u32 pos_y, u32 size,
    xr_vector<u8>& pixels, Result& result)
{
    Scope scope(ticket, result);
    if (!scope.valid())
        return false;
    constexpr pcstr vertex_source = R"glsl(#version 300 es
precision highp float;
precision highp int;
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)glsl";
    constexpr pcstr fragment_source = R"glsl(#version 300 es
precision highp float;
precision highp int;
uniform highp sampler2D u_depth;
uniform ivec2 u_origin;
uniform int u_selftest;
layout(location=0) out vec4 o_color;
void main() {
    int x = int(gl_FragCoord.x);
    float d = u_selftest != 0 ? (x == 0 ? 0.0 : (x == 1 ? 0.5 : 1.0)) :
        texelFetch(u_depth, u_origin + ivec2(gl_FragCoord.xy), 0).r;
    uint bits = floatBitsToUint(d);
    uvec4 bytes = uvec4(bits & 255u, (bits >> 8) & 255u, (bits >> 16) & 255u, bits >> 24);
    o_color = vec4(bytes) / 255.0;
}
)glsl";
    scope.vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    scope.fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    if (!CheckErrors(ticket, "create_shaders") ||
        !Compile(ticket, scope.vertex_shader, vertex_source, "vertex_shader") ||
        !Compile(ticket, scope.fragment_shader, fragment_source, "fragment_shader"))
        return false;
    scope.program = glCreateProgram();
    if (!CheckErrors(ticket, "create_program") || !scope.program)
        return false;
    glAttachShader(scope.program, scope.vertex_shader);
    glAttachShader(scope.program, scope.fragment_shader);
    glLinkProgram(scope.program);
    GLint linked = 0;
    glGetProgramiv(scope.program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE)
    {
        GLchar info[2048]{};
        glGetProgramInfoLog(scope.program, GLsizei(sizeof(info) - 1), nullptr, info);
        Msg("! [gles_spot_shadow_dump] ticket=%u stage=link info=%s", ticket, info);
    }
    if (!CheckErrors(ticket, "link") || linked != GL_TRUE)
        return false;
    glGenVertexArrays(1, &scope.vao);
    glGenFramebuffers(1, &scope.fbo);
    glGenTextures(1, &scope.color_texture);
    glGenSamplers(1, &scope.sampler);
    if (!CheckErrors(ticket, "create_objects") || !scope.vao || !scope.fbo || !scope.color_texture || !scope.sampler)
        return false;
    glBindTexture(GL_TEXTURE_2D, scope.color_texture);
    // Storage allocation has no client pointer: UNPACK parameters/buffer never
    // change and cannot reinterpret nullptr as a PBO offset.
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, GLsizei(size), GLsizei(size));
    GLHardwareState::InvalidateDrawBuffers();
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, scope.fbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scope.color_texture, 0);
    const GLenum color_attachment = GL_COLOR_ATTACHMENT0;
    GLHardwareState::InvalidateDrawBuffers();
    glDrawBuffers(1, &color_attachment);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, scope.fbo);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    const GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    if (!CheckErrors(ticket, "output_fbo") || status != GL_FRAMEBUFFER_COMPLETE)
    {
        Msg("! [gles_spot_shadow_dump] ticket=%u stage=output_fbo status=0x%x", ticket, u32(status));
        return false;
    }
    glSamplerParameteri(scope.sampler, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glSamplerParameteri(scope.sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(scope.sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(scope.sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(scope.sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, depth_texture);
    glBindSampler(scope.unit(), scope.sampler);
    glUseProgram(scope.program);
    glBindVertexArray(scope.vao);
    const GLint depth_location = glGetUniformLocation(scope.program, "u_depth");
    const GLint origin_location = glGetUniformLocation(scope.program, "u_origin");
    const GLint selftest_location = glGetUniformLocation(scope.program, "u_selftest");
    if (!CheckErrors(ticket, "setup_sampling") || depth_location < 0 || origin_location < 0 || selftest_location < 0)
    {
        Msg("! [gles_spot_shadow_dump] ticket=%u stage=uniforms depth=%d origin=%d selftest=%d",
            ticket, depth_location, origin_location, selftest_location);
        return false;
    }
    glUniform1i(depth_location, GLint(scope.unit()));
    glUniform2i(origin_location, GLint(pos_x), GLint(pos_y));
    scope.Prepare(size);
    // Check the exact byte path on this GPU, including framebuffer conversion
    // and readback, before trusting the depth capture.
    glUniform1i(selftest_location, 1);
    scope.Viewport(3, 1);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    std::array<u8, 12> test{};
    glReadPixels(0, 0, 3, 1, GL_RGBA, GL_UNSIGNED_BYTE, test.data());
    result.selftest_ok = Bits(test.data()) == 0x00000000u &&
        Bits(test.data() + 4) == 0x3f000000u && Bits(test.data() + 8) == 0x3f800000u;
    Msg("* [gles_spot_shadow_dump] ticket=%u stage=selftest ok=%u zero=0x%08x half=0x%08x one=0x%08x",
        ticket, u32(result.selftest_ok), Bits(test.data()), Bits(test.data() + 4), Bits(test.data() + 8));
    if (!CheckErrors(ticket, "selftest") || !result.selftest_ok)
        return false;
    glUniform1i(selftest_location, 0);
    scope.Viewport(size, size);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glReadPixels(0, 0, GLsizei(size), GLsizei(size), GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    result.captured = CheckErrors(ticket, "depth_readback");
    return result.captured;
}

inline bool WriteFile(pcstr alias, pcstr name, const void* data, size_t size)
{
    IWriter* writer = FS.w_open(alias, name);
    if (!writer || !writer->valid())
    {
        if (writer)
            FS.w_close(writer);
        Msg("! [gles_spot_shadow_dump] stage=save file=%s/%s valid=0", alias, name);
        return false;
    }
    writer->w(data, size);
    FS.w_close(writer);
    return true;
}

inline void Capture(u32 ticket, u32 frame, u32 context, GLuint depth_texture,
    u32 pos_x, u32 pos_y, u32 size, u32 smapsize)
{
    static u32 attempts = 0;
    if (!Enabled() || !ticket || attempts >= 2)
        return;
    // Cap temporary GPU/CPU allocations; bounds use subtraction to avoid wrap.
    if (!depth_texture || size < 3 || size > 2048 || pos_x > smapsize || pos_y > smapsize ||
        size > smapsize - pos_x || size > smapsize - pos_y)
        return;
    ++attempts;
    Msg("* [gles_spot_shadow_dump] ticket=%u stage=begin attempt=%u frame=%u context=%u depth=%u atlas=%u,%u,%u smapsize=%u",
        ticket, attempts, frame, context, depth_texture, pos_x, pos_y, size, smapsize);
    // Do not blame earlier engine errors on the diagnostic. Abort this attempt
    // if any were observed; ordinary rendering continues without state changes.
    if (!CheckErrors(ticket, "preexisting"))
        return;
    const bool source_valid = GLAD_GL_ES_VERSION_3_0 && glIsTexture(depth_texture);
    if (!CheckErrors(ticket, "validate_source") || !source_valid)
    {
        Msg("! [gles_spot_shadow_dump] ticket=%u stage=validate_source valid=0", ticket);
        return;
    }
    static_assert(sizeof(float) == sizeof(u32) && std::numeric_limits<float>::is_iec559,
        "Depth dump requires IEEE-754 binary32");
    xr_vector<u8> pixels(size_t(size) * size * 4);
    Result result;
    const bool read_ok = ReadDepth(ticket, depth_texture, pos_x, pos_y, size, pixels, result);
    // The Scope destructor has already restored, verified and deleted every GL
    // object before CPU processing or filesystem I/O starts.
    pcstr alias = FS.get_path("$logs$", nullptr) ? "$logs$" :
        (FS.get_path("$app_data_root$", nullptr) ? "$app_data_root$" : nullptr);
    if (!alias)
    {
        Msg("! [gles_spot_shadow_dump] ticket=%u stage=save writable_alias_missing=1", ticket);
        return;
    }
    u32 valid = 0, invalid = 0, nonclear = 0, different_clear = 0;
    float minimum = std::numeric_limits<float>::infinity();
    float maximum = -std::numeric_limits<float>::infinity();
    constexpr float nonclear_threshold = 1.f - 1e-6f;
    if (read_ok)
    {
        for (size_t i = 0; i < pixels.size(); i += 4)
        {
            const u32 bits = Bits(pixels.data() + i);
            float depth = 0.f;
            std::memcpy(&depth, &bits, sizeof(depth));
            if (!std::isfinite(depth) || depth < 0.f || depth > 1.f)
            {
                ++invalid;
                continue;
            }
            ++valid;
            minimum = std::min(minimum, depth);
            maximum = std::max(maximum, depth);
            nonclear += depth < nonclear_threshold;
            different_clear += bits != 0x3f800000u;
        }
    }
    if (!valid)
        minimum = maximum = 0.f;
    bool image_saved = false, preview_saved = false;
    string_path tga_name, pgm_name, meta_name;
    xr_sprintf(tga_name, "spot_depth_f%u_t%u_rgba.tga", frame, ticket);
    xr_sprintf(pgm_name, "spot_depth_f%u_t%u_normalized.pgm", frame, ticket);
    xr_sprintf(meta_name, "spot_depth_f%u_t%u.txt", frame, ticket);
    if (read_ok)
    {
        // TGA stores BGRA, bottom row first. Reversing R/B reconstructs the
        // exact RGBA8 float-bit encoding returned by glReadPixels.
        xr_vector<u8> tga(18 + pixels.size(), 0);
        tga[2] = 2;
        tga[12] = u8(size); tga[13] = u8(size >> 8);
        tga[14] = u8(size); tga[15] = u8(size >> 8);
        tga[16] = 32; tga[17] = 8;
        for (size_t i = 0; i < pixels.size(); i += 4)
        {
            tga[18 + i] = pixels[i + 2]; tga[19 + i] = pixels[i + 1];
            tga[20 + i] = pixels[i]; tga[21 + i] = pixels[i + 3];
        }
        image_saved = WriteFile(alias, tga_name, tga.data(), tga.size());
        string128 pgm_header;
        xr_sprintf(pgm_header, "P5\n%u %u\n255\n", size, size);
        const size_t header_size = std::strlen(pgm_header);
        xr_vector<u8> pgm(header_size + size_t(size) * size);
        std::memcpy(pgm.data(), pgm_header, header_size);
        for (u32 y = 0; y < size; ++y)
        {
            for (u32 x = 0; x < size; ++x)
            {
                const u32 bits = Bits(pixels.data() + (size_t(size - 1 - y) * size + x) * 4);
                float depth = 0.f;
                std::memcpy(&depth, &bits, sizeof(depth));
                float preview = maximum > minimum ? (depth - minimum) / (maximum - minimum) : depth;
                if (!std::isfinite(preview) || depth < 0.f || depth > 1.f)
                    preview = 0.f;
                preview = std::max(0.f, std::min(1.f, preview));
                pgm[header_size + size_t(y) * size + x] = u8(std::round(preview * 255.f));
            }
        }
        preview_saved = WriteFile(alias, pgm_name, pgm.data(), pgm.size());
    }
    string4096 metadata;
    xr_sprintf(metadata,
        "ticket=%u\nframe=%u\ncontext=%u\nattempt=%u\ndepth_texture=%u\natlas=%u,%u,%u\nsmapsize=%u\n"
        "read_ok=%u\nselftest_ok=%u\nrestore_ok=%u\ncleanup_ok=%u\nimage_saved=%u\npreview_saved=%u\n"
        "encoding=floatBitsToUint(depth); RGBA=(bits[7:0],bits[15:8],bits[23:16],bits[31:24])\n"
        "tga=BGRA32_bottom_left; swap B/R to recover RGBA; alpha is data, never composite\n"
        "clear_depth=1.0; clear_bits=0x3f800000\nnonclear_threshold=%.9g\n"
        "pixels=%u\nvalid=%u\ninvalid=%u\nmin_depth=%.9g\nmax_depth=%.9g\nnonclear=%u\ndifferent_clear=%u\n"
        "preview=PGM_top_left; normalized (depth-min)/(max-min), invalid pixels black\n"
        "image=%s\npreview_file=%s\n",
        ticket, frame, context, attempts, depth_texture, pos_x, pos_y, size, smapsize,
        u32(read_ok), u32(result.selftest_ok), u32(result.restore_ok), u32(result.cleanup_ok),
        u32(image_saved), u32(preview_saved), nonclear_threshold, size * size, valid, invalid,
        minimum, maximum, nonclear, different_clear, tga_name, pgm_name);
    const bool metadata_saved = WriteFile(alias, meta_name, metadata, std::strlen(metadata));
    Msg("* [gles_spot_shadow_dump] ticket=%u stage=result read_ok=%u selftest_ok=%u restore_ok=%u cleanup_ok=%u "
        "valid=%u invalid=%u min=%g max=%g nonclear=%u different_clear=%u threshold=%g "
        "image_saved=%u preview_saved=%u metadata_saved=%u path=%s/%s",
        ticket, u32(read_ok), u32(result.selftest_ok), u32(result.restore_ok), u32(result.cleanup_ok),
        valid, invalid, minimum, maximum, nonclear, different_clear, nonclear_threshold,
        u32(image_saved), u32(preview_saved), u32(metadata_saved), alias, meta_name);
}
} // namespace xray::render::RENDER_NAMESPACE::GLSpotShadowDump
