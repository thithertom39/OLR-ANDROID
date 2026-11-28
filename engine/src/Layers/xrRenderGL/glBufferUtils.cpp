#include "stdafx.h"
#include "Layers/xrRender/BufferUtils.h"
#include "glBackendStateCache.h"
#include "glStreamSync.h"
#include "glVertexBinding.h"
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
#include "glGpuProfiler.h"
#endif

#include <FlexibleVertexFormat.h>

namespace xray::render::RENDER_NAMESPACE
{
enum
{
    LOCKFLAGS_FLUSH  = GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_BUFFER_BIT,
    LOCKFLAGS_APPEND = GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT, // TODO: Implement buffer object appending using glBufferSubData
};

u32 GetFVFVertexSize(u32 FVF)
{
    return static_cast<u32>(::FVF::ComputeVertexSize(FVF));
}

u32 GetDeclVertexSize(const VertexElement* decl, u32 Stream)
{
    return static_cast<u32>(::FVF::ComputeVertexSize(decl, Stream));
}

u32 GetDeclLength(const VertexElement* decl)
{
    return static_cast<u32>(::FVF::GetDeclLength(decl));
}

static HRESULT CreateBuffer(GLuint* pBuffer, const void* pData, u32 dataSize, bool bDynamic, bool bIndexBuffer)
{
    const GLenum usage = bDynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
    // Uploading an index buffer must not replace the current VAO's EBO.
    // Buffer storage is independent of the target used to create it.
    const GLenum target = bIndexBuffer ? GL_COPY_WRITE_BUFFER : GL_ARRAY_BUFFER;

    glGenBuffers(1, pBuffer);
    glBindBuffer(target, *pBuffer);
    CHK_GL(glBufferData(target, dataSize, pData, usage));
    return S_OK;
}

static inline HRESULT CreateVertexBuffer(VertexBufferHandle* pBuffer, const void* pData, u32 dataSize, bool bDynamic)
{
    return CreateBuffer(pBuffer, pData, dataSize, bDynamic, false);
}

static inline HRESULT CreateIndexBuffer(IndexBufferHandle* pBuffer, const void* pData, u32 dataSize, bool bDynamic)
{
    return CreateBuffer(static_cast<GLuint*>(pBuffer), pData, dataSize, bDynamic, true);
}

const GLsizei VertexSizeList[] =
{
    1, // D3DDECLTYPE_FLOAT1
    2, // D3DDECLTYPE_FLOAT2
    3, // D3DDECLTYPE_FLOAT3
    4, // D3DDECLTYPE_FLOAT4
    4, // D3DDECLTYPE_D3DCOLOR
    4, // D3DDECLTYPE_UBYTE4
    2, // D3DDECLTYPE_SHORT2
    4, // D3DDECLTYPE_SHORT4
    4, // D3DDECLTYPE_UBYTE4N
    2, // D3DDECLTYPE_SHORT2N
    4, // D3DDECLTYPE_SHORT4N
    2, // D3DDECLTYPE_USHORT2N
    4, // D3DDECLTYPE_USHORT4N
    4, // D3DDECLTYPE_UDEC3
    4, // D3DDECLTYPE_DEC3N
    2, // D3DDECLTYPE_FLOAT16_2
    4 // D3DDECLTYPE_FLOAT16_4
};

const GLenum VertexTypeList[] =
{
    GL_FLOAT, // D3DDECLTYPE_FLOAT1
    GL_FLOAT, // D3DDECLTYPE_FLOAT2
    GL_FLOAT, // D3DDECLTYPE_FLOAT3
    GL_FLOAT, // D3DDECLTYPE_FLOAT4
    GL_UNSIGNED_BYTE, // D3DDECLTYPE_D3DCOLOR
    GL_UNSIGNED_BYTE, // D3DDECLTYPE_UBYTE4
    GL_SHORT, // D3DDECLTYPE_SHORT2
    GL_SHORT, // D3DDECLTYPE_SHORT4
    GL_UNSIGNED_BYTE, // D3DDECLTYPE_UBYTE4N
    GL_SHORT, // D3DDECLTYPE_SHORT2N
    GL_SHORT, // D3DDECLTYPE_SHORT4N
    GL_UNSIGNED_SHORT, // D3DDECLTYPE_USHORT2N
    GL_UNSIGNED_SHORT, // D3DDECLTYPE_USHORT4N
    GL_UNSIGNED_INT_2_10_10_10_REV, // D3DDECLTYPE_UDEC3
    GL_INT_2_10_10_10_REV, // D3DDECLTYPE_DEC3N
    GL_HALF_FLOAT, // D3DDECLTYPE_FLOAT16_2
    GL_HALF_FLOAT // D3DDECLTYPE_FLOAT16_4
};

const GLboolean VertexNormalizedList[] =
{
    GL_FALSE, // D3DDECLTYPE_FLOAT1
    GL_FALSE, // D3DDECLTYPE_FLOAT2
    GL_FALSE, // D3DDECLTYPE_FLOAT3
    GL_FALSE, // D3DDECLTYPE_FLOAT4
    GL_TRUE, // D3DDECLTYPE_D3DCOLOR
    GL_FALSE, // D3DDECLTYPE_UBYTE4
    GL_FALSE, // D3DDECLTYPE_SHORT2
    GL_FALSE, // D3DDECLTYPE_SHORT4
    GL_TRUE, // D3DDECLTYPE_UBYTE4N
    GL_TRUE, // D3DDECLTYPE_SHORT2N
    GL_TRUE, // D3DDECLTYPE_SHORT4N
    GL_TRUE, // D3DDECLTYPE_USHORT2N
    GL_TRUE, // D3DDECLTYPE_USHORT4N
    GL_FALSE, // D3DDECLTYPE_UDEC3
    GL_TRUE, // D3DDECLTYPE_DEC3N
    GL_FALSE, // D3DDECLTYPE_FLOAT16_2
    GL_FALSE // D3DDECLTYPE_FLOAT16_4
};

const GLuint VertexUsageList[] =
{
    3, // D3DDECLUSAGE_POSITION
    ~0u, // D3DDECLUSAGE_BLENDWEIGHT
    ~0u, // D3DDECLUSAGE_BLENDINDICES
    5, // D3DDECLUSAGE_NORMAL
    ~0u, // D3DDECLUSAGE_PSIZE
    8, // D3DDECLUSAGE_TEXCOORD
    4, // D3DDECLUSAGE_TANGENT
    6, // D3DDECLUSAGE_BINORMAL
    ~0u, // D3DDECLUSAGE_TESSFACTOR
    3, // D3DDECLUSAGE_POSITIONT
    0, // D3DDECLUSAGE_COLOR
    7, // D3DDECLUSAGE_FOG
    ~0u, // D3DDECLUSAGE_DEPTH
    ~0u, // D3DDECLUSAGE_SAMPLE
};

template <typename F>
void IterVertexDeclaration(const VertexElement* dxdecl, F&& callback)
{
    // XXX: tamlin: use 'stride', or drop it.
    // GLsizei stride = GetDeclVertexSize(dxdecl, 0);
    for (int i = 0; i < MAXD3DDECLLENGTH; ++i)
    {
        const D3DVERTEXELEMENT9& desc = dxdecl[i];

        if (desc.Stream == 0xFF)
            break;

        GLuint location = VertexUsageList[desc.Usage];
        GLint size = VertexSizeList[desc.Type];
        GLenum type = VertexTypeList[desc.Type];
        GLboolean normalized = VertexNormalizedList[desc.Type];

        if (location == GLuint(~0u))
            continue; // Unsupported

        location += desc.UsageIndex;
        callback(location, size, type, normalized, desc.Offset, desc.Stream);
    }
}

void SetVertexDeclaration(const VertexElement* dxdecl, size_t base_offset = 0)
{
    auto stride = GetDeclVertexSize(dxdecl, 0);
    IterVertexDeclaration(dxdecl,
    [&](GLuint location, GLint size, GLenum type, GLboolean normalized, intptr_t offset, GLuint /*stream*/)
    {
        CHK_GL(glVertexAttribPointer(
            location, size, type, normalized, stride, (void*)(offset + base_offset)));
    });
}

void ConvertVertexDeclaration(const VertexElement* dxdecl, SDeclaration* decl)
{
    RCache.set_Format(decl);
    IterVertexDeclaration(dxdecl,
    [](GLuint location, GLint size, GLenum type, GLboolean normalized, GLuint offset, GLuint stream)
    {
        CHK_GL(glEnableVertexAttribArray(location));
        if (UseSeparatedVertexBinding())
        {
            CHK_GL(glVertexAttribFormat(location, size, type, normalized, offset));
            CHK_GL(glVertexAttribBinding(location, stream));
        }
    });
}

void SetGLVertexPointer(SDeclaration* decl, size_t base_offset)
{
    if (!decl)
        return;
    SetVertexDeclaration(decl->dcl_code.data(), base_offset);
}

//-----------------------------------------------------------------------------
VertexStagingBuffer::~VertexStagingBuffer()
{
    Destroy();
}

void VertexStagingBuffer::Create(size_t size, bool allowReadBack /*= false*/)
{
    m_Size = size;
    m_AllowReadBack = allowReadBack;

    m_HostBuffer = xr_alloc<u8>(size);
    AddRef();
}

bool VertexStagingBuffer::IsValid() const
{
    return !!m_DeviceBuffer;
}

void* VertexStagingBuffer::Map(
    size_t offset /*= 0*/,
    size_t size /*= 0*/,
    bool read /*= false*/)
{
    VERIFY2(m_HostBuffer, "Buffer wasn't created or already discarded");
    VERIFY2(!read || m_AllowReadBack, "Can't read from write only buffer");
    VERIFY2((size + offset) <= m_Size, "Map region is too large");

    return static_cast<u8*>(m_HostBuffer) + offset;
}

void VertexStagingBuffer::Unmap(bool doFlush /*= false*/)
{
    if (!doFlush)
    {
        /* Do nothing*/
        return;
    }

    VERIFY2(!m_DeviceBuffer, "Attempting to upload buffer twice");
    VERIFY(m_HostBuffer && m_Size);

    // Upload data to the device
    CreateVertexBuffer(&m_DeviceBuffer, m_HostBuffer, m_Size, false);

    if (!m_AllowReadBack)
    {
        // Cache buffer isn't required anymore. Free host memory
        DiscardHostBuffer();
    }
}

VertexBufferHandle VertexStagingBuffer::GetBufferHandle() const
{
    return m_DeviceBuffer;
}

void VertexStagingBuffer::Destroy()
{
    DiscardHostBuffer();
    m_Size = 0;

    if (m_DeviceBuffer)
    {
        glDeleteBuffers(1, &m_DeviceBuffer);
        GLHardwareState::InvalidateGeometry();
        m_DeviceBuffer = 0;
    }
}

void VertexStagingBuffer::DiscardHostBuffer()
{
    if (m_HostBuffer)
        xr_free(m_HostBuffer);
}

size_t VertexStagingBuffer::GetSystemMemoryUsage() const
{
    return m_HostBuffer ? m_Size : 0;
}

size_t VertexStagingBuffer::GetVideoMemoryUsage() const
{
    if (!m_DeviceBuffer)
        return 0;

    GLint bufferSize;
    glBindBuffer(GL_ARRAY_BUFFER, m_DeviceBuffer);
    CHK_GL(glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bufferSize));
    return bufferSize;
}

