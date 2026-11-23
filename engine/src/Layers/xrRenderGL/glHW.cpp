// glHW.cpp: implementation of the OpenGL specialisation of CHW.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#pragma hdrstop

#include "glHW.h"
#include "glTextureBindings.h"
#include "glVertexBinding.h"
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
#include "glGpuProfiler.h"
#endif
#include "xrEngine/XR_IOConsole.h"

#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
#include <atomic>
#endif

#if defined(XR_PLATFORM_ANDROID)
#include <dlfcn.h>
#endif

namespace xray::render::RENDER_NAMESPACE
{
CHW HW;

#ifdef XR_PLATFORM_ANDROID
namespace
{
struct PresentGamma
{
    bool enabled{};
    bool attempted{};
    Fvector3 curve{};
    Fvector3 balance{};
    GLuint program{};
    GLuint sampler{};
    GLint curveLocation{-1};
    GLint balanceLocation{-1};

    static GLuint Compile(GLenum type, const char* source)
    {
        const GLuint shader = glCreateShader(type);
        if (!shader)
            return 0;
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint status = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status == GL_TRUE)
            return shader;
        char log[512]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        Msg("! Android gamma shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0;
    }

    void Initialize()
    {
        attempted = true;
        constexpr char vertex[] = R"glsl(#version 300 es
out vec2 uv;
void main()
{
    uv = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
)glsl";
        constexpr char fragment[] = R"glsl(#version 300 es
precision highp float;
uniform sampler2D frameColor;
uniform vec3 curve;
uniform vec3 balance;
in vec2 uv;
out vec4 color;
void main()
{
    vec4 frame = texture(frameColor, uv);
    color = vec4(clamp((curve.y * pow(frame.rgb, vec3(curve.x)) + curve.z) * balance, 0.0, 1.0), frame.a);
}
)glsl";
        const GLuint vs = Compile(GL_VERTEX_SHADER, vertex);
        const GLuint fs = Compile(GL_FRAGMENT_SHADER, fragment);
        if (vs && fs)
            program = glCreateProgram();
        if (program)
        {
            glAttachShader(program, vs);
            glAttachShader(program, fs);
            glLinkProgram(program);
            GLint status = GL_FALSE;
            glGetProgramiv(program, GL_LINK_STATUS, &status);
            if (status == GL_TRUE)
            {
                curveLocation = glGetUniformLocation(program, "curve");
                balanceLocation = glGetUniformLocation(program, "balance");
                glGenSamplers(1, &sampler);
                if (sampler)
                {
                    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    Msg("* Android display gamma: shader correction available");
                }
            }
            else
            {
                char log[512]{};
                glGetProgramInfoLog(program, sizeof(log), nullptr, log);
                Msg("! Android gamma shader link failed: %s", log);
            }
            if (!sampler)
            {
                glDeleteProgram(program);
                program = 0;
            }
        }
        if (vs)
            glDeleteShader(vs);
        if (fs)
            glDeleteShader(fs);
    }

    void Release(bool contextCurrent)
    {
        if (contextCurrent)
        {
            if (program)
                glDeleteProgram(program);
            if (sampler)
                glDeleteSamplers(1, &sampler);
        }
        program = sampler = 0;
        attempted = false;
    }

    bool Draw(int width, int height)
    {
        if (!enabled)
            return false;
        if (!attempted)
            Initialize();
        if (!program)
            return false;

        // Sample the same texture attachment as the ordinary present blit.
        GLint attachment = GL_NONE, type = GL_NONE, texture = 0;
        glGetIntegerv(GL_READ_BUFFER, &attachment);
        if (attachment == GL_NONE)
            return false;
        glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, GLenum(attachment),
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
        if (type != GL_TEXTURE)
            return false;
        glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, GLenum(attachment),
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &texture);
        if (!texture)
            return false;

        GLint previousProgram, activeUnit, previousTexture, previousSampler, viewport[4];
        GLboolean colorMask[4];
        glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeUnit);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        glGetIntegerv(GL_SAMPLER_BINDING, &previousSampler);

