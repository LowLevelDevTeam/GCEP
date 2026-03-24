#pragma once
// Private physics debug renderer declarations — included only inside VulkanRHI.
// Two pipelines share one pipeline layout:
//   - eLineList  : wireframe colliders, contact normals, velocity vectors
//   - eTriangleList : solid/transparent shape fills

// ─── Methods ──────────────────────────────────────────────────────────────────
void createPhysicsDebugPipelines();
void initPhysicsDebugBuffers();
void recordPhysicsDebugPass();

// ─── Pipelines ────────────────────────────────────────────────────────────────
/// Shared layout: single push constant (DebugPushConstants = 64 bytes, vertex stage only).
vk::raii::PipelineLayout m_debugPipelineLayout = nullptr;

/// eLineList — wireframe edges, contacts, velocities
vk::raii::Pipeline m_debugLinePipeline = nullptr;

/// eTriangleList — solid / translucent shape fills
vk::raii::Pipeline m_debugTriPipeline = nullptr;

// ─── Per-frame double-buffered vertex buffers ─────────────────────────────────
// host-visible + host-coherent so the CPU can write every frame without staging.
// Double-buffered (one slot per frame-in-flight) so the GPU can read slot N-1
// while the CPU writes slot N. The existing fence in VulkanFrameContext guarantees
// the GPU is done before the CPU rewrites.

static constexpr vk::DeviceSize kDebugLineBufferSize = 2 * 1024 * 1024; // 2 MB → ~131 K vertices
static constexpr vk::DeviceSize kDebugTriBufferSize  = 4 * 1024 * 1024; // 4 MB → ~262 K vertices

struct DebugFrameBuffers
{
    vk::raii::Buffer       lineBuffer = nullptr;
    vk::raii::DeviceMemory lineMem    = nullptr;
    void*                  lineMapped = nullptr;

    vk::raii::Buffer       triBuffer  = nullptr;
    vk::raii::DeviceMemory triMem     = nullptr;
    void*                  triMapped  = nullptr;
};

std::array<DebugFrameBuffers, MAX_FRAMES_IN_FLIGHT> m_debugFrameBuffers;

/// Number of line vertices written into the current frame's line buffer.
uint32_t m_debugLineVertCount = 0;
/// Number of triangle vertices written into the current frame's triangle buffer.
uint32_t m_debugTriVertCount  = 0;
/// Toggles 0 ↔ 1 each frame (m_debugFrameSlot ^= 1 in flushPhysicsDebugData).
uint32_t m_debugFrameSlot     = 0;