//-----------------------------------------------------------------------------
IndexStagingBuffer::~IndexStagingBuffer()
{
    Destroy();
}

void IndexStagingBuffer::Create(size_t size, bool allowReadBack /*= false*/, bool /*managed = true*/)
{
    m_Size = size;
    m_AllowReadBack = allowReadBack;

    m_HostBuffer = xr_alloc<u8>(size);
    AddRef();
}

bool IndexStagingBuffer::IsValid() const
{
    return !!m_DeviceBuffer;
}

void* IndexStagingBuffer::Map(
    size_t offset /*= 0*/,
    size_t size /*= 0*/,
    bool read /*= false*/)
{
    VERIFY2(m_HostBuffer, "Buffer wasn't created or already discarded");
    VERIFY2(!read || m_AllowReadBack, "Can't read from write only buffer");
    VERIFY2((size + offset) <= m_Size, "Map region is too large");

    return static_cast<u8*>(m_HostBuffer) + offset;
}

void IndexStagingBuffer::Unmap(bool doFlush /*= false*/)
{
    if (!doFlush)
    {
        /* Do nothing*/
        return;
    }

    VERIFY2(!m_DeviceBuffer, "Attempting to upload buffer twice");
    VERIFY(m_HostBuffer && m_Size);

    // Upload data to the device
    CreateIndexBuffer(&m_DeviceBuffer, m_HostBuffer, m_Size, false);

    if (!m_AllowReadBack)
    {
        // Cache buffer isn't required anymore. Free host memory
        DiscardHostBuffer();
    }
}