        constexpr GLenum caps[] = {GL_BLEND, GL_CULL_FACE, GL_DEPTH_TEST, GL_STENCIL_TEST,
            GL_SCISSOR_TEST, GL_RASTERIZER_DISCARD, GL_SAMPLE_ALPHA_TO_COVERAGE, GL_SAMPLE_COVERAGE};
        GLboolean previousCaps[sizeof(caps) / sizeof(caps[0])];
        for (size_t i = 0; i < sizeof(caps) / sizeof(caps[0]); ++i)
        {
            previousCaps[i] = glIsEnabled(caps[i]);
            if (previousCaps[i])
                glDisable(caps[i]);
        }

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glViewport(0, 0, width, height);
        glBindTexture(GL_TEXTURE_2D, GLuint(texture));
        glBindSampler(0, sampler);
        glUseProgram(program);
        // The sampler uniform defaults to unit zero at link time.
        glUniform3f(curveLocation, curve.x, curve.y, curve.z);
        glUniform3f(balanceLocation, balance.x, balance.y, balance.z);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glUseProgram(GLuint(previousProgram));
        glBindSampler(0, GLuint(previousSampler));
        glBindTexture(GL_TEXTURE_2D, GLuint(previousTexture));
        glActiveTexture(GLenum(activeUnit));
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
        for (size_t i = 0; i < sizeof(caps) / sizeof(caps[0]); ++i)
            if (previousCaps[i])
                glEnable(caps[i]);
        return true;
    }
};

PresentGamma presentGamma;
}

void SetAndroidPresentGamma(float gamma, float brightness, float contrast, const Fcolor& balance)
{
    // Match the display LUT curve used by CGammaControl on other platforms.
    presentGamma.curve.set(1.f / (gamma + EPS), (contrast + 1.f) * 0.5f,
        (brightness - contrast) * (16384.f / 65535.f));
    presentGamma.balance.set(balance.r, balance.g, balance.b);
    presentGamma.enabled = gamma != 1.f || brightness != 1.f || contrast != 1.f ||
        balance.r != 1.f || balance.g != 1.f || balance.b != 1.f;
}
#endif

#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
namespace
{
bool AngleAuditEnabled()
{
    if (!Core.Params)
        return false;
    constexpr char flag[] = "-gles_angle_audit";
    constexpr size_t length = sizeof(flag) - 1;
    for (pcstr arg = Core.Params; (arg = strstr(arg, flag)) != nullptr; ++arg)
        if ((arg == Core.Params || strchr(" \t\r\n", arg[-1])) &&
            (arg[length] == '\0' || strchr(" \t\r\n", arg[length])))
            return true;
    return false;
}

// Opt-in Release diagnostics. Never call GL here: callbacks may be delivered
// inside a GL entry point. Limit logging even if the same error repeats per draw.
void CALLBACK OnAngleAuditDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar* message, const void* /*userParam*/)
{
    if (type != GL_DEBUG_TYPE_ERROR && type != GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR &&
        severity != GL_DEBUG_SEVERITY_HIGH && severity != GL_DEBUG_SEVERITY_MEDIUM)
        return;
    const bool critical = type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR ||
        severity == GL_DEBUG_SEVERITY_HIGH;
    static std::atomic<u32> errors{0}, warnings{0};
    const u32 limit = critical ? 64 : 16;
    const u32 index = (critical ? errors : warnings).fetch_add(1, std::memory_order_relaxed);
    if (index < limit)
        Msg("! [gles_angle_audit] debug source=0x%x type=0x%x id=%u severity=0x%x message=%.*s",
            u32(source), u32(type), id, u32(severity),
            message && length > 0 ? (length < 1024 ? int(length) : 1024) : 0,
            message ? message : "");
    else if (index == limit)
        Msg("! [gles_angle_audit] debug %s message limit reached (%u); further messages in this category suppressed",
            critical ? "error" : "warning", limit);
}
static_assert(std::is_same_v<decltype(&OnAngleAuditDebugCallback), GLDEBUGPROC>);
} // namespace
#endif

