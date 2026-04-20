#pragma once

#include "ergo/gpu_particle/types.h"

#include <cstddef>
#include <cstdint>

namespace ergo::gpu_particle {

// ---------------------------------------------------------------------------
// GPU backend abstraction.
//
// Ergo modules don't link against a specific graphics API; instead the
// ParticleSystem is handed an `IGpuBackend` implementation by the host
// (e.g. a Vulkan adapter living in the Pictor integration layer, or a
// WebGPU adapter for the WebGL build). The interface is intentionally
// minimal — only buffer / 1D-texture / compute-shader operations the
// particle pipeline actually needs.
// ---------------------------------------------------------------------------

struct BufferHandle   { uint64_t id = 0; explicit operator bool() const { return id != 0; } };
struct TextureHandle  { uint64_t id = 0; explicit operator bool() const { return id != 0; } };
struct ShaderHandle   { uint64_t id = 0; explicit operator bool() const { return id != 0; } };

enum class BufferUsage : uint8_t {
    Storage   = 0,   // SSBO — read/write from compute
    Uniform   = 1,   // UBO
    Indirect  = 2,   // draw/dispatch indirect
    Staging   = 3,   // host-visible upload scratch
};

enum class TextureFormat : uint8_t {
    R8       = 0,
    RGBA8    = 1,
    R16F     = 2,
    RGBA16F  = 3,
    R32F     = 4,
    RGBA32F  = 5,
};

enum class ResourceKind : uint8_t {
    StorageBuffer     = 0,
    UniformBuffer     = 1,
    SampledTexture1D  = 2,
    StorageTexture1D  = 3,
};

struct ResourceBinding {
    uint32_t      binding_index = 0;   // matches GLSL `layout(binding = N)`
    ResourceKind  kind          = ResourceKind::StorageBuffer;
    BufferHandle  buffer;
    TextureHandle texture;
    size_t        offset        = 0;
    size_t        range         = 0;   // 0 = whole buffer
};

struct DispatchRange { uint32_t x = 1, y = 1, z = 1; };

// ---------------------------------------------------------------------------
// IGpuBackend — implemented by the integrating application.
//
// The particle pipeline uses the backend in a strictly single-threaded
// fashion from the thread that calls `ParticleSystem::update()`. All
// handles must remain stable until the owning resource is explicitly
// destroyed.
// ---------------------------------------------------------------------------

class IGpuBackend {
public:
    virtual ~IGpuBackend() = default;

    // -- Buffers ---------------------------------------------------------
    virtual BufferHandle create_buffer(size_t bytes, BufferUsage usage,
                                       const char* debug_name = nullptr) = 0;
    virtual void         destroy_buffer(BufferHandle) = 0;
    virtual void         upload_buffer(BufferHandle, size_t offset,
                                       const void* data, size_t bytes) = 0;
    virtual void         zero_buffer(BufferHandle) = 0;
    /// Read back a buffer range (blocking). Used sparingly (e.g. to
    /// fetch `ParticleCounters::draw_count` for indirect draw fallback).
    virtual void         readback_buffer(BufferHandle, size_t offset,
                                         void* out, size_t bytes) = 0;

    // -- Textures --------------------------------------------------------
    virtual TextureHandle create_texture_1d(uint32_t width, TextureFormat fmt,
                                            const char* debug_name = nullptr) = 0;
    virtual void          upload_texture_1d(TextureHandle, const void* data,
                                            size_t bytes) = 0;
    virtual void          destroy_texture(TextureHandle) = 0;

    // -- Shaders ---------------------------------------------------------
    virtual ShaderHandle  create_compute_shader(const uint32_t* spirv,
                                                size_t spirv_size_bytes,
                                                const char* entry_point = "main",
                                                const char* debug_name = nullptr) = 0;
    virtual void          destroy_shader(ShaderHandle) = 0;

    // -- Dispatch + sync -------------------------------------------------
    virtual void dispatch_compute(ShaderHandle                  shader,
                                  DispatchRange                 groups,
                                  const ResourceBinding*        bindings,
                                  uint32_t                      binding_count) = 0;
    /// Insert a memory barrier so a compute-write on `buffer` is visible
    /// to the next compute-read in the same submission.
    virtual void barrier_buffer(BufferHandle buffer) = 0;
};

} // namespace ergo::gpu_particle