IndexBufferHandle IndexStagingBuffer::GetBufferHandle() const
{
    return m_DeviceBuffer;
}

void IndexStagingBuffer::Destroy()
{
#if defined(XR_PLATFORM_ANDROID)
    m_StaticBatchSource = false;
#endif
    DiscardHostBuffer();
    m_Size = 0;

    if (m_DeviceBuffer)
    {
        glDeleteBuffers(1, &m_DeviceBuffer);
        GLHardwareState::InvalidateGeometry();
        m_DeviceBuffer = 0;
    }
}

void IndexStagingBuffer::DiscardHostBuffer()
{
    if (m_HostBuffer)
        xr_free(m_HostBuffer);
}

size_t IndexStagingBuffer::GetSystemMemoryUsage() const
{
    return m_HostBuffer ? m_Size : 0;
}

size_t IndexStagingBuffer::GetVideoMemoryUsage() const
{
    if (!m_DeviceBuffer)
        return 0;

    GLint bufferSize;
    glBindBuffer(GL_COPY_WRITE_BUFFER, m_DeviceBuffer);
    CHK_GL(glGetBufferParameteriv(GL_COPY_WRITE_BUFFER, GL_BUFFER_SIZE, &bufferSize));
    return bufferSize;
}

//-----------------------------------------------------------------------------
VertexStreamBuffer::~VertexStreamBuffer()
{
    Destroy();
}