#if defined(XR_PLATFORM_ANDROID)
namespace
{
template <typename T>
void LogGLFunctionProvider(pcstr name, T function)
{
    Dl_info info{};
    const bool found = function && dladdr(reinterpret_cast<const void*>(function), &info) != 0;
    Msg("* [RenderBackend] %s provider: %s", name,
        found && info.dli_fname ? info.dli_fname : "unavailable");
}

void VerifyAndroidRenderBackend(pcstr renderer)
{
    const auto requested = SDL_getenv("OPENXRAY_RENDER_BACKEND_REQUESTED");
    const auto effective = SDL_getenv("OPENXRAY_RENDER_BACKEND_EFFECTIVE");
    const auto eglDriver = SDL_getenv("SDL_VIDEO_EGL_DRIVER");
    const auto glDriver = SDL_getenv("SDL_VIDEO_GL_DRIVER");
    const auto anglePlatform = SDL_getenv("ANGLE_DEFAULT_PLATFORM");
    Msg("* [RenderBackend] requested launch selection: %s", requested ? requested : "unspecified");
    Msg("* [RenderBackend] effective launch selection: %s", effective ? effective : "unspecified");
    Msg("* [RenderBackend] SDL EGL driver: %s", eglDriver ? eglDriver : "system default");
    Msg("* [RenderBackend] SDL GLES driver: %s", glDriver ? glDriver : "system default");
    Msg("* [RenderBackend] ANGLE platform request: %s", anglePlatform ? anglePlatform : "unset");
    Msg("* [RenderBackend] engine and ImGui dispatch: shared GLAD table via SDL_GL_GetProcAddress");
    LogGLFunctionProvider("glGetString", glad_glGetString);
    LogGLFunctionProvider("glDrawElements", glad_glDrawElements);

    // Library presence or an environment variable cannot prove the selected
    // implementation actually created this context. Check the live renderer.
    if (effective && (strcmp(effective, "angle_vulkan") == 0 || strcmp(effective, "angle_gles") == 0))
    {
        const bool isAngle = renderer && strstr(renderer, "ANGLE") != nullptr;
        const bool isVulkan = renderer && strstr(renderer, "Vulkan") != nullptr;
        CHECK_OR_EXIT(isAngle,
            "ANGLE was selected, but the active OpenGL renderer is not ANGLE. Select Native OpenGL ES in the launcher and report the RenderBackend log.");
        CHECK_OR_EXIT(strcmp(effective, "angle_vulkan") != 0 || isVulkan,
            "ANGLE Vulkan was selected, but the active ANGLE renderer does not report Vulkan. Select Native OpenGL ES in the launcher and report the RenderBackend log.");
        CHECK_OR_EXIT(strcmp(effective, "angle_gles") != 0 || !isVulkan,
            "ANGLE OpenGL ES was selected, but the active ANGLE renderer reports Vulkan. Select Native OpenGL ES in the launcher and report the RenderBackend log.");
        Msg("* [RenderBackend] verified live context: %s", effective);
    }
}
} // namespace
#endif

void CALLBACK OnDebugCallback(GLenum /*source*/, GLenum /*type*/, GLuint id, GLenum severity, GLsizei /*length*/,
    const GLchar* message, const void* /*userParam*/)
{
    static size_t spamCount = 0;
    if (severity == GL_DEBUG_SEVERITY_HIGH || severity == GL_DEBUG_SEVERITY_MEDIUM)
    {
        const std::string_view msg = message ? std::string_view(message) : std::string_view();
        if (msg.find("EsxBufferObject") != std::string_view::npos ||
            msg.find("Namespace collision") != std::string_view::npos ||
            msg.find("Unable to delete shader object") != std::string_view::npos ||
            msg.find("unable to delete shader object") != std::string_view::npos)
        {
            if (++spamCount <= 5)
                Log("~ [spam] ", std::string(msg).c_str());
            return;
        }
        Log(message, id);
    }
}

static_assert(std::is_same_v<decltype(&OnDebugCallback), GLDEBUGPROC>);