void VertexStreamBuffer::Create(size_t size)
{
    if (GLStreamSync::UploadEnabled())
    {
        CHECK_OR_EXIT(!m_DeviceBuffer && !m_UploadBuffer && size > 0 && size <= static_cast<size_t>(~u32{0}),
            "Invalid vertex upload stream creation");
        m_UploadCapacity = size;
        m_UploadBuffer = xr_alloc<u8>(size);
        CHECK_OR_EXIT(m_UploadBuffer != nullptr, "Could not allocate vertex upload staging memory");
    }
#if defined(XR_PLATFORM_ANDROID)
    if (!(GLStreamSync::UploadEnabled() && GLStreamSync::HasFlag("-olr") &&
        GLStreamSync::HasFlag("-gles_fenced_vertex_stream") && !CBackend::gles_state_rebind_enabled() &&
        m_FencedVertexStream.Create(m_DeviceBuffer, size)))
#endif
        CreateVertexBuffer(&m_DeviceBuffer, nullptr, size, true);
    if (GLStreamSync::UploadEnabled())
        GLStreamSync::CheckUploadCreation(m_DeviceBuffer, size);
    AddRef();
}

void VertexStreamBuffer::Destroy()
{
    if (m_UploadBuffer)
        xr_free(m_UploadBuffer);
    m_UploadCapacity = m_UploadOffset = m_UploadSize = 0;
    m_UploadMapped = false;
    if (m_DeviceBuffer == 0)
        return;

#if defined(XR_PLATFORM_ANDROID)
    if (m_FencedVertexStream.Release(m_DeviceBuffer))
#endif
        glDeleteBuffers(1, &m_DeviceBuffer);
    GLHardwareState::InvalidateGeometry();
    m_DeviceBuffer = 0;
}