// GLES 3.0+ exposes the extension list through glGetStringi; older contexts and
// desktop GL only have the single GL_EXTENSIONS string.
bool CHW::HasExtension(cpcstr name)
{
    if (!name)
        return false;

    GLint num_extensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
    if (num_extensions > 0 && glGetStringi != nullptr)
    {
        for (GLint i = 0; i < num_extensions; ++i)
        {
            const auto ext = reinterpret_cast<cpcstr>(glGetStringi(GL_EXTENSIONS, i));
            if (ext && xr_strcmp(ext, name) == 0)
                return true;
        }
        return false;
    }

    const auto extensions = reinterpret_cast<cpcstr>(glGetString(GL_EXTENSIONS));
    return extensions && strstr(extensions, name) != nullptr;
}

void UpdateVSync()
{
#if defined(XR_PLATFORM_ANDROID)
    static const bool unlockFps = []
    {
        for (pcstr arg = Core.Params; (arg = strstr(arg, "-unlock_fps")) != nullptr; ++arg)
        {
            if ((arg == Core.Params || strchr(" \t\r\n", arg[-1]) != nullptr) &&
                strchr(" \t\r\n", arg[11]) != nullptr)
                return true;
        }
        return false;
    }();
    if (unlockFps)
    {
        if (SDL_GL_SetSwapInterval(0) != 0)
            Msg("! FPS unlock: swap interval 0 rejected: %s", SDL_GetError());
        return;
    }
#endif
    if (psDeviceFlags.test(rsVSync))
    {
        // Try adaptive vsync first
        if (SDL_GL_SetSwapInterval(-1) == -1)
            SDL_GL_SetSwapInterval(1);
    }
    else
    {
        SDL_GL_SetSwapInterval(0);
    }
}

CHW::CHW()
{
    if (!ThisInstanceIsGlobal())
        return;

    Device.seqAppActivate.Add(this);
    Device.seqAppDeactivate.Add(this);
}

CHW::~CHW()
{
    if (!ThisInstanceIsGlobal())
        return;

    Device.seqAppActivate.Remove(this);
    Device.seqAppDeactivate.Remove(this);
}

void CHW::OnAppActivate()
{
    if (m_window)
    {
        SDL_RestoreWindow(m_window);
    }
}

void CHW::OnAppDeactivate()
{
    if (m_window)
    {
        if (psDeviceMode.WindowStyle == rsFullscreen || psDeviceMode.WindowStyle == rsFullscreenBorderless)
            SDL_MinimizeWindow(m_window);
    }
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
void CHW::CreateDevice(SDL_Window* hWnd)
{
    ZoneScoped;
    GLHardwareState::Invalidate();
    GLTextureBindings::Invalidate();

    m_window = hWnd;

    R_ASSERT(m_window);

    // Choose the closest pixel format
    SDL_DisplayMode mode;
    SDL_GetWindowDisplayMode(m_window, &mode);
    mode.format = SDL_PIXELFORMAT_RGBA8888;
    // Apply the pixel format to the device context
    SDL_SetWindowDisplayMode(m_window, &mode);

    Caps.fTarget = D3DFMT_A8R8G8B8;
    Caps.fDepth = D3DFMT_D24S8;

    // Create the context
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
    // Some drivers (e.g. Mesa Panfrost on Mali-G31) only support OpenGL ES 3.1
    // even though the hardware advertises 3.2. Try the highest version first
    // and fall back to older ones.
    m_context = nullptr;
    constexpr int glesVersions[][2] = { {3, 2}, {3, 1}, {3, 0} };
    for (const auto& version : glesVersions)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, version[0]);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, version[1]);
        m_context = SDL_GL_CreateContext(m_window);
        if (m_context != nullptr)
        {
            ESVersion = version[0] * 10 + version[1];
            Msg("* OpenGL ES: created context version %d.%d", version[0], version[1]);
            break;
        }
        Msg("! OpenGL: could not create OpenGL ES %d.%d context: %s", version[0], version[1], SDL_GetError());
    }
    if (m_context == nullptr)
    {
        Log("! OpenGL: could not create any OpenGL ES drawing context:", SDL_GetError());
#if defined(XR_PLATFORM_ANDROID)
        xrDebug::DoExit("Unable to create an OpenGL ES context after trying versions 3.2, 3.1 and 3.0. "
            "See the OpenGL and RenderBackend logs for the selected libraries and SDL error. "
            "If ANGLE was selected, reopen the launcher and select Native OpenGL ES.");
#else
        return;
#endif
    }

    if (MakeContextCurrent(IRender::PrimaryContext) != 0)
    {
        Log("! OpenGL: could not make context current:", SDL_GetError());
#if defined(XR_PLATFORM_ANDROID)
        xrDebug::DoExit("Unable to make the selected OpenGL ES context current. "
            "Graphics initialization has stopped before creating resources. See the OpenGL and RenderBackend logs. "
            "If ANGLE was selected, reopen the launcher and select Native OpenGL ES.");
#else
        return;
#endif
    }

    int version;
    {
        ZoneScopedN("gladLoadGL");
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
        version = gladLoadGLES2(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress));