void* VertexStreamBuffer::Map(size_t offset, size_t size, bool flush /*= false*/)
{
    if (GLStreamSync::UploadEnabled())
    {
        CHECK_OR_EXIT(m_DeviceBuffer && m_UploadBuffer && !m_UploadMapped && size > 0 &&
            offset <= m_UploadCapacity && size <= m_UploadCapacity - offset,
            "Invalid or nested vertex upload stream mapping");
        if (flush)
        {
#if defined(XR_PLATFORM_ANDROID)
            if (m_FencedVertexStream.Enabled())
            {
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                GLGpuProfiler::Scope discard(GLGpuProfiler::Stage::VertexRingDiscard, false);
#endif
                m_FencedVertexStream.Discard(m_DeviceBuffer);
                if (Device.dwFrame % 67 == 0)
                    Msg("* [gles_fenced_vertex_stream] frame=%u mapped_discards=%u fallback_discards=%u",
                        Device.dwFrame, m_FencedVertexStream.mappedDiscards, m_FencedVertexStream.fallbackDiscards);
            }
            else
#endif
            {
                glBindBuffer(GL_ARRAY_BUFFER, m_DeviceBuffer);
                GLStreamSync::ReplaceStorage(GL_ARRAY_BUFFER, m_UploadCapacity, offset, size);
            }
        }
        m_UploadOffset = offset;
        m_UploadSize = size;
        m_UploadMapped = true;
#if defined(XR_PLATFORM_ANDROID)
        if (m_FencedVertexStream.MappedSlot())
            return m_FencedVertexStream.Address(offset);
#endif
        return static_cast<u8*>(m_UploadBuffer) + offset;
    }
    VERIFY(m_DeviceBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_DeviceBuffer);

    void *pData = nullptr;
    const bool orphaned = GLStreamSync::OrphanOnFlush(GL_ARRAY_BUFFER, flush, offset, size);
    // Without a global frame fence, the driver must protect every mapped
    // range from pending GPU reads, including appends to the current store.
    const auto flags = GLStreamSync::OrphaningEnabled() ? GL_MAP_WRITE_BIT :
        (flush && !orphaned ? LOCKFLAGS_FLUSH : LOCKFLAGS_APPEND);
    CHK_GL(pData = (void*)glMapBufferRange(
        GL_ARRAY_BUFFER,
        offset,
        size,
        flags));

    GLStreamSync::CheckMap(pData, GL_ARRAY_BUFFER, m_DeviceBuffer, offset, size);
    return pData;
}

void VertexStreamBuffer::Unmap()
{
    Unmap(static_cast<size_t>(-1));
}

void VertexStreamBuffer::Unmap(size_t usedBytes)
{
    if (GLStreamSync::UploadEnabled())
    {
        CHECK_OR_EXIT(m_DeviceBuffer && m_UploadBuffer && m_UploadMapped,
            "Vertex upload stream Unmap without Map");
        if (usedBytes == static_cast<size_t>(-1))
            usedBytes = m_UploadSize;
        if (usedBytes > m_UploadSize)
            Msg("[vertex_upload_audit] stream=%p buffer=%u used_bytes=%zu reserved_bytes=%zu offset=%zu capacity=%zu mapped=%u",
                static_cast<void*>(this), unsigned(m_DeviceBuffer), usedBytes, m_UploadSize,
                m_UploadOffset, m_UploadCapacity, unsigned(m_UploadMapped));
        CHECK_OR_EXIT(usedBytes <= m_UploadSize, "Vertex upload exceeds mapped reservation");
        if (usedBytes
#if defined(XR_PLATFORM_ANDROID)
            && !m_FencedVertexStream.MappedSlot()
#endif
            )
        {
            glBindBuffer(GL_ARRAY_BUFFER, m_DeviceBuffer);
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
            GLGpuProfiler::Scope upload(GLGpuProfiler::Stage::SharedStreamUpload, false);
#endif
            GLStreamSync::UploadRange(GL_ARRAY_BUFFER, m_DeviceBuffer, m_UploadOffset, usedBytes,
                static_cast<u8*>(m_UploadBuffer) + m_UploadOffset);
        }
        m_UploadMapped = false;
        return;
    }
    VERIFY(m_DeviceBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_DeviceBuffer);
    GLboolean result = GL_FALSE;
    CHK_GL(result = glUnmapBuffer(GL_ARRAY_BUFFER));
    GLStreamSync::CheckUnmap(result, GL_ARRAY_BUFFER, m_DeviceBuffer);
}

bool VertexStreamBuffer::IsValid() const
{
    return !!m_DeviceBuffer;
}

//-----------------------------------------------------------------------------
IndexStreamBuffer::~IndexStreamBuffer()
{
    Destroy();
}