#else
        version = gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress));
#endif
    }
#else
    m_context = SDL_GL_CreateContext(m_window);
    if (m_context == nullptr)
    {
        Log("! OpenGL: could not create drawing context:", SDL_GetError());
        return;
    }

    if (MakeContextCurrent(IRender::PrimaryContext) != 0)
    {
        Log("! OpenGL: could not make context current:", SDL_GetError());
        return;
    }

    int version;
    {
        ZoneScopedN("gladLoadGL");
        version = gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress));
    }
#endif
    if (version == 0)
    {
        Log("! OpenGL: could not initialize GLAD.");
        if (const auto err = SDL_GetError())
            Log("SDL Error:", err);
#if defined(XR_PLATFORM_ANDROID)
        xrDebug::DoExit("Unable to load OpenGL ES functions for the selected context through SDL/GLAD. "
            "Graphics initialization has stopped before using the dispatch table. See the OpenGL and RenderBackend logs. "
            "If ANGLE was selected, reopen the launcher and select Native OpenGL ES.");
#else
        return;
#endif
    }

    InitializeVertexBinding();
    if (ThisInstanceIsGlobal())
    {
        UpdateVSync();

        // Every driver-side message routes through this callback, so enabling it
        // in a shipping build costs a branch per message for diagnostics nobody
        // is reading. Debug/Mixed builds keep it for PIX_EVENT frame analysis.
#if defined(DEBUG) || defined(_DEBUG) || XRAY_ENABLE_GL_ERROR_CHECK
        if (glDebugMessageCallback)
        {
            CHK_GL(glEnable(GL_DEBUG_OUTPUT));
            CHK_GL(glDebugMessageCallback((GLDEBUGPROC)OnDebugCallback, nullptr));
        }
#endif
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
        if (AngleAuditEnabled())
        {
            const bool debugSupported = ESVersion >= 32 || HasExtension("GL_KHR_debug");
            if (debugSupported && glDebugMessageCallback)
            {
                glEnable(GL_DEBUG_OUTPUT);
                glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
                glDebugMessageCallback(OnAngleAuditDebugCallback, nullptr);
                Msg("* [gles_angle_audit] bounded synchronous debug callback enabled (diagnostic only)");
            }
            else
                Msg("* [gles_angle_audit] debug callback unavailable; use bounded pass error checks");
        }
#endif
    }

    int iMaxVTFUnits, iMaxCTIUnits;
    glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &iMaxVTFUnits);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &iMaxCTIUnits);

    AdapterName = reinterpret_cast<pcstr>(glGetString(GL_RENDERER));
    OpenGLVersionString = reinterpret_cast<pcstr>(glGetString(GL_VERSION));
    ShadingVersion = reinterpret_cast<pcstr>(glGetString(GL_SHADING_LANGUAGE_VERSION));

    // gl_ClipDistance is not core GLSL ES. It only exists when the driver reports
    // GL_EXT_clip_cull_distance, and the volumetric shaders can only redeclare
    // gl_PerVertex to add it when that is the case. Desktop GL has it in core
    // (GLSL 4.00+), so no extension query is needed there.
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
    ClipCullDistanceSupported = HasExtension("GL_EXT_clip_cull_distance");
#else
    ClipCullDistanceSupported = true;
#endif

    Msg("* [GLES] Active Renderer: [%s]", AdapterName ? AdapterName : "Unknown");
    Msg("* [GLES] Active Vendor: [%s]", glGetString(GL_VENDOR) ? reinterpret_cast<pcstr>(glGetString(GL_VENDOR)) : "Unknown");
#if defined(XR_PLATFORM_ANDROID)
    VerifyAndroidRenderBackend(AdapterName);
#endif
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
    Msg("* [GLES] G-buffer position sampler: %s",
        strstr(Core.Params, "-gles_highp_position") ? "highp (diagnostic)" : "default precision");
    Msg("* [GLES] State rebind diagnostic: %s", CBackend::gles_state_rebind_enabled() ? "on" : "off");
    Msg("* [GLES] Shared physical state cache: on");
    Msg("* [GLES] Vertex binding: %s", UseSeparatedVertexBinding() ? "separated (ES 3.1 core)" : "legacy pointers");
    Msg("* [GLES] Vertex binding at draw: %s", VertexBindingState().eager ? "eager control" : "deferred");
    if (AngleAuditEnabled())
    {
        Msg("* [gles_angle_audit] caps OES_texture_float_linear=%u EXT_color_buffer_float=%u "
            "EXT_color_buffer_half_float=%u EXT_float_blend=%u",
            u32(HasExtension("GL_OES_texture_float_linear")),
            u32(HasExtension("GL_EXT_color_buffer_float")),
            u32(HasExtension("GL_EXT_color_buffer_half_float")),
            u32(HasExtension("GL_EXT_float_blend")));
        const GLenum precisions[] = {GL_LOW_FLOAT, GL_MEDIUM_FLOAT, GL_HIGH_FLOAT};
        for (const auto precision : precisions)
        {
            GLint range[2]{}, bits = 0;
            glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, precision, range, &bits);
            Msg("* [gles_angle_audit] fragment precision=0x%x range=%d,%d bits=%d",
                u32(precision), range[0], range[1], bits);
        }
    }
#endif
    Msg("* GPU vendor: [%s] device: [%s]", glGetString(GL_VENDOR), AdapterName);
    Msg("* GPU OpenGL version: %s", OpenGLVersionString);
    Msg("* GPU OpenGL shading language version: %s", ShadingVersion);
    Msg("* GPU OpenGL VTF units: [%d] CTI units: [%d]", iMaxVTFUnits, iMaxCTIUnits);
    Msg("* GL_EXT_clip_cull_distance (hardware gl_ClipDistance): %s", ClipCullDistanceSupported ? "yes" : "no");

    // The semantics map in shaders/gl/shared/common.h occupies locations 0..15, so
    // every shader that uses TEXCOORD7 needs 16 varying vectors. GLSL ES 3.0 only
    // guarantees 15, and drivers that sit at that minimum cannot link the water
    // and bloom shaders at all.
    GLint maxVaryingVectors = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryingVectors);
    Msg("* GL_MAX_VARYING_VECTORS: %d (16 required for TEXCOORD7)", maxVaryingVectors);
    if (maxVaryingVectors > 0 && maxVaryingVectors < 16)
    {
        Msg("! warning: only %d varying vectors available, water/bloom shaders that use TEXCOORD7 will fail to link",
            maxVaryingVectors);
    }

    ComputeShadersSupported = false; // XXX: Implement compute shaders support

    if (glGenFramebuffers && glBindFramebuffer)
        UpdateViews();
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Initialize();
#endif
}