void IndexStreamBuffer::Create(size_t size)
{
    if (GLStreamSync::UploadEnabled())
    {
        CHECK_OR_EXIT(!m_DeviceBuffer && !m_UploadBuffer && size > 0 && size <= static_cast<size_t>(~u32{0}),
            "Invalid index upload stream creation");
        m_UploadCapacity = size;
        m_UploadBuffer = xr_alloc<u8>(size);
        CHECK_OR_EXIT(m_UploadBuffer != nullptr, "Could not allocate index upload staging memory");
    }
    CreateIndexBuffer(&m_DeviceBuffer, nullptr, size, true);
    if (GLStreamSync::UploadEnabled())
        GLStreamSync::CheckUploadCreation(m_DeviceBuffer, size);
    AddRef();
}

void IndexStreamBuffer::Destroy()
{
    if (m_UploadBuffer)
        xr_free(m_UploadBuffer);
    m_UploadCapacity = m_UploadOffset = m_UploadSize = 0;
    m_UploadMapped = false;
    if (m_DeviceBuffer == 0)
        return;

    glDeleteBuffers(1, &m_DeviceBuffer);
    GLHardwareState::InvalidateGeometry();
    m_DeviceBuffer = 0;
}

void* IndexStreamBuffer::Map(size_t offset, size_t size, bool flush /*= false*/)
{
    if (GLStreamSync::UploadEnabled())
    {
        CHECK_OR_EXIT(m_DeviceBuffer && m_UploadBuffer && !m_UploadMapped && size > 0 &&
            offset <= m_UploadCapacity && size <= m_UploadCapacity - offset,
            "Invalid or nested index upload stream mapping");
        if (flush)
        {
            glBindBuffer(GL_COPY_WRITE_BUFFER, m_DeviceBuffer);
            GLStreamSync::ReplaceStorage(GL_COPY_WRITE_BUFFER, m_UploadCapacity, offset, size);
        }
        m_UploadOffset = offset;
        m_UploadSize = size;
        m_UploadMapped = true;
        return static_cast<u8*>(m_UploadBuffer) + offset;
    }
    VERIFY(m_DeviceBuffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, m_DeviceBuffer);

    void *pData = nullptr;
    const bool orphaned = GLStreamSync::OrphanOnFlush(GL_COPY_WRITE_BUFFER, flush, offset, size);
    const auto flags = GLStreamSync::OrphaningEnabled() ? GL_MAP_WRITE_BIT :
        (flush && !orphaned ? LOCKFLAGS_FLUSH : LOCKFLAGS_APPEND);
    CHK_GL(pData = (void*)glMapBufferRange(
        GL_COPY_WRITE_BUFFER,
        offset,
        size,
        flags));

    GLStreamSync::CheckMap(pData, GL_COPY_WRITE_BUFFER, m_DeviceBuffer, offset, size);
    return pData;
}

void IndexStreamBuffer::Unmap()
{
    Unmap(static_cast<size_t>(-1));
}

void IndexStreamBuffer::Unmap(size_t usedBytes)
{
    if (GLStreamSync::UploadEnabled())
    {
        CHECK_OR_EXIT(m_DeviceBuffer && m_UploadBuffer && m_UploadMapped,
            "Index upload stream Unmap without Map");
        if (usedBytes == static_cast<size_t>(-1))
            usedBytes = m_UploadSize;
        CHECK_OR_EXIT(usedBytes <= m_UploadSize, "Index upload exceeds mapped reservation");
        if (usedBytes)
        {
            glBindBuffer(GL_COPY_WRITE_BUFFER, m_DeviceBuffer);
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
            GLGpuProfiler::Scope upload(GLGpuProfiler::Stage::SharedStreamUpload, false);
            GLGpuProfiler::Scope indexUpload(GLGpuProfiler::Stage::SharedIndexUpload, false);
#endif
            GLStreamSync::UploadRange(GL_COPY_WRITE_BUFFER, m_DeviceBuffer, m_UploadOffset, usedBytes,
                static_cast<u8*>(m_UploadBuffer) + m_UploadOffset);
        }
        m_UploadMapped = false;
        return;
    }
    VERIFY(m_DeviceBuffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, m_DeviceBuffer);
    GLboolean result = GL_FALSE;
    CHK_GL(result = glUnmapBuffer(GL_COPY_WRITE_BUFFER));
    GLStreamSync::CheckUnmap(result, GL_COPY_WRITE_BUFFER, m_DeviceBuffer);
}

bool IndexStreamBuffer::IsValid() const
{
    return !!m_DeviceBuffer;
}
} // namespace xray::render::RENDER_NAMESPACE