void CHW::DestroyDevice()
{
#ifdef XR_PLATFORM_ANDROID
    if (ThisInstanceIsGlobal())
        presentGamma.Release(m_context && SDL_GL_GetCurrentContext() == m_context);
#endif
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Shutdown(SDL_GL_GetCurrentContext() == m_context && m_context != nullptr);
#endif
    GLHardwareState::Invalidate();
    GLTextureBindings::Invalidate();
    if (pFB && glDeleteFramebuffers)
    {
        CHK_GL(glDeleteFramebuffers(1, &pFB));
        pFB = 0;
    }

    const auto context = SDL_GL_GetCurrentContext();
    if (context && context == m_context)
        SDL_GL_MakeCurrent(nullptr, nullptr);

    if (m_context)
    {
        SDL_GL_DeleteContext(m_context);
        m_context = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////
// Resetting device
//////////////////////////////////////////////////////////////////////
void CHW::Reset()
{
    ZoneScoped;
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Shutdown(SDL_GL_GetCurrentContext() == m_context && m_context != nullptr);
#endif
    GLHardwareState::Invalidate();
    GLTextureBindings::Invalidate();

    CHK_GL(glDeleteFramebuffers(1, &pFB));
    pFB = 0;
    UpdateViews();

    UpdateVSync();
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Initialize();
#endif
}

void CHW::SetPrimaryAttributes(u32& windowFlags)
{
    windowFlags |= SDL_WINDOW_OPENGL;

#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    if (!strstr(Core.Params, "-no_gl_context"))
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    }
#endif

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
}

IRender::RenderContext CHW::GetCurrentContext() const
{
    const auto context = SDL_GL_GetCurrentContext();
    if (context == m_context)
        return IRender::PrimaryContext;
    return IRender::NoContext;
}

int CHW::MakeContextCurrent(IRender::RenderContext context) const
{
    switch (context)
    {
    case IRender::NoContext:
        return SDL_GL_MakeCurrent(nullptr, nullptr);

    case IRender::PrimaryContext:
        return SDL_GL_MakeCurrent(m_window, m_context);

    default:
        NODEFAULT;
    }
    return -1;
}

void CHW::UpdateViews()
{
    // Create the default framebuffer
    glGenFramebuffers(1, &pFB);
    CHK_GL(glBindFramebuffer(GL_FRAMEBUFFER, pFB));
    GLHardwareState::Get().framebuffer.invalidate();
    GLHardwareState::InvalidateDrawBuffers();
    GLHardwareState::InvalidateAttachments();

    BackBufferCount = 1;
}

void CHW::BeginScene()
{
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::BeginFrame();
#endif
}
void CHW::EndScene() { }

void CHW::Present()
{
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope presentScope(GLGpuProfiler::Stage::PresentBlit);
#endif
    // The engine renders into a dwWidth x dwHeight FBO. When it is smaller
    // than the SDL window (vid_mode below native on Android), stretch it
    // to the full drawable so the picture is not stuck in a corner.
    int w = 0, h = 0;
    SDL_GL_GetDrawableSize(m_window, &w, &h);
    if (!w || !h)
    {
        w = static_cast<int>(Device.dwWidth);
        h = static_cast<int>(Device.dwHeight);
    }
    {
        static bool once = false;
        if (!once)
        {
            once = true;
            Msg("* CHW::Present: FBO %ux%u, window %dx%d", Device.dwWidth, Device.dwHeight, w, h);
        }
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, pFB);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    GLHardwareState::Get().framebuffer.invalidate();
    GLHardwareState::InvalidateDrawBuffers();
    GLHardwareState::InvalidateAttachments();

    // Clear black so borderless letterboxing never shows stale garbage.
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

#ifdef XR_PLATFORM_ANDROID
    if (!presentGamma.Draw(w, h))
#endif
    CHK_GL(glBlitFramebuffer(
        0, 0, Device.dwWidth, Device.dwHeight,
        0, 0, w, h,
        GL_COLOR_BUFFER_BIT, GL_LINEAR));

#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    presentScope.Stop();
    GLGpuProfiler::EndGpuFrame();
#endif
    SDL_GL_SwapWindow(m_window);
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::FinishFrame();
#endif
    CurrentBackBuffer = (CurrentBackBuffer + 1) % BackBufferCount;
}

DeviceState CHW::GetDeviceState() const
{
    //  TODO: OGL: Implement GetDeviceState
    return DeviceState::Normal;
}

std::pair<u32, u32> CHW::GetSurfaceSize()
{
    return
    {
        psDeviceMode.Width,
        psDeviceMode.Height
    };
}

bool CHW::ThisInstanceIsGlobal() const
{
    return this == &HW;
}

void CHW::BeginPixEvent(pcstr name) const
{
    if (glPushDebugGroup)
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
}

void CHW::EndPixEvent() const
{
    if (glPushDebugGroup)
        glPopDebugGroup();
}
} // namespace xray::render::RENDER_NAMESPACE
